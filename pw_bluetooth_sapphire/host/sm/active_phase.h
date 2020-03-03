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

class ActivePhase : public PairingPhase {
 public:
  ~ActivePhase() override = default;

  // Kick off the state machine for the concrete ActivePhase.
  virtual void Start() = 0;

  // Cleans up pairing state and and invokes the listener's error calback.
  void OnFailure(Status status);

  // Ends the current pairing procedure with the given failure |ecode|.
  void Abort(ErrorCode ecode);

  // Called by the owning class when the SMP pairing timer expires.
  void OnPairingTimeout();

 protected:
  // For derived final classes to implement PairingChannel::Handler:
  void HandleChannelClosed();

  // Just delegates to the PairingPhase constructor, as this is a stateless class
  ActivePhase(fxl::WeakPtr<PairingChannel> chan, fxl::WeakPtr<Listener> listener,
              hci::Connection::Role role);

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ActivePhase);
};

}  // namespace sm
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_ACTIVE_PHASE_H_
