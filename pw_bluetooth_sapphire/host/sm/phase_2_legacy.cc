// Copyright 2020 the Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "phase_2_legacy.h"

#include <zircon/assert.h>

#include <optional>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/random.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uint128.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/active_phase.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/packet.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {

namespace sm {
namespace {
// We do not support OOB pairing & Legacy pairing does not permit Numeric Comparison.
bool IsSupportedLegacyMethod(PairingMethod method) {
  return (method == PairingMethod::kJustWorks || method == PairingMethod::kPasskeyEntryDisplay ||
          method == PairingMethod::kPasskeyEntryInput);
}
}  // namespace

Phase2Legacy::Phase2Legacy(fxl::WeakPtr<PairingChannel> chan, fxl::WeakPtr<Listener> listener,
                           hci::Connection::Role role, PairingFeatures features,
                           const ByteBuffer& preq, const ByteBuffer& pres,
                           const DeviceAddress& initiator_add, const DeviceAddress& responder_add,
                           OnPhase2KeyGeneratedCallback cb)
    : ActivePhase(std::move(chan), std::move(listener), role),
      sent_local_confirm_(false),
      sent_local_rand_(false),
      tk_(std::nullopt),
      local_confirm_(std::nullopt),
      peer_confirm_(std::nullopt),
      local_rand_(std::nullopt),
      peer_rand_(std::nullopt),
      features_(features),
      initiator_addr_(initiator_add),
      responder_addr_(responder_add),
      on_stk_ready_(std::move(cb)),
      weak_ptr_factory_(this) {
  // Cache |preq| and |pres|. These are used for confirm value generation.
  ZX_ASSERT(preq.size() == preq_.size());
  ZX_ASSERT(pres.size() == pres_.size());
  ZX_ASSERT_MSG(IsSupportedLegacyMethod(features.method), "unsupported legacy pairing method!");
  preq.Copy(&preq_);
  pres.Copy(&pres_);
  sm_chan().SetChannelHandler(weak_ptr_factory_.GetWeakPtr());
}

void Phase2Legacy::Start() {
  ZX_ASSERT(!has_failed());
  ZX_ASSERT(!features_.secure_connections);
  ZX_ASSERT(!tk_.has_value());
  ZX_ASSERT(!peer_confirm_.has_value());
  ZX_ASSERT(!peer_rand_.has_value());
  ZX_ASSERT(!sent_local_confirm_);
  ZX_ASSERT(!sent_local_rand_);
  MakeTemporaryKeyRequest();
}

void Phase2Legacy::MakeTemporaryKeyRequest() {
  bt_log(TRACE, "sm", "TK request - method: %s",
         sm::util::PairingMethodToString(features_.method).c_str());
  ZX_ASSERT(listener());
  auto self = weak_ptr_factory_.GetWeakPtr();
  if (features_.method == sm::PairingMethod::kPasskeyEntryInput) {
    // The TK will be provided by the user.
    listener()->RequestPasskey([self](int64_t passkey) {
      if (!self) {
        return;
      }
      bool success = passkey >= 0;
      self->HandleTemporaryKey(success ? std::optional<uint32_t>(passkey) : std::nullopt);
    });
    return;
  }

  if (features_.method == sm::PairingMethod::kPasskeyEntryDisplay) {
    // Randomly generate a 6 digit passkey.
    // TODO(50003): Fix modulo biasing of random passkey.
    uint32_t passkey;
    zx_cprng_draw(&passkey, sizeof(passkey));
    passkey = passkey % 1000000;
    listener()->DisplayPasskey(
        passkey, false /* is_numeric_comparison */, [passkey, self](bool confirm) {
          if (!self) {
            return;
          }
          self->HandleTemporaryKey(confirm ? std::optional<uint32_t>(passkey) : std::nullopt);
        });
    return;
  }

  // TODO(fxb/601): Support providing a TK out of band.
  ZX_ASSERT(features_.method == sm::PairingMethod::kJustWorks);
  listener()->ConfirmPairing([self](bool confirm) {
    if (!self) {
      return;
    }
    self->HandleTemporaryKey(confirm ? std::optional<uint32_t>(0) : std::nullopt);
  });
}

void Phase2Legacy::HandleTemporaryKey(std::optional<uint32_t> maybe_tk) {
  if (!maybe_tk.has_value()) {
    bt_log(TRACE, "sm", "TK listener() responded with error; aborting");
    if (features_.method == PairingMethod::kPasskeyEntryInput) {
      Abort(ErrorCode::kPasskeyEntryFailed);
    } else {
      Abort(ErrorCode::kUnspecifiedReason);
    }
    return;
  }
  uint32_t tk = *maybe_tk;
  tk_ = UInt128{0};
  // Set the lower bits to |tk|.
  tk = htole32(tk);
  std::memcpy(tk_.value().data(), &tk, sizeof(tk));

  // We have TK so we can generate the confirm value now.
  local_rand_ = Random<UInt128>();
  local_confirm_ = UInt128();
  util::C1(tk_.value(), local_rand_.value(), preq_, pres_, initiator_addr_, responder_addr_,
           &(local_confirm_.value()));

  // If we are the initiator then we just generated the "Mconfirm" value. We start the exchange by
  // sending this value to the peer. Otherwise this is the "Sconfirm" value and we either:
  //    a. send it now if the peer has sent us its confirm value while we were
  //    waiting for the TK.
  //    b. send it later when we receive Mconfirm.
  if (is_initiator() || peer_confirm_.has_value()) {
    SendConfirmValue();
  }
}

void Phase2Legacy::SendConfirmValue() {
  ZX_ASSERT(!sent_local_confirm_);
  ZX_ASSERT(local_confirm_.has_value());
  // Only allowed on the LE transport.
  if (sm_chan()->link_type() != hci::Connection::LinkType::kLE) {
    bt_log(TRACE, "sm", "attempted to send confirm value over BR/EDR, not sending");
    return;
  }

  auto pdu = util::NewPdu(sizeof(PairingConfirmValue));
  PacketWriter writer(kPairingConfirm, pdu.get());
  *writer.mutable_payload<PairingConfirmValue>() = local_confirm_.value();
  sm_chan()->Send(std::move(pdu));
  sent_local_confirm_ = true;
}

void Phase2Legacy::OnPairingConfirm(PairingConfirmValue confirm) {
  ErrorCode err = CanReceivePairingConfirm();
  if (err != ErrorCode::kNoError) {
    Abort(err);
    return;
  }

  peer_confirm_ = confirm;

  if (is_initiator()) {
    // We MUST have a TK and have previously generated an Mconfirm - this was implicitly checked in
    // CanReceivePairingConfirm by checking whether we've sent the confirm value.
    ZX_ASSERT(tk_.has_value());
    ZX_ASSERT(sent_local_confirm_);

    // We have sent Mconfirm and just received Sconfirm. We now send Mrand for the peer to compare.
    SendRandomValue();
  } else if (tk_.has_value()) {
    // We are the responder and have just received Mconfirm. If we already have a TK, we now send
    // the local confirm to the peer. If not, HandleTemporaryKey will take care of that.
    SendConfirmValue();
  }
}

void Phase2Legacy::SendRandomValue() {
  ZX_ASSERT(!sent_local_rand_);
  // This is always generated in the TK callback, which must have been called by now as the random
  // are sent after the confirm values, and the TK must exist in order to send the confirm.
  ZX_ASSERT(local_rand_.has_value());

  // Only allowed on the LE transport.
  if (sm_chan()->link_type() != hci::Connection::LinkType::kLE) {
    bt_log(WARN, "sm", "attempted to send confirm value over BR/EDR, not sending");
    return;
  }

  auto pdu = util::NewPdu(sizeof(PairingRandomValue));
  PacketWriter writer(kPairingRandom, pdu.get());
  *writer.mutable_payload<PairingRandomValue>() = local_rand_.value();
  sm_chan()->Send(std::move(pdu));
  sent_local_rand_ = true;
}

void Phase2Legacy::OnPairingRandom(PairingRandomValue rand) {
  ErrorCode err = CanReceivePairingRandom();
  if (err != ErrorCode::kNoError) {
    Abort(err);
    return;
  }
  // These should have been checked in CanReceivePairingRandom
  ZX_ASSERT(local_rand_.has_value());
  ZX_ASSERT(tk_.has_value());
  ZX_ASSERT(peer_confirm_.has_value());

  peer_rand_ = rand;

  // We have the peer's confirm and rand. Verify the peer confirm to validate the authentication.
  UInt128 peer_confirm_check;
  util::C1(tk_.value(), peer_rand_.value(), preq_, pres_, initiator_addr_, responder_addr_,
           &peer_confirm_check);
  if (peer_confirm_check != peer_confirm_) {
    bt_log(ERROR, "sm", "%sconfirm value does not match!", is_initiator() ? "S" : "M");
    Abort(ErrorCode::kConfirmValueFailed);
    return;
  }

  // Generate the STK.
  UInt128 stk;
  UInt128* initiator_rand = &local_rand_.value();
  UInt128* responder_rand = &peer_rand_.value();
  if (is_responder()) {
    std::swap(initiator_rand, responder_rand);
  }
  util::S1(tk_.value(), *responder_rand, *initiator_rand, &stk);

  // Mask the key based on the requested encryption key size.
  uint8_t key_size = features_.encryption_key_size;
  if (key_size < kMaxEncryptionKeySize) {
    MutableBufferView view(stk.data() + key_size, kMaxEncryptionKeySize - key_size);
    view.SetToZeros();
  }

  // We've generated the STK, so Phase 2 is now over if we're the initiator.
  on_stk_ready_(stk);

  // As responder, we choose to notify the STK to the higher layer before sending our SRand. We
  // expect the peer initiator to request encryption immediately after receiving SRand, and we want
  // to ensure the STK is available at the hci::Connection layer when this occurs.
  if (is_responder()) {
    SendRandomValue();
  }
}

ErrorCode Phase2Legacy::CanReceivePairingConfirm() const {
  // Only allowed on the LE transport.
  if (sm_chan()->link_type() != hci::Connection::LinkType::kLE) {
    bt_log(TRACE, "sm", "\"Confirm value\" over BR/EDR not supported!");
    return ErrorCode::kCommandNotSupported;
  }

  // Per the message sequence charts in V5.1 Vol. 3 Part H Appendix C.2.1, reject the pairing
  // confirm value and abort if
  //    a. we are the initiator, and have not yet sent our confirm value.
  //    b. we are the responder, and have already sent our confirm value.
  if ((is_initiator() && !sent_local_confirm_) || (is_responder() && sent_local_confirm_)) {
    bt_log(WARN, "sm", "abort pairing due to confirm value received out of order");
    return ErrorCode::kUnspecifiedReason;
  }

  // Legacy pairing only allows for one confirm/random exchange per pairing.
  if (peer_confirm_.has_value()) {
    bt_log(WARN, "sm", "already received confirm value! aborting");
    return ErrorCode::kUnspecifiedReason;
  }

  // The confirm value shouldn't be sent after the random value. (See spec V5.0 Vol 3, Part H,
  // 2.3.5.5 and Appendix C.2.1.1 for the specific order of events).
  if (peer_rand_.has_value() || sent_local_rand_) {
    bt_log(WARN, "sm", "\"Pairing Confirm\" must come before \"Pairing Random\"");
    return ErrorCode::kUnspecifiedReason;
  }

  return ErrorCode::kNoError;
}

ErrorCode Phase2Legacy::CanReceivePairingRandom() const {
  // Only allowed on the LE transport.
  if (sm_chan()->link_type() != hci::Connection::LinkType::kLE) {
    bt_log(TRACE, "sm", "\"Random value\" over BR/EDR not supported!");
    return ErrorCode::kCommandNotSupported;
  }

  if (!tk_.has_value()) {
    bt_log(WARN, "sm", "abort pairing, random value received before user input");
    return ErrorCode::kUnspecifiedReason;
  }

  // V5.0 Vol 3, Part H, 2.3.5.5 dictates that there should be exactly one pairing random value
  // received by each peer in Legacy Pairing Phase 2.
  if (peer_rand_.has_value()) {
    bt_log(WARN, "sm", "already received random value! aborting");
    return ErrorCode::kUnspecifiedReason;
  }

  // The random value shouldn't be sent before the confirm value. See V5.0 Vol 3, Part H, 2.3.5.5
  // and Appendix C.2.1.1 for the specific order of events.
  if (!peer_confirm_.has_value()) {
    bt_log(WARN, "sm", "\"Pairing Rand\" expected after \"Pairing Confirm\"");
    return ErrorCode::kUnspecifiedReason;
  }

  if (is_initiator()) {
    // The initiator distributes both values before the responder sends Srandom.
    if (!sent_local_rand_ || !sent_local_confirm_) {
      bt_log(WARN, "sm", "\"Pairing Random\" received in wrong order!");
      return ErrorCode::kUnspecifiedReason;
    }
  } else {
    // We know we have not received Mrand, and should not have sent Srand without receiving Mrand.
    ZX_ASSERT(!sent_local_rand_);

    // We need to send Sconfirm before the initiator sends Mrand.
    if (!sent_local_confirm_) {
      bt_log(WARN, "sm", "\"Pairing Random\" received in wrong order!");
      return ErrorCode::kUnspecifiedReason;
    }
  }

  return ErrorCode::kNoError;
}

void Phase2Legacy::OnRxBFrame(ByteBufferPtr sdu) {
  fit::result<ValidPacketReader, ErrorCode> maybe_reader = ValidPacketReader::ParseSdu(sdu, mtu());
  if (maybe_reader.is_error()) {
    Abort(maybe_reader.error());
    return;
  }
  ValidPacketReader reader = maybe_reader.value();
  Code smp_code = reader.code();

  if (smp_code == kPairingFailed) {
    OnFailure(Status(reader.payload<ErrorCode>()));
  } else if (smp_code == kPairingConfirm) {
    OnPairingConfirm(reader.payload<PairingConfirmValue>());
  } else if (smp_code == kPairingRandom) {
    OnPairingRandom(reader.payload<PairingRandomValue>());
  } else {
    bt_log(INFO, "sm", "received unexpected code %#.2X when in Pairing Legacy Phase 2", smp_code);
    Abort(ErrorCode::kUnspecifiedReason);
  }
}

}  // namespace sm
}  // namespace bt
