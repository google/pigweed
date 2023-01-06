// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "phase_2_secure_connections.h"

#include <memory>
#include <optional>
#include <type_traits>

#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/uint256.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/ecdh_key.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/error.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/packet.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/pairing_phase.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/sc_stage_1_just_works_numeric_comparison.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/sc_stage_1_passkey.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt::sm {

Phase2SecureConnections::Phase2SecureConnections(
    PairingChannel::WeakPtr chan, Listener::WeakPtr listener, Role role, PairingFeatures features,
    PairingRequestParams preq, PairingResponseParams pres, const DeviceAddress& initiator_addr,
    const DeviceAddress& responder_addr, OnPhase2KeyGeneratedCallback cb)
    : PairingPhase(std::move(chan), std::move(listener), role),
      sent_local_ecdh_(false),
      local_ecdh_(),
      peer_ecdh_(),
      stage_1_results_(),
      sent_local_dhkey_check_(false),
      features_(features),
      preq_(preq),
      pres_(pres),
      initiator_addr_(initiator_addr),
      responder_addr_(responder_addr),
      weak_self_(this),
      on_ltk_ready_(std::move(cb)) {
  BT_ASSERT(features_.secure_connections);
  local_ecdh_ = LocalEcdhKey::Create();
  BT_ASSERT_MSG(local_ecdh_.has_value(), "failed to generate ecdh key");
  BT_ASSERT(sm_chan().SupportsSecureConnections());
  SetPairingChannelHandler(*this);
}

void Phase2SecureConnections::Start() {
  BT_ASSERT(!has_failed());
  if (role() == Role::kInitiator) {
    SendLocalPublicKey();
  }
}

void Phase2SecureConnections::SendLocalPublicKey() {
  BT_ASSERT(!sent_local_ecdh_);
  // If in the responder role (i.e. not in the initiator role), attempting to send our Public Key
  // before we've received the peer's is a programmer error.
  BT_ASSERT(role() == Role::kInitiator || peer_ecdh_.has_value());

  sm_chan().SendMessage(kPairingPublicKey, local_ecdh_->GetSerializedPublicKey());
  sent_local_ecdh_ = true;
  bt_log(DEBUG, "sm", "sent ecdh public key to peer");
  if (role() == Role::kResponder) {
    BT_ASSERT(ecdh_exchange_complete());
    StartAuthenticationStage1();
  }
}

fit::result<ErrorCode> Phase2SecureConnections::CanReceivePeerPublicKey() const {
  // Only allowed on the LE transport.
  if (sm_chan().link_type() != bt::LinkType::kLE) {
    bt_log(DEBUG, "sm", "cannot accept peer ecdh key value over BR/EDR");
    return fit::error(ErrorCode::kCommandNotSupported);
  }
  if (peer_ecdh_.has_value()) {
    bt_log(WARN, "sm", "received peer ecdh key twice!");
    return fit::error(ErrorCode::kUnspecifiedReason);
  }
  if (role() == Role::kInitiator && !sent_local_ecdh_) {
    bt_log(WARN, "sm", "received peer ecdh key before sending local key as initiator!");
    return fit::error(ErrorCode::kUnspecifiedReason);
  }
  return fit::ok();
}

void Phase2SecureConnections::OnPeerPublicKey(PairingPublicKeyParams peer_pub_key) {
  if (fit::result result = CanReceivePeerPublicKey(); result.is_error()) {
    Abort(result.error_value());
    return;
  }
  std::optional<EcdhKey> maybe_peer_key = EcdhKey::ParseFromPublicKey(peer_pub_key);
  if (!maybe_peer_key.has_value()) {
    bt_log(WARN, "sm", "unable to validate peer public ECDH key");
    Abort(ErrorCode::kInvalidParameters);
    return;
  }

  EcdhKey peer_key = std::move(*maybe_peer_key);
  BT_ASSERT(local_ecdh_.has_value());
  if (peer_key.GetPublicKeyX() == local_ecdh_->GetPublicKeyX() &&
      peer_key.GetPublicKeyY() == local_ecdh_->GetPublicKeyY()) {
    // NOTE(fxbug.dev/80650): When passkey entry is used, the non-initiating device can reflect
    // our public key (which we send in plaintext). The inputs to the hash that we disclose bit-
    // by-bit in ScStage1Passkey are the two public keys, our nonce, and one bit of our passkey, so
    // if the peer uses the same public key then it can easily brute force for the passkey bit.
    bt_log(WARN, "sm", "peer public ECDH key mirrors local public ECDH key (sent_local_ecdh_: %d)",
           sent_local_ecdh_);
    Abort(ErrorCode::kInvalidParameters);
    return;
  }
  peer_ecdh_ = std::move(peer_key);

  if (role() == Role::kResponder) {
    SendLocalPublicKey();
  } else {
    BT_ASSERT(ecdh_exchange_complete());
    StartAuthenticationStage1();
  }
}

void Phase2SecureConnections::StartAuthenticationStage1() {
  BT_ASSERT(peer_ecdh_);
  auto self = weak_self_.GetWeakPtr();
  auto complete_cb = [self](fit::result<ErrorCode, ScStage1::Output> result) {
    if (self.is_alive()) {
      self->OnAuthenticationStage1Complete(result);
    }
  };
  if (is_just_works_or_numeric_comparison()) {
    bt_log(DEBUG, "sm", "Starting SC Stage 1 Numeric Comparison/Just Works");
    stage_1_ = std::make_unique<ScStage1JustWorksNumericComparison>(
        listener(), role(), local_ecdh_->GetPublicKeyX(), peer_ecdh_->GetPublicKeyX(),
        features_.method, sm_chan().GetWeakPtr(), std::move(complete_cb));
  } else if (is_passkey_entry()) {
    bt_log(DEBUG, "sm", "Starting SC Stage 1 Passkey Entry");
    stage_1_ = std::make_unique<ScStage1Passkey>(listener(), role(), local_ecdh_->GetPublicKeyX(),
                                                 peer_ecdh_->GetPublicKeyX(), features_.method,
                                                 sm_chan().GetWeakPtr(), std::move(complete_cb));
  } else {  // method == kOutOfBand
    // TODO(fxbug.dev/601): OOB would require significant extra plumbing & add security exposure not
    // necessary for current goals. This is not spec-compliant but should allow us to pass PTS.
    bt_log(WARN, "sm", "Received unsupported request for OOB pairing");
    Abort(ErrorCode::kCommandNotSupported);
    return;
  }
  stage_1_->Run();
}

void Phase2SecureConnections::OnAuthenticationStage1Complete(
    fit::result<ErrorCode, ScStage1::Output> result) {
  BT_ASSERT(peer_ecdh_.has_value());
  BT_ASSERT(stage_1_);
  BT_ASSERT(!ltk_.has_value());
  BT_ASSERT(!expected_peer_dhkey_check_.has_value());
  BT_ASSERT(!local_dhkey_check_.has_value());
  // The presence of Stage 1 determines whether to accept PairingConfirm/Random packets, so as it
  // is now over, it should be reset.
  stage_1_ = nullptr;

  if (result.is_error()) {
    Abort(result.error_value());
    return;
  }
  stage_1_results_ = result.value();
  StartAuthenticationStage2();
}

void Phase2SecureConnections::StartAuthenticationStage2() {
  BT_ASSERT(stage_1_results_.has_value());
  std::optional<util::F5Results> maybe_f5 =
      util::F5(local_ecdh_->CalculateDhKey(peer_ecdh_.value()), stage_1_results_->initiator_rand,
               stage_1_results_->responder_rand, initiator_addr_, responder_addr_);
  if (!maybe_f5.has_value()) {
    bt_log(WARN, "sm", "unable to calculate local LTK/MacKey");
    Abort(ErrorCode::kUnspecifiedReason);
    return;
  }

  // Ea & Eb are the DHKey Check values used/defined in V5.0 Vol. 3 Part H Section 2.3.5.6.5/3.5.7.
  // (Ea/Eb) is sent by the (initiator/responder) to be verified by the (responder/initiator). This
  // exchange verifies that each device knows the private key associated with its public key.
  std::optional<PairingDHKeyCheckValueE> ea =
      util::F6(maybe_f5->mac_key, stage_1_results_->initiator_rand,
               stage_1_results_->responder_rand, stage_1_results_->responder_r, preq_.auth_req,
               preq_.oob_data_flag, preq_.io_capability, initiator_addr_, responder_addr_);
  std::optional<PairingDHKeyCheckValueE> eb =
      util::F6(maybe_f5->mac_key, stage_1_results_->responder_rand,
               stage_1_results_->initiator_rand, stage_1_results_->initiator_r, pres_.auth_req,
               pres_.oob_data_flag, pres_.io_capability, responder_addr_, initiator_addr_);
  if (!eb.has_value() || !ea.has_value()) {
    bt_log(WARN, "sm", "unable to calculate dhkey check \"E\"");
    Abort(ErrorCode::kUnspecifiedReason);
    return;
  }
  local_dhkey_check_ = ea;
  expected_peer_dhkey_check_ = eb;
  if (role() == Role::kResponder) {
    std::swap(local_dhkey_check_, expected_peer_dhkey_check_);
  }
  ltk_ = maybe_f5->ltk;

  if (role() == Role::kInitiator) {
    SendDhKeyCheckE();
  } else if (actual_peer_dhkey_check_.has_value()) {
    // As responder, it's possible the initiator sent us the DHKey check while we waited for user
    // input. In that case, check it now instead of when we receive it.
    ValidatePeerDhKeyCheck();
  }
}

void Phase2SecureConnections::SendDhKeyCheckE() {
  BT_ASSERT(stage_1_results_.has_value());
  BT_ASSERT(!sent_local_dhkey_check_);
  BT_ASSERT(ltk_.has_value());
  BT_ASSERT(local_dhkey_check_.has_value());

  // Send local DHKey Check
  sm_chan().SendMessage(kPairingDHKeyCheck, *local_dhkey_check_);
  sent_local_dhkey_check_ = true;
  if (role() == Role::kResponder) {
    // As responder, we should only send the local DHKey check after receiving and validating the
    // peer's. The presence of `peer_dhkey_check` verifies this invariant.
    BT_ASSERT(actual_peer_dhkey_check_.has_value());
    on_ltk_ready_(ltk_.value());
  }
}

fit::result<ErrorCode> Phase2SecureConnections::CanReceiveDhKeyCheck() const {
  // Only allowed on the LE transport.
  if (sm_chan().link_type() != bt::LinkType::kLE) {
    bt_log(WARN, "sm", "cannot accept peer ecdh key check over BR/EDR (SC)");
    return fit::error(ErrorCode::kCommandNotSupported);
  }
  if (!stage_1_results_.has_value() && !stage_1_) {
    bt_log(WARN, "sm", "received peer ecdh check too early! (before stage 1 started)");
    return fit::error(ErrorCode::kUnspecifiedReason);
  }
  if (actual_peer_dhkey_check_.has_value()) {
    bt_log(WARN, "sm", "received peer ecdh key check twice (SC)");
    return fit::error(ErrorCode::kUnspecifiedReason);
  }
  if (role() == Role::kInitiator && !sent_local_dhkey_check_) {
    bt_log(WARN, "sm",
           "received peer ecdh key check as initiator before sending local ecdh key check (SC)");
    return fit::error(ErrorCode::kUnspecifiedReason);
  }
  return fit::ok();
}

void Phase2SecureConnections::OnDhKeyCheck(PairingDHKeyCheckValueE check) {
  if (fit::result result = CanReceiveDhKeyCheck(); result.is_error()) {
    Abort(result.error_value());
    return;
  }
  actual_peer_dhkey_check_ = check;
  // As responder, it's possible to receive the DHKey check from the peer while waiting for user
  // input in Stage 1 - if that happens, we validate the peer DhKey check when Stage 1 completes.
  if (!stage_1_results_.has_value()) {
    BT_ASSERT(role() == Role::kResponder);
    BT_ASSERT(stage_1_);
    return;
  }
  ValidatePeerDhKeyCheck();
}

void Phase2SecureConnections::ValidatePeerDhKeyCheck() {
  BT_ASSERT(actual_peer_dhkey_check_.has_value());
  BT_ASSERT(expected_peer_dhkey_check_.has_value());
  if (*expected_peer_dhkey_check_ != *actual_peer_dhkey_check_) {
    bt_log(WARN, "sm", "DHKey check value failed - possible attempt to hijack pairing!");
    Abort(ErrorCode::kDHKeyCheckFailed);
    return;
  }
  if (role() == Role::kInitiator) {
    bt_log(INFO, "sm", "completed secure connections Phase 2 of pairing");
    on_ltk_ready_(ltk_.value());
  } else {
    SendDhKeyCheckE();
  }
}

void Phase2SecureConnections::OnPairingConfirm(PairingConfirmValue confirm) {
  if (!stage_1_) {
    bt_log(WARN, "sm", "received pairing confirm in SC outside of authentication stage 1");
    Abort(ErrorCode::kUnspecifiedReason);
    return;
  }
  stage_1_->OnPairingConfirm(confirm);
}

void Phase2SecureConnections::OnPairingRandom(PairingRandomValue rand) {
  if (!stage_1_) {
    bt_log(WARN, "sm", "received pairing random in SC outside of authentication stage 1");
    Abort(ErrorCode::kUnspecifiedReason);
    return;
  }
  stage_1_->OnPairingRandom(rand);
}

void Phase2SecureConnections::OnRxBFrame(ByteBufferPtr sdu) {
  fit::result<ErrorCode, ValidPacketReader> maybe_reader = ValidPacketReader::ParseSdu(sdu);
  if (maybe_reader.is_error()) {
    Abort(maybe_reader.error_value());
    return;
  }
  ValidPacketReader reader = maybe_reader.value();
  Code smp_code = reader.code();

  if (smp_code == kPairingFailed) {
    OnFailure(Error(reader.payload<ErrorCode>()));
  } else if (smp_code == kPairingPublicKey) {
    OnPeerPublicKey(reader.payload<PairingPublicKeyParams>());
  } else if (smp_code == kPairingConfirm) {
    OnPairingConfirm(reader.payload<PairingConfirmValue>());
  } else if (smp_code == kPairingRandom) {
    OnPairingRandom(reader.payload<PairingRandomValue>());
  } else if (smp_code == kPairingDHKeyCheck) {
    OnDhKeyCheck(reader.payload<PairingDHKeyCheckValueE>());
  } else {
    bt_log(INFO, "sm", "received unexpected code %d when in Pairing SecureConnections Phase 2",
           smp_code);
    Abort(ErrorCode::kUnspecifiedReason);
  }
}

}  // namespace bt::sm
