// Copyright 2020 the Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "idle_phase.h"

#include <zircon/assert.h>

#include <type_traits>

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/packet.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/util.h"

namespace bt {

namespace sm {

IdlePhase::IdlePhase(fxl::WeakPtr<PairingChannel> chan, fxl::WeakPtr<Listener> listener, Role role,
                     PairingRequestCallback on_pairing_req, SecurityRequestCallback on_security_req)
    : PairingPhase(std::move(chan), std::move(listener), role),
      on_pairing_req_(std::move(on_pairing_req)),
      on_security_req_(std::move(on_security_req)),
      weak_ptr_factory_(this) {
  sm_chan().SetChannelHandler(weak_ptr_factory_.GetWeakPtr());
}

void IdlePhase::Abort(ErrorCode ecode) {
  // If no security request is pending, there is nothing to abort.
  if (pending_security_request().has_value()) {
    PairingPhase::Abort(ecode);
  }
}

void IdlePhase::MakeSecurityRequest(SecurityLevel desired_level, BondableMode bondable_mode) {
  ZX_ASSERT(role() == Role::kResponder);
  ZX_ASSERT(desired_level >= SecurityLevel::kEncrypted);
  AuthReqField security_req_payload = 0u;
  if (desired_level >= SecurityLevel::kAuthenticated) {
    security_req_payload |= AuthReq::kMITM;
  }
  if (bondable_mode == BondableMode::Bondable) {
    security_req_payload |= AuthReq::kBondingFlag;
  }
  pending_security_request_ = desired_level;
  sm_chan().SendMessage(kSecurityRequest, security_req_payload);
}

void IdlePhase::OnPairingRequest(PairingRequestParams req_params) {
  // Only the initiator may send the Pairing Request (V5.0 Vol. 3 Part H 3.5.1).
  if (role() == Role::kInitiator) {
    bt_log(DEBUG, "sm", "rejecting \"Pairing Request\" as initiator");
    sm_chan().SendMessage(kPairingFailed, ErrorCode::kCommandNotSupported);
    return;
  }
  on_pairing_req_(req_params);
}

void IdlePhase::OnSecurityRequest(AuthReqField req) {
  // Only the responder may send the Security Request (V5.0 Vol. 3 Part H 2.4.6).
  if (role() == Role::kResponder) {
    bt_log(DEBUG, "sm", "rejecting \"Security Request\" as responder");
    sm_chan().SendMessage(kPairingFailed, ErrorCode::kCommandNotSupported);
    return;
  }
  on_security_req_(req);
}

void IdlePhase::OnRxBFrame(ByteBufferPtr sdu) {
  fit::result<ValidPacketReader, ErrorCode> maybe_reader = ValidPacketReader::ParseSdu(sdu);
  if (maybe_reader.is_error()) {
    bt_log(INFO, "sm", "dropped SMP packet: %s", bt_str(Status(maybe_reader.error())));
    return;
  }
  ValidPacketReader reader = maybe_reader.value();
  Code smp_code = reader.code();

  if (smp_code == kPairingRequest) {
    OnPairingRequest(reader.payload<PairingRequestParams>());
  } else if (smp_code == kSecurityRequest) {
    OnSecurityRequest(reader.payload<AuthReqField>());
  } else {
    bt_log(INFO, "sm", "dropped unexpected SMP code %#.2X when not pairing", smp_code);
  }
}

}  // namespace sm
}  // namespace bt
