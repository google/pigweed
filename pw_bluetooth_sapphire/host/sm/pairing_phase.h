// Copyright 2020 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PAIRING_PHASE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PAIRING_PHASE_H_

#include <string>

#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/pairing_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/connectivity/bluetooth/lib/cpp-string/string_printf.h"
#include "src/lib/fxl/memory/weak_ptr.h"

namespace bt::sm {

// Abstract class representing one of the four in-progress phases of pairing described in Vol. 3
// Part H 2.1.
//
// After a PairingPhase fails (i.e. through calling OnFailure), it is invalid to make any further
// method calls on the phase.
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
    // TODO(fxbug.dev/49966): Use an optional to convey success/failure instead of the signedness of
    // passkey.
    using PasskeyResponseCallback = fit::function<void(int64_t passkey)>;
    virtual void RequestPasskey(PasskeyResponseCallback respond) = 0;

    // Called when an on-going pairing procedure terminates with an error. This method should
    // destroy the Phase that calls it.
    virtual void OnPairingFailed(Error error) = 0;
  };

  virtual ~PairingPhase() = default;

  // Kick off the state machine for the concrete PairingPhase.
  virtual void Start() = 0;

  // Return a representation of the current state of the pairing phase for display purposes.
  std::string ToString() {
    return bt_lib_cpp_string::StringPrintf("%s Role: SMP %s%s", ToStringInternal().c_str(),
                                           role_ == Role::kInitiator ? "initiator" : "responder",
                                           has_failed_ ? " - pairing has failed" : "");
  }

  // Cleans up pairing state and and invokes Listener::OnPairingFailed.
  void OnFailure(Error error);

  // Ends the current pairing procedure unsuccessfully with |ecode| as the reason, and calls
  // OnFailure.
  void Abort(ErrorCode ecode);

  Role role() const { return role_; }

 protected:
  // Protected constructor as PairingPhases should not be created directly. Initializes this
  // PairingPhase with the following parameters:
  //   - |chan|: The L2CAP SMP fixed channel.
  //   - |listener|: The class that will handle higher-level requests from the current phase.
  //   - |role|: The local connection role.
  PairingPhase(fxl::WeakPtr<PairingChannel> chan, fxl::WeakPtr<Listener> listener, Role role);

  // For derived final classes to implement PairingChannel::Handler:
  void HandleChannelClosed();

  PairingChannel& sm_chan() const {
    ZX_ASSERT(sm_chan_);
    return *sm_chan_;
  }

  fxl::WeakPtr<Listener> listener() const { return listener_; }

  // Concrete classes of PairingPhase must be PairingChannelHandlers and set the channel's handler
  // to a weak pointer to themselves. This abstract method forces concrete PairingPhases to vend
  // weak pointers to help enforce this.
  virtual fxl::WeakPtr<PairingChannel::Handler> AsChannelHandler() = 0;

  // To ZX_ASSERT that methods are not called on a phase that has already failed.
  bool has_failed() const { return has_failed_; }

  // For subclasses to provide more detailed inspect information.
  virtual std::string ToStringInternal() = 0;

 private:
  fxl::WeakPtr<PairingChannel> sm_chan_;
  fxl::WeakPtr<Listener> listener_;
  Role role_;
  bool has_failed_;
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(PairingPhase);
};

}  // namespace bt::sm

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_SM_PAIRING_PHASE_H_
