// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_IDLE_PHASE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_IDLE_PHASE_H_

#include <lib/fit/function.h>

#include "src/connectivity/bluetooth/core/bt-host/sm/pairing_channel.h"
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
  //   - |chan|, |listener|, |role|: To construct the base PairingPhase
  //   - |on_pairing_req|: callback which signals the receiption of an SMP Pairing Request.
  //   - |on_security_req|: callback which handles the reception of an SMP Security Request.
  IdlePhase(fxl::WeakPtr<PairingChannel> chan, fxl::WeakPtr<Listener> listener, Role role,
            PairingRequestCallback on_pairing_req, SecurityRequestCallback on_security_req);

  ~IdlePhase() override = default;

  // PairingPhase override. IdlePhase does no work besides making security requests, thus Start
  // does no work.
  void Start() final{};

  // PairingPhase override. Needed so `Abort`ing while no security upgrade is active does not
  // result in pairing failure side effects.
  void Abort(ErrorCode ecode) override;

  // Makes a Security Request to the peer per V5.0 Vol. 3 Part H 2.4.6 - may only be called as the
  // SMP responder. Providing SecurityLevel::kNoSecurity as |desired_level| is a client error and
  // will assert.
  void MakeSecurityRequest(SecurityLevel desired_level, BondableMode bondable_mode);

  std::optional<SecurityLevel> pending_security_request() const {
    return pending_security_request_;
  }

 private:
  // Perform basic HCI-level role validation
  void OnPairingRequest(PairingRequestParams req_params);
  void OnSecurityRequest(AuthReqField req);

  // PairingChannelHandler overrides:
  void OnRxBFrame(ByteBufferPtr sdu) override;
  void OnChannelClosed() override { PairingPhase::HandleChannelClosed(); };

  // PairingPhase override
  fxl::WeakPtr<PairingChannelHandler> AsChannelHandler() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

  std::optional<SecurityLevel> pending_security_request_;

  PairingRequestCallback on_pairing_req_;
  SecurityRequestCallback on_security_req_;

  fxl::WeakPtrFactory<IdlePhase> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(IdlePhase);
};

}  // namespace sm
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_IDLE_PHASE_H_
