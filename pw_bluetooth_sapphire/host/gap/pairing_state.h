// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_PAIRING_STATE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_PAIRING_STATE_H_

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/smp.h"

namespace bt {
namespace gap {

// Represents the local user interaction that will occur, as inferred from Core
// Spec v5.0 Vol 3, Part C, Sec 5.2.2.6 (Table 5.7). This is not directly
// coupled to the reply action for the HCI "User" event for pairing; e.g.
// kDisplayPasskey may mean automatically confirming User Confirmation Request
// or displaying the value from User Passkey Notification.
enum class PairingAction {
  // Don't involve the user.
  kAutomatic,

  // Request yes/no consent.
  kGetConsent,

  // Display 6-digit value with "cancel."
  kDisplayPasskey,

  // Display 6-digit value with "yes/no."
  kComparePasskey,

  // Request a 6-digit value entry.
  kRequestPasskey,
};

class PairingState final {
 public:
  PairingState();
  ~PairingState() = default;

  bool initiator() const { return initiator_; }

  // Starts pairing against the peer, if pairing is not already in progress.
  // This device becomes the pairing initiator.
  void InitiatePairing();

  void OnIOCapabilitiesResponse(hci::IOCapability peer_iocap);

 private:
  bool initiator_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(PairingState);
};

PairingAction GetInitiatorPairingAction(hci::IOCapability initiator_cap,
                                        hci::IOCapability responder_cap);
PairingAction GetResponderPairingAction(hci::IOCapability initiator_cap,
                                        hci::IOCapability responder_cap);
hci::EventCode GetExpectedEvent(hci::IOCapability local_cap,
                                hci::IOCapability peer_cap);
bool IsPairingAuthenticated(hci::IOCapability local_cap,
                            hci::IOCapability peer_cap);

// Get the Authentication Requirements for a locally-initiated pairing according
// to Core Spec v5.0, Vol 2, Part E, Sec 7.1.29.
//
// Non-Bondable Mode and Dedicated Bonding over BR/EDR are not supported and
// this always returns kMITMGeneralBonding if |local_cap| is not
// kNoInputNoOutput, kGeneralBonding otherwise. This requests authentication
// when possible (based on IO Capabilities), as we don't know the peer's
// authentication requirements yet.
hci::AuthRequirements GetInitiatorAuthRequirements(hci::IOCapability local_cap);

// Get the Authentication Requirements for a peer-initiated pairing. This will
// request MITM protection whenever possible to obtain an "authenticated" link
// encryption key.
//
// Local service requirements and peer authentication bonding type should be
// available by the time this is called, but Non-Bondable Mode and Dedicated
// Bonding over BR/EDR are not supported, so this always returns
// kMITMGeneralBonding if this pairing can result in an authenticated link key,
// kGeneralBonding otherwise.
hci::AuthRequirements GetResponderAuthRequirements(
    hci::IOCapability local_cap, hci::IOCapability remote_cap);

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_PAIRING_STATE_H_
