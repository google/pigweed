// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_ACTIVE_PHASE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_ACTIVE_PHASE_H_

#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/pairing_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/pairing_phase.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {
namespace sm {

// Abstract class representing one of the four in-progress phases of pairing described in Vol. 3
// Part H 2.1. Each of those phases is represented as a derived class of ActivePhase.
//
// After an ActivePhase fails (i.e. through calling OnFailure), it is invalid to make any further
// method calls on the phase.
class ActivePhase : public PairingPhase {
 public:
  ~ActivePhase() override = default;

  // Kick off the state machine for the concrete ActivePhase.
  virtual void Start() = 0;

  // Cleans up pairing state and and invokes PairingPhase::Listener::OnPairingFailed.
  void OnFailure(Status status);

  // Ends the current pairing procedure unsuccessfully, with |ecode| as the reason, and calls
  // OnFailure.
  void Abort(ErrorCode ecode);

  // Called by the owning class when the SMP pairing timer expires, calls OnFailure.
  void OnPairingTimeout();

 protected:
  // For derived final classes to implement PairingChannel::Handler:
  void HandleChannelClosed();

  // Just delegates to the PairingPhase constructor.
  ActivePhase(fxl::WeakPtr<PairingChannel> chan, fxl::WeakPtr<Listener> listener, Role role);

  // To ZX_ASSERT that methods are not called on a PairingPhase that has already failed.
  bool has_failed() const { return has_failed_; }

 private:
  bool has_failed_;
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ActivePhase);
};

}  // namespace sm
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_ACTIVE_PHASE_H_
