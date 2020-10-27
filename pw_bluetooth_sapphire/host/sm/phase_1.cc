// Copyright 2020 the Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "phase_1.h"

#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/common/status.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci_constants.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/scoped_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/packet.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"

namespace bt::sm {

std::unique_ptr<Phase1> Phase1::CreatePhase1Initiator(
    fxl::WeakPtr<PairingChannel> chan, fxl::WeakPtr<Listener> listener, IOCapability io_capability,
    BondableMode bondable_mode, SecurityLevel requested_level, CompleteCallback on_complete) {
  // Use `new` & unique_ptr constructor here instead of `std::make_unique` because the private
  // Phase1 constructor prevents std::make_unique from working (https://abseil.io/tips/134).
  return std::unique_ptr<Phase1>(new Phase1(std::move(chan), std::move(listener), Role::kInitiator,
                                            std::nullopt, io_capability, bondable_mode,
                                            requested_level, std::move(on_complete)));
}

std::unique_ptr<Phase1> Phase1::CreatePhase1Responder(
    fxl::WeakPtr<PairingChannel> chan, fxl::WeakPtr<Listener> listener, PairingRequestParams preq,
    IOCapability io_capability, BondableMode bondable_mode, SecurityLevel minimum_allowed_level,
    CompleteCallback on_complete) {
  // Use `new` & unique_ptr constructor here instead of `std::make_unique` because the private
  // Phase1 constructor prevents std::make_unique from working (https://abseil.io/tips/134).
  return std::unique_ptr<Phase1>(new Phase1(std::move(chan), std::move(listener), Role::kResponder,
                                            preq, io_capability, bondable_mode,
                                            minimum_allowed_level, std::move(on_complete)));
}

Phase1::Phase1(fxl::WeakPtr<PairingChannel> chan, fxl::WeakPtr<Listener> listener, Role role,
               std::optional<PairingRequestParams> preq, IOCapability io_capability,
               BondableMode bondable_mode, SecurityLevel requested_level,
               CompleteCallback on_complete)
    : PairingPhase(std::move(chan), std::move(listener), role),
      preq_(preq),
      requested_level_(requested_level),
      oob_available_(false),
      io_capability_(io_capability),
      bondable_mode_(bondable_mode),
      on_complete_(std::move(on_complete)),
      weak_ptr_factory_(this) {
  ZX_ASSERT(!(role == Role::kInitiator && preq_.has_value()));
  ZX_ASSERT(!(role == Role::kResponder && !preq_.has_value()));
  ZX_ASSERT(requested_level_ >= SecurityLevel::kEncrypted);
  if (requested_level_ > SecurityLevel::kEncrypted) {
    ZX_ASSERT(io_capability != IOCapability::kNoInputNoOutput);
  }
  sm_chan().SetChannelHandler(weak_ptr_factory_.GetWeakPtr());
}

void Phase1::Start() {
  ZX_ASSERT(!has_failed());
  if (role() == Role::kResponder) {
    ZX_ASSERT(preq_.has_value());
    RespondToPairingRequest(*preq_);
    return;
  }
  ZX_ASSERT(!preq_.has_value());
  InitiateFeatureExchange();
}

void Phase1::InitiateFeatureExchange() {
  // Only the initiator can initiate the feature exchange.
  ZX_ASSERT(role() == Role::kInitiator);
  LocalPairingParams preq_values = BuildPairingParameters();
  preq_ = PairingRequestParams{.io_capability = preq_values.io_capability,
                               .oob_data_flag = preq_values.oob_data_flag,
                               .auth_req = preq_values.auth_req,
                               .max_encryption_key_size = preq_values.max_encryption_key_size,
                               .initiator_key_dist_gen = preq_values.local_keys,
                               .responder_key_dist_gen = preq_values.remote_keys};
  sm_chan().SendMessage(kPairingRequest, *preq_);
}

void Phase1::RespondToPairingRequest(const PairingRequestParams& req_params) {
  // We should only be in this state when pairing is initiated by the remote i.e. we are the
  // responder.
  ZX_ASSERT(role() == Role::kResponder);

  LocalPairingParams pres_values = BuildPairingParameters();
  pres_ = PairingResponseParams{.io_capability = pres_values.io_capability,
                                .oob_data_flag = pres_values.oob_data_flag,
                                .auth_req = pres_values.auth_req,
                                .max_encryption_key_size = pres_values.max_encryption_key_size};
  // The keys that will be exchanged correspond to the intersection of what the
  // initiator requests and we support.
  pres_->initiator_key_dist_gen = pres_values.remote_keys & req_params.initiator_key_dist_gen;
  pres_->responder_key_dist_gen = pres_values.local_keys & req_params.responder_key_dist_gen;

  fit::result<PairingFeatures, ErrorCode> maybe_features =
      ResolveFeatures(false /* local_initiator */, req_params, *pres_);
  if (maybe_features.is_error()) {
    bt_log(DEBUG, "sm", "rejecting pairing features");
    Abort(maybe_features.error());
    return;
  }
  PairingFeatures features = maybe_features.value();
  // If we've accepted a non-bondable pairing request in bondable mode as indicated by setting
  // features.will_bond false, we should reflect that in the rsp_params we send to the peer.
  if (!features.will_bond && bondable_mode_ == BondableMode::Bondable) {
    pres_->auth_req &= ~AuthReq::kBondingFlag;
  }

  sm_chan().SendMessage(kPairingResponse, *pres_);

  on_complete_(features, *preq_, *pres_);
}

LocalPairingParams Phase1::BuildPairingParameters() {
  // We build `local_params` to reflect the capabilities of this device over the LE transport.
  LocalPairingParams local_params;
  if (sm_chan().SupportsSecureConnections()) {
    local_params.auth_req |= AuthReq::kSC;
  }
  if (requested_level_ >= SecurityLevel::kAuthenticated) {
    local_params.auth_req |= AuthReq::kMITM;
  }

  // If we are in non-bondable mode there will be no key distribution per V5.1 Vol 3 Part C Section
  // 9.4.2.2, so we use the default "no keys" value for LocalPairingParams.
  if (bondable_mode_ == BondableMode::Bondable) {
    local_params.auth_req |= AuthReq::kBondingFlag;
    // We always request identity information from the remote.
    local_params.remote_keys = KeyDistGen::kIdKey;

    ZX_ASSERT(listener());
    if (listener()->OnIdentityRequest().has_value()) {
      local_params.local_keys |= KeyDistGen::kIdKey;
    }

    // For the current connection, the responder-generated encryption keys (LTK) is always used. As
    // device roles may change in future connections, Fuchsia supports distribution and generation
    // of LTKs by both the local and remote device (V5.0 Vol. 3 Part H 2.4.2.3).
    local_params.remote_keys |= KeyDistGen::kEncKey;
    local_params.local_keys |= KeyDistGen::kEncKey;

    // If we support SC over LE, we always try to generate the cross-transport BR/EDR key by
    // setting the link key bit (V5.0 Vol. 3 Part H 3.6.1).
    if (local_params.auth_req & AuthReq::kSC) {
      local_params.local_keys |= KeyDistGen::kLinkKey;
      local_params.remote_keys |= KeyDistGen::kLinkKey;
    }
  }
  // The CT2 bit indicates support for the 2nd Cross-Transport Key Derivation hashing function,
  // a.k.a. H7 (v5.2 Vol. 3 Part H 3.5.1 and 2.4.2.4).
  local_params.auth_req |= AuthReq::kCT2;
  local_params.io_capability = io_capability_;
  local_params.oob_data_flag = oob_available_ ? OOBDataFlag::kPresent : OOBDataFlag::kNotPresent;
  return local_params;
}

fit::result<PairingFeatures, ErrorCode> Phase1::ResolveFeatures(bool local_initiator,
                                                                const PairingRequestParams& preq,
                                                                const PairingResponseParams& pres) {
  // Select the smaller of the initiator and responder max. encryption key size
  // values (Vol 3, Part H, 2.3.4).
  uint8_t enc_key_size = std::min(preq.max_encryption_key_size, pres.max_encryption_key_size);
  uint8_t min_allowed_enc_key_size = (requested_level_ == SecurityLevel::kSecureAuthenticated)
                                         ? kMaxEncryptionKeySize
                                         : kMinEncryptionKeySize;
  if (enc_key_size < min_allowed_enc_key_size) {
    bt_log(DEBUG, "sm", "encryption key size too small! (%u)", enc_key_size);
    return fit::error(ErrorCode::kEncryptionKeySize);
  }

  bool will_bond = (preq.auth_req & kBondingFlag) && (pres.auth_req & kBondingFlag);
  if (!will_bond) {
    bt_log(INFO, "sm", "negotiated non-bondable pairing (local mode: %s)",
           bondable_mode_ == BondableMode::Bondable ? "bondable" : "non-bondable");
  }
  bool sc = (preq.auth_req & AuthReq::kSC) && (pres.auth_req & AuthReq::kSC);
  bool mitm = (preq.auth_req & AuthReq::kMITM) || (pres.auth_req & AuthReq::kMITM);
  bool init_oob = preq.oob_data_flag == OOBDataFlag::kPresent;
  bool rsp_oob = pres.oob_data_flag == OOBDataFlag::kPresent;

  IOCapability local_ioc, peer_ioc;
  if (local_initiator) {
    local_ioc = preq.io_capability;
    peer_ioc = pres.io_capability;
  } else {
    local_ioc = pres.io_capability;
    peer_ioc = preq.io_capability;
  }

  PairingMethod method =
      util::SelectPairingMethod(sc, init_oob, rsp_oob, mitm, local_ioc, peer_ioc, local_initiator);

  // If MITM protection is required but the pairing method cannot provide MITM,
  // then reject the pairing.
  if (mitm && method == PairingMethod::kJustWorks) {
    return fit::error(ErrorCode::kAuthenticationRequirements);
  }

  // The "Pairing Response" command (i.e. |pres|) determines the keys that shall
  // be distributed. The keys that will be distributed by us and the peer
  // depends on whichever one initiated the feature exchange by sending a
  // "Pairing Request" command.
  KeyDistGenField local_keys, remote_keys;
  if (local_initiator) {
    local_keys = pres.initiator_key_dist_gen;
    remote_keys = pres.responder_key_dist_gen;

    // v5.1, Vol 3, Part H Section 3.6.1 requires that the responder shall not set to one
    // any flag in the key dist gen fields that the initiator has set to zero.
    // Hence we reject the pairing if the responder requests keys that we don't
    // support.
    if ((preq.initiator_key_dist_gen & local_keys) != local_keys ||
        (preq.responder_key_dist_gen & remote_keys) != remote_keys) {
      return fit::error(ErrorCode::kInvalidParameters);
    }
  } else {
    local_keys = pres.responder_key_dist_gen;
    remote_keys = pres.initiator_key_dist_gen;

    // When we are the responder we always respect the initiator's wishes.
    ZX_ASSERT((preq.initiator_key_dist_gen & remote_keys) == remote_keys);
    ZX_ASSERT((preq.responder_key_dist_gen & local_keys) == local_keys);
  }
  // v5.1 Vol 3 Part C Section 9.4.2.2 says that bonding information shall not be exchanged or
  // stored in non-bondable mode. This check ensures that we avoid a situation where, if we were in
  // bondable mode and a peer requested non-bondable mode with a non-zero keydistgen field, we pair
  // in non-bondable mode but also attempt to distribute keys.
  if (!will_bond && (local_keys || remote_keys)) {
    return fit::error(ErrorCode::kInvalidParameters);
  }

  // "If both [...] devices support [LE] Secure Connections [...] the devices may optionally
  // generate the BR/EDR key [..] as part of the LE pairing procedure" (v5.2 Vol. 3 Part C 14.1).
  std::optional<CrossTransportKeyAlgo> generate_ct_key = std::nullopt;
  if (sc) {
    // "In LE Secure Connections pairing, when SMP is running on the LE transport, then the EncKey
    // field is ignored" (V5.0 Vol. 3 Part H 3.6.1). We ignore the Encryption Key bit here to allow
    // for uniform handling of it later.
    local_keys &= ~KeyDistGen::kEncKey;
    remote_keys &= ~KeyDistGen::kEncKey;

    // "When LinkKey is set to 1 by both devices in the initiator and responder [KeyDistGen] fields,
    // the procedures for calculating the BR/EDR link key from the LTK shall be used". The chosen
    // procedure depends on the CT2 bit of the AuthReq (v5.2 Vol. 3 Part H 3.5.1 and 3.6.1).
    if (local_keys & remote_keys & KeyDistGen::kLinkKey) {
      generate_ct_key = (preq.auth_req & AuthReq::kCT2) && (pres.auth_req & AuthReq::kCT2)
                            ? CrossTransportKeyAlgo::kUseH7
                            : CrossTransportKeyAlgo::kUseH6;
    }

  } else if (requested_level_ == SecurityLevel::kSecureAuthenticated) {
    // SecureAuthenticated means Secure Connections is required, so if this pairing would not use
    // Secure Connections it does not meet the requirements of `requested_level_`
    return fit::error(ErrorCode::kAuthenticationRequirements);
  }

  return fit::ok(PairingFeatures(local_initiator, sc, will_bond, generate_ct_key, method,
                                 enc_key_size, local_keys, remote_keys));
}

void Phase1::OnPairingResponse(const PairingResponseParams& response_params) {
  // Support receiving a pairing response only as the initiator.
  if (role() == Role::kResponder) {
    bt_log(DEBUG, "sm", "received pairing response when acting as responder");
    Abort(ErrorCode::kCommandNotSupported);
    return;
  }

  if (!preq_.has_value() || pres_.has_value()) {
    bt_log(DEBUG, "sm", "ignoring unexpected \"Pairing Response\" packet");
    Abort(ErrorCode::kUnspecifiedReason);
    return;
  }

  fit::result<PairingFeatures, ErrorCode> maybe_features =
      ResolveFeatures(true /* local_initiator */, *preq_, response_params);

  if (maybe_features.is_error()) {
    bt_log(DEBUG, "sm", "rejecting pairing features");
    Abort(maybe_features.error());
    return;
  }
  PairingFeatures features = maybe_features.value();
  pres_ = response_params;
  on_complete_(features, *preq_, *pres_);
}

void Phase1::OnRxBFrame(ByteBufferPtr sdu) {
  fit::result<ValidPacketReader, ErrorCode> maybe_reader = ValidPacketReader::ParseSdu(sdu);
  if (maybe_reader.is_error()) {
    Abort(maybe_reader.error());
    return;
  }
  ValidPacketReader reader = maybe_reader.value();
  Code smp_code = reader.code();

  if (smp_code == kPairingFailed) {
    OnFailure(Status(reader.payload<ErrorCode>()));
  } else if (smp_code == kPairingResponse) {
    OnPairingResponse(reader.payload<PairingResponseParams>());
  } else {
    bt_log(INFO, "sm", "received unexpected code %#.2X when in Pairing Phase 1", smp_code);
    Abort(ErrorCode::kUnspecifiedReason);
  }
}

}  // namespace bt::sm
