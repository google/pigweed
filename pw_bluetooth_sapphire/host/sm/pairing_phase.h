// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PAIRING_PHASE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PAIRING_PHASE_H_

#include <lib/fit/function.h>
#include <lib/fit/result.h>
#include <zircon/assert.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/packet.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/pairing_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/status.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt {
namespace sm {

// Abstract class representing one of the SMP pairing phases as outlined in V5.1 Vol. 3 Part H
// section 2.1. A generic pairing phase operates over the fixed SMP L2CAP channel and directly
// implements a small subset of functionality relevant to all phases of pairing.
//
// This class is not thread safe and is meant to be accessed on the thread it
// was created on. All callbacks will be run by the default dispatcher of a
// PairingPhase's creation thread.
using PairingChannelHandler = PairingChannel::Handler;

class PairingPhase {
 public:
  // Interface for notifying the owner of the phase object.
  class Listener {
   public:
    virtual ~Listener() = default;

    // Polls for the local identity information, which must be handled by another component of the
    // Bluetooth stack. Returns std::nullopt if no local identity info is available.
    virtual std::optional<IdentityInfo> OnIdentityRequest() = 0;

    using ConfirmCallback = fit::function<void(bool confirm)>;
    virtual void ConfirmPairing(ConfirmCallback confirm) = 0;

    // Show the user the 6-digit |passkey| that should be compared to the peer's passkey or entered
    // into the peer. |confirm| may be called to accept a comparison or to reject the pairing.
    virtual void DisplayPasskey(uint32_t passkey, Delegate::DisplayMethod method,
                                ConfirmCallback confirm) = 0;

    // Ask the user to enter a 6-digit passkey or reject pairing. Reports the result by invoking
    // |respond| with |passkey| - a negative value of |passkey| indicates entry failed.
    // TODO(49966): Use an optional to convey success/failure instead of the signedness of passkey.
    using PasskeyResponseCallback = fit::function<void(int64_t passkey)>;
    virtual void RequestPasskey(PasskeyResponseCallback respond) = 0;

    // Called when a new LTK is available from legacy pairing phase 3.
    virtual void OnNewLongTermKey(const LTK& ltk) = 0;

    // Called when an on-going pairing procedure terminates with an error. This method should
    // destroy the Phase that calls it. |status| will never indicate success.
    virtual void OnPairingFailed(Status status) = 0;
  };

  virtual ~PairingPhase() = default;

  // Returns the SMP role.
  Role role() const { return role_; }

 protected:
  // Protected constructor as PairingPhases should not be created directly. Initializes this
  // PairingPhase with the following parameters:
  //   - |chan|: The L2CAP SMP fixed channel.
  //   - |listener|: The class that will handle higher-level requests from the current phase.
  //   - |role|: The local connection role.
  PairingPhase(fxl::WeakPtr<PairingChannel> chan, fxl::WeakPtr<Listener> listener, Role role);

  // Concrete classes of PairingPhase must be PairingChannelHandlers and set the channel's handler
  // to a weak pointer to themselves. This abstract method forces concrete PairingPhases to vend
  // weak pointers to help enforce this.
  virtual fxl::WeakPtr<PairingChannelHandler> AsChannelHandler() = 0;

  // Sends a Pairing Failed command to the peer with `ecode` as the associated reason.
  void SendPairingFailed(ErrorCode ecode);

  PairingChannel& sm_chan() const {
    ZX_ASSERT(chan_);
    return *chan_;
  }

  fxl::WeakPtr<Listener> listener() { return listener_; }

 private:
  fxl::WeakPtr<PairingChannel> chan_;
  fxl::WeakPtr<Listener> listener_;
  Role role_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(PairingPhase);
};

}  // namespace sm
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PAIRING_PHASE_H_
