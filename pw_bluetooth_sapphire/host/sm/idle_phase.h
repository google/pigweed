// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_IDLE_PHASE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_IDLE_PHASE_H_

#include <lib/fit/function.h>

#include "src/connectivity/bluetooth/core/bt-host/sm/pairing_phase.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {
namespace sm {

// Represents the state an SMP pairing not in progress. Its only responsibilites are accepting and
// passing up Security Requests as initiator / Pairing Requests as responder.
//
// This class is not thread safe and is meant to be accessed on the thread it was created on. All
// callbacks will be run by the default dispatcher of an IdlePhase's creation thread.
using PairingRequestCallback = fit::function<void(PairingRequestParams)>;
using SecurityRequestCallback = fit::function<void(AuthReqField)>;

class IdlePhase final : public PairingPhase, public PairingChannelHandler {
 public:
  // Initializes this IdlePhase with the following parameters:
  //   - |chan|: The L2CAP SMP fixed channel.
  //   - |listener|: The Listener implementer
  //   - |role|: The local connection role.
  //   - |on_pairing_req|: callback which signals the receiption of an SMP Pairing Request.
  //   - |on_security_req|: callback which handles the reception of an SMP Security Request.
  IdlePhase(fxl::WeakPtr<PairingChannel> chan, fxl::WeakPtr<Listener> listener, Role role,
            PairingRequestCallback on_pairing_req, SecurityRequestCallback on_security_req);

  ~IdlePhase() override = default;

 private:
  // Perform basic HCI-level role validation
  void OnPairingRequest(PairingRequestParams req_params);
  void OnSecurityRequest(AuthReqField req);

  // PairingChannelHandler overrides:
  void OnRxBFrame(ByteBufferPtr sdu) override;
  void OnChannelClosed() override;

  // PairingPhase override
  fxl::WeakPtr<PairingChannelHandler> AsChannelHandler() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  fxl::WeakPtrFactory<IdlePhase> weak_ptr_factory_;
  PairingRequestCallback on_pairing_req_;
  SecurityRequestCallback on_security_req_;
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(IdlePhase);
};

}  // namespace sm
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_IDLE_PHASE_H_
