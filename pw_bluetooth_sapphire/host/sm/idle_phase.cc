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

namespace bt {

namespace sm {

IdlePhase::IdlePhase(fxl::WeakPtr<PairingChannel> chan, fxl::WeakPtr<Listener> listener,
                     hci::Connection::Role role, PairingRequestCallback on_pairing_req,
                     SecurityRequestCallback on_security_req)
    : PairingPhase(std::move(chan), std::move(listener), role),
      weak_ptr_factory_(this),
      on_pairing_req_(std::move(on_pairing_req)),
      on_security_req_(std::move(on_security_req)) {
  sm_chan().SetChannelHandler(weak_ptr_factory_.GetWeakPtr());
}

void IdlePhase::OnPairingRequest(PairingRequestParams req_params) {
  // Reject the command if we are the master.
  if (is_initiator()) {
    bt_log(TRACE, "sm", "rejecting \"Pairing Request\" as master");
    SendPairingFailed(ErrorCode::kCommandNotSupported);
    return;
  }
  on_pairing_req_(req_params);
}

void IdlePhase::OnSecurityRequest(AuthReqField req) {
  // Reject the command if we are the slave.
  if (is_responder()) {
    bt_log(TRACE, "sm", "rejecting \"Security Request\" as slave");
    SendPairingFailed(ErrorCode::kCommandNotSupported);
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

void IdlePhase::OnChannelClosed() { bt_log(TRACE, "sm", "channel closed while not pairing"); }

}  // namespace sm
}  // namespace bt
