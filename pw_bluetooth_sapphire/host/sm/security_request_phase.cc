// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "security_request_phase.h"

#include <type_traits>

#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/packet.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"

namespace bt::sm {

SecurityRequestPhase::SecurityRequestPhase(fxl::WeakPtr<PairingChannel> chan,
                                           fxl::WeakPtr<Listener> listener,
                                           SecurityLevel desired_level, BondableMode bondable_mode,
                                           PairingRequestCallback on_pairing_req)
    : PairingPhase(std::move(chan), std::move(listener), Role::kResponder),
      bondable_mode_(bondable_mode),
      pending_security_request_(desired_level),
      on_pairing_req_(std::move(on_pairing_req)),
      weak_ptr_factory_(this) {
  sm_chan().SetChannelHandler(weak_ptr_factory_.GetWeakPtr());
}

void SecurityRequestPhase::Start() {
  MakeSecurityRequest(pending_security_request_, bondable_mode_);
}

void SecurityRequestPhase::MakeSecurityRequest(SecurityLevel desired_level,
                                               BondableMode bondable_mode) {
  BT_ASSERT(desired_level >= SecurityLevel::kEncrypted);
  AuthReqField security_req_payload = 0u;
  if (desired_level >= SecurityLevel::kAuthenticated) {
    security_req_payload |= AuthReq::kMITM;
  }
  if (bondable_mode == BondableMode::Bondable) {
    security_req_payload |= AuthReq::kBondingFlag;
  }
  if (desired_level == SecurityLevel::kSecureAuthenticated) {
    security_req_payload |= AuthReq::kSC;
  }
  pending_security_request_ = desired_level;
  sm_chan().SendMessage(kSecurityRequest, security_req_payload);
}

void SecurityRequestPhase::OnPairingRequest(PairingRequestParams req_params) {
  on_pairing_req_(req_params);
}

void SecurityRequestPhase::OnRxBFrame(ByteBufferPtr sdu) {
  fit::result<ErrorCode, ValidPacketReader> maybe_reader = ValidPacketReader::ParseSdu(sdu);
  if (maybe_reader.is_error()) {
    Abort(maybe_reader.error_value());
    return;
  }
  ValidPacketReader reader = maybe_reader.value();
  Code smp_code = reader.code();

  if (smp_code == kPairingRequest) {
    OnPairingRequest(reader.payload<PairingRequestParams>());
  } else {
    bt_log(DEBUG, "sm", "received unexpected code %#.2X with pending Security Request", smp_code);
    Abort(ErrorCode::kUnspecifiedReason);
  }
}

}  // namespace bt::sm
