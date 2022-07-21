// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_SECURITY_REQUEST_PHASE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_SECURITY_REQUEST_PHASE_H_

#include <lib/fit/function.h>

#include <string>

#include "src/connectivity/bluetooth/core/bt-host/sm/pairing_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/pairing_phase.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/connectivity/bluetooth/lib/cpp-string/string_printf.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt::sm {

// SecurityRequestPhase is responsible for sending outbound Security Requests and handling the
// peer's response. As Security Requests can only be sent from an SMP responder, this class should
// only be instantiated when acting as the SMP responder.
//
// This class is not thread safe and is meant to be accessed on the thread it was created on. All
// callbacks will be run by the default dispatcher of an SecurityRequestPhase's creation thread.

class SecurityRequestPhase final : public PairingPhase, public PairingChannelHandler {
 public:
  // Initializes this SecurityRequestPhase with the following parameters:
  //   - |chan|, |listener|: To construct the base PairingPhase
  //   - |desired_level|: The level of security requested by the SM client to cause this Security
  //                      Request.
  //   - |bondable_mode|: The operating bondable mode of the device (v5.2 Vol. 3 Part C 9.4).
  //   - |on_pairing_req|: Used to signal the owning class of an inbound Pairing Request triggered
  //                       by this Security Request.
  SecurityRequestPhase(fxl::WeakPtr<PairingChannel> chan, fxl::WeakPtr<Listener> listener,
                       SecurityLevel desired_level, BondableMode bondable_mode,
                       PairingRequestCallback on_pairing_req);

  ~SecurityRequestPhase() override = default;

  // PairingPhase override.
  void Start() final;

  SecurityLevel pending_security_request() const { return pending_security_request_; }

 private:
  // Makes a Security Request to the peer per V5.0 Vol. 3 Part H 2.4.6. Providing
  // SecurityLevel::kNoSecurity as |desired_level| is a client error and will assert.
  void MakeSecurityRequest(SecurityLevel desired_level, BondableMode bondable_mode);

  // Handle pairing requests from the peer.
  void OnPairingRequest(PairingRequestParams req_params);

  // PairingChannelHandler overrides:
  void OnRxBFrame(ByteBufferPtr sdu) override;
  void OnChannelClosed() override { PairingPhase::HandleChannelClosed(); }

  // PairingPhase overrides
  fxl::WeakPtr<PairingChannelHandler> AsChannelHandler() override {
    return weak_ptr_factory_.GetWeakPtr();
  }
  std::string ToStringInternal() override {
    return bt_lib_cpp_string::StringPrintf(
        "Security Request Phase  - pending security request for %s",
        LevelToString(pending_security_request_));
  }

  BondableMode bondable_mode_;
  SecurityLevel pending_security_request_;

  PairingRequestCallback on_pairing_req_;
  fxl::WeakPtrFactory<SecurityRequestPhase> weak_ptr_factory_;
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(SecurityRequestPhase);
};

}  // namespace bt::sm

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_SECURITY_REQUEST_PHASE_H_
