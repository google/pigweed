// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_PAIRING_STATE_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_PAIRING_STATE_H_

#include <optional>
#include <vector>

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
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

// Tracks the pairing state of a peer's BR/EDR link. This drives HCI
// transactions and user interactions for pairing in order to obtain the highest
// possible level of link security given the capabilities of the controllers
// and hosts participating in the pairing.
//
// This implements Core Spec v5.0 Vol 2, Part F, Sec 4.2 through Sec 4.4, per
// logic requirements in Vol 3, Part C, Sec 5.2.2.
//
// This tracks both the bonded case (both hosts furnish their Link Keys to their
// controllers) and the unbonded case (both controllers perform Secure Simple
// Pairing and deliver the resulting Link Keys to their hosts).
//
// Pairing is considered complete when the Link Keys have been used to
// successfully encrypt the link, at which time pairing may be restarted (e.g.
// with different capabilities).
//
// This state machine navigates the following HCI message sequences, in which
// both the host subsystem and the Link Manager use knowledge of both peers' IO
// Capabilities and Authentication Requirements to decide on the same
// association model.
// ▶ means command.
// ◀ means event.
//
// Initiator flow
// --------------
// Authentication Requested▶
//     ◀ Link Key Request
// Link Key Request Reply▶ (skip to "Set Connection Encryption")
//     or
// Link Key Request Negative Reply▶ (continue with pairing)
//     ◀ Command Complete
//     ◀ IO Capability Request
// IO Capability Request Reply▶
//     or
// IO Capability Request Negative Reply▶ (reject pairing)
//     ◀ Command Complete
//     ◀ IO Capability Response
//     ◀ User Confirmation Request
//         or
//     ◀ User Passkey Request
//         or
//     ◀ User Passkey Notification
//         or
//     ◀ Remote OOB Data Request
// User Confirmation Request Reply▶
//     or
// User Confirmation Request Negative Reply▶ (reject pairing)
//     or
// User Passkey Request Reply▶
//     or
// User Passkey Request Negative Reply▶ (reject pairing)
//     or
// Remote OOB Data Request Reply▶
//     or
// Remote OOB Extended Data Request Reply▶
//     or
// Remote OOB Data Request Negative Reply▶ (reject pairing)
//     ◀ Simple Pairing Complete (status may be error)
//     ◀ Link Key Notification (key may be insufficient)
//     ◀ Authentication Complete (status may be error)
// Set Connection Encryption▶
//     ◀ Command Status
//     ◀ Encryption Change (status may be error or encryption may be disabled)
//
// Responder flow
// --------------
//     ◀ IO Capability Response
//     ◀ IO Capability Request
// IO Capability Request Reply▶
//     or
// IO Capability Request Negative Reply▶ (reject pairing)
//     ◀ Command Complete
// Pairing
//     ◀ User Confirmation Request
//         or
//     ◀ User Passkey Request
//         or
//     ◀ User Passkey Notification
//         or
//     ◀ Remote OOB Data Request
// User Confirmation Request Reply▶
//     or
// User Confirmation Request Negative Reply▶ (reject pairing)
//     or
// User Passkey Request Reply▶
//     or
// User Passkey Request Negative Reply▶ (reject pairing)
//     or
// Remote OOB Data Request Reply▶
//     or
// Remote OOB Extended Data Request Reply▶
//     or
// Remote OOB Data Request Negative Reply▶ (reject pairing)
//     ◀ Simple Pairing Complete (status may contain error)
//     ◀ Link Key Notification (key may be insufficient)
// Set Connection Encryption▶
//     ◀ Command Status
//     ◀ Encryption Change (status may be error or encryption may be disabled)
//
// This class is not thread-safe and should only be called on the thread on
// which it was created.
class PairingState final {
 public:
  // Used to report the status of each pairing procedure on this link. |status|
  // will contain HostError::kNotSupported if the pairing procedure does not
  // proceed in the order of events expected.
  using StatusCallback = fit::function<void(hci::ConnectionHandle, hci::Status)>;

  // Constructs a PairingState for the ACL connection |link|. This object will
  // receive its "encryption change" callbacks. Successful pairing is reported
  // through |status_cb| after encryption is enabled. When errors occur, this
  // object will be put in a "failed" state and the owner shall disconnect the
  // link and destroy its PairingState.
  //
  // |link| must be valid for the lifetime of this object.
  PairingState(hci::Connection* link, StatusCallback status_cb);
  ~PairingState() = default;

  // True if there is currently a pairing procedure in progress that the local
  // device initiated.
  bool initiator() const { return is_pairing() ? current_pairing_->initiator : false; }

  // Starts pairing against the peer, if pairing is not already in progress.
  // If not, this device becomes the pairing initiator, and returns
  // |kSendAuthenticationRequest| to indicate that the caller shall send an
  // Authentication Request for this peer.
  //
  // When pairing completes or errors out, the |status_cb| of each call to this
  // function will be invoked with the result.
  enum class InitiatorAction {
    kDoNotSendAuthenticationRequest,
    kSendAuthenticationRequest,
  };
  [[nodiscard]] InitiatorAction InitiatePairing(StatusCallback status_cb);

  // Event handlers. Caller must ensure that the event is addressed to the link
  // for this PairingState.

  // Returns value for IO Capability Request Reply, else std::nullopt for IO
  // Capability Negative Reply.
  //
  // TODO(BT-8): Indicate presence of out-of-band (OOB) data.
  [[nodiscard]] std::optional<hci::IOCapability> OnIoCapabilityRequest();

  // Caller is not expected to send a response.
  void OnIoCapabilityResponse(hci::IOCapability peer_iocap);

  // |cb| is called with: true to send User Confirmation Request Reply, else
  // for to send User Confirmation Request Negative Reply. It may not be called
  // from the same thread that called OnUserConfirmationRequest.
  using UserConfirmationCallback = fit::callback<void(bool confirm)>;
  void OnUserConfirmationRequest(uint32_t numeric_value, UserConfirmationCallback cb);

  // |cb| is called with: passkey value to send User Passkey Request Reply, else
  // std::nullopt to send User Passkey Request Negative Reply. It may not be
  // called from the same thread that called OnUserPasskeyRequest.
  using UserPasskeyCallback = fit::callback<void(std::optional<uint32_t> passkey)>;
  void OnUserPasskeyRequest(UserPasskeyCallback cb);

  // Caller is not expected to send a response.
  void OnUserPasskeyNotification(uint32_t numeric_value);

  // Caller is not expected to send a response.
  void OnSimplePairingComplete(hci::StatusCode status_code);

  // Caller is not expected to send a response.
  void OnLinkKeyNotification(const UInt128& link_key, hci::LinkKeyType key_type);

  // Caller is not expected to send a response.
  void OnAuthenticationComplete(hci::StatusCode status_code);

  // Handler for hci::Connection::set_encryption_change_callback.
  void OnEncryptionChange(hci::Status status, bool enabled);

 private:
  enum class State {
    // Wait for initiator's IO Capability Response or for locally-initiated
    // pairing.
    kIdle,

    // As initiator, wait for IO Capability Request or Authentication Complete.
    kInitiatorPairingStarted,

    // As initiator, wait for IO Capability Response.
    kInitiatorWaitIoCapResponse,

    // As responder, wait for IO Capability Request.
    kResponderWaitIoCapRequest,

    // Wait for controller event for pairing action.
    kWaitPairingEvent,

    // Wait for Simple Pairing Complete.
    kWaitPairingComplete,

    // Wait for Link Key Notification.
    kWaitLinkKey,

    // As initiator, wait for Authentication Complete.
    kInitiatorWaitAuthComplete,

    // Wait for Encryption Change.
    kWaitEncryption,

    // Error occurred; wait for link closure and ignore events.
    kFailed,
  };

  // Extra information for pairing constructed when a pairing procedure begins and destroyed when
  // the pairing procedure is reset or errors out.
  struct Pairing final {
    // True if the local device initiated pairing.
    bool initiator;

    // Callbacks from callers of |InitiatePairing|.
    std::vector<StatusCallback> initiator_callbacks;
  };

  static const char* ToString(State state);

  State state() const { return state_; }

  bool is_pairing() const { return current_pairing_.has_value(); }

  hci::ConnectionHandle handle() const { return link_->handle(); }

  // Call the permanent status callback this object was created with as well as any callbacks from
  // local initiators. Resets the current pairing but does not change the state machine state.
  void SignalStatus(hci::Status status);

  // Called to enable encryption on the link for this peer. Sets |state_| to
  // kWaitEncryption.
  void EnableEncryption();

  // Called when an event is received while in a state that doesn't expect that
  // event. Invokes |status_callback_| with HostError::kNotSupported and sets
  // |state_| to kFailed. Logs an error using |handler_name| for identification.
  void FailWithUnexpectedEvent(const char* handler_name);

  // The BR/EDR link whose pairing is being driven by this object.
  hci::Connection* const link_;

  // State machine representation.
  State state_;

  // Represents an ongoing pairing procedure. Will contain a value when the state isn't kIdle or
  // kFailed.
  std::optional<Pairing> current_pairing_;

  // Callback that status of this pairing is reported back through.
  StatusCallback status_callback_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(PairingState);
};

PairingAction GetInitiatorPairingAction(hci::IOCapability initiator_cap,
                                        hci::IOCapability responder_cap);
PairingAction GetResponderPairingAction(hci::IOCapability initiator_cap,
                                        hci::IOCapability responder_cap);
hci::EventCode GetExpectedEvent(hci::IOCapability local_cap, hci::IOCapability peer_cap);
bool IsPairingAuthenticated(hci::IOCapability local_cap, hci::IOCapability peer_cap);

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
hci::AuthRequirements GetResponderAuthRequirements(hci::IOCapability local_cap,
                                                   hci::IOCapability remote_cap);

}  // namespace gap
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_GAP_PAIRING_STATE_H_
