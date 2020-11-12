// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_PAIRING_DELEGATE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_PAIRING_DELEGATE_H_

#include <lib/fit/function.h>

#include "src/connectivity/bluetooth/core/bt-host/common/identifier.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/status.h"

namespace bt::gap {

// An object that implements PairingDelegate is responsible for fulfilling user
// authentication challenges during pairing.
class PairingDelegate {
 public:
  using ConfirmCallback = fit::callback<void(bool confirm)>;

  // Selects the intent of the passkey provided to the underlying pairing
  // provider.
  enum DisplayMethod {
    kComparison,  // Both sides display a passkey which the user compares.
    kPeerEntry,   // User enters the displayed passkey into the peer device.
  };

  virtual ~PairingDelegate() = default;

  // Returns the I/O capability of this delegate.
  virtual sm::IOCapability io_capability() const = 0;

  // Terminate any ongoing pairing challenge for the peer device with the given
  // |identifier|.
  virtual void CompletePairing(PeerId peer_id, sm::Status status) = 0;

  // Ask the user to confirm the pairing request from the device with the given
  // |id| and confirm or reject by calling |confirm|.
  virtual void ConfirmPairing(PeerId peer_id, ConfirmCallback confirm) = 0;

  // Show the user the 6-digit |passkey| that should be compared to the peer's passkey or entered
  // into the peer. |confirm| may be called to accept a comparison or to reject the pairing.
  virtual void DisplayPasskey(PeerId peer_id, uint32_t passkey, DisplayMethod method,
                              ConfirmCallback confirm) = 0;

  // Ask the user to enter a 6-digit passkey or reject pairing. Report the
  // result by invoking |respond|.
  //
  // A valid |passkey| must be a non-negative integer. Pass a negative value to
  // reject pairing.
  using PasskeyResponseCallback = fit::callback<void(int64_t passkey)>;
  virtual void RequestPasskey(PeerId peer_id, PasskeyResponseCallback respond) = 0;

 protected:
  PairingDelegate() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(PairingDelegate);
};

}  // namespace bt::gap

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_PAIRING_DELEGATE_H_
