// Copyright 2024 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "pw_bluetooth_sapphire/internal/host/gap/legacy_pairing_state.h"

#include "pw_bluetooth_sapphire/internal/host/common/assert.h"
#include "pw_bluetooth_sapphire/internal/host/common/log.h"
#include "pw_bluetooth_sapphire/internal/host/gap/types.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/constants.h"
#include "pw_bluetooth_sapphire/internal/host/hci-spec/util.h"
#include "pw_bluetooth_sapphire/internal/host/sm/types.h"

namespace bt::gap {

namespace {
const char* const kInspectEncryptionStatusPropertyName = "encryption_status";
const char* const kInspectSecurityPropertiesPropertyName =
    "security_properties";
}  // namespace

LegacyPairingState::LegacyPairingState(Peer::WeakPtr peer,
                                       WeakPtr<hci::BrEdrConnection> link,
                                       bool outgoing_connection,
                                       fit::closure auth_cb,
                                       StatusCallback status_cb)
    : peer_id_(peer->identifier()),
      peer_(std::move(peer)),
      outgoing_connection_(outgoing_connection) {
  // We may receive a request for Legacy Pairing prior to the ACL connection
  // being complete. We can only populate |link_|,
  // |send_auth_request_callback_|, and |status_callback_| if the ACL
  // connection is complete.
  if (link.is_alive()) {
    link_ = std::move(link);
    send_auth_request_callback_ = std::move(auth_cb);
    status_callback_ = std::move(status_cb);

    link_->set_encryption_change_callback(
        fit::bind_member<&LegacyPairingState::OnEncryptionChange>(this));
  }
}

LegacyPairingState::~LegacyPairingState() {
  if (link_.is_alive()) {
    link_->set_encryption_change_callback(nullptr);

    // Pairing requests are only initiated after |link_| is established
    auto callbacks_to_signal =
        CompletePairingRequests(ToResult(HostError::kLinkDisconnected));

    bt_log(TRACE,
           "gap-bredr",
           "Signaling %zu unresolved pairing listeners for link %#.4x",
           callbacks_to_signal.size(),
           handle());

    for (auto& cb : callbacks_to_signal) {
      cb();
    }
  }
}

void LegacyPairingState::InitiatePairing(StatusCallback status_cb) {
  if (!link_.is_alive()) {
    bt_log(WARN,
           "gap-bredr",
           "Do not initiate Legacy Pairing before ACL connection is complete");
    state_ = State::kFailed;
    SignalStatus(ToResult(HostError::kFailed), __func__);
    return;
  }

  BT_ASSERT(peer_.is_alive());

  // If we interrogated the peer and they support SSP, we should be using SSP
  // since we also support SSP.
  if (IsPeerSecureSimplePairingSupported()) {
    bt_log(WARN,
           "gap-bredr",
           "Do not use Legacy Pairing when peer actually supports SSP");
    state_ = State::kFailed;
    SignalStatus(ToResult(HostError::kFailed), __func__);
    return;
  }

  if (state_ == State::kIdle) {
    BT_ASSERT(!is_pairing());

    // TODO(fxbug.dev/348676274): Do not downgrade to LP if peer was
    // previously bonded with SSP
    // TODO(fxbug.dev/348674937): Re-pair with SSP if peer was
    // previously bonded with LP

    // If the current link key already meets the security requirements, skip
    // pairing and report success.
    if (link_->ltk_type() &&
        SecurityPropertiesMeetRequirements(
            sm::SecurityProperties(link_->ltk_type().value()),
            kNoSecurityRequirements)) {
      status_cb(handle(), fit::ok());
      return;
    }

    // TODO(fxbug.dev/42118593): If there is no pairing delegate set AND
    // the current peer does not have a bonded link key, there is no way to
    // upgrade the link security, so we Do not need to bother calling
    // |send_auth_request_callback_|.

    PairingRequest request{.security_requirements = kNoSecurityRequirements,
                           .status_callback = std::move(status_cb)};
    request_queue_.push_back(std::move(request));
    InitiateNextPairingRequest();
    return;
  }

  // Multiple consumers may wish to initiate pairing (e.g. concurrent outbound
  // L2CAP channels), but each should wait for the results of any ongoing
  // pairing procedure before sending their own HCI_Authentication_Request.
  if (is_pairing()) {
    BT_ASSERT(state_ != State::kIdle);
    bt_log(INFO,
           "gap-bredr",
           "Already pairing on link %#.4x for peer id %s; blocking callback on "
           "completion",
           handle(),
           bt_str(peer_id_));

    PairingRequest request{.security_requirements = kNoSecurityRequirements,
                           .status_callback = std::move(status_cb)};
    request_queue_.push_back(std::move(request));
  } else {
    // In the error state, we should expect no pairing to be created and cancel
    // this particular request immediately.
    BT_ASSERT(state_ == State::kFailed);
    status_cb(handle(), ToResult(HostError::kCanceled));
  }
}

std::optional<hci_spec::LinkKey> LegacyPairingState::OnLinkKeyRequest() {
  if (state_ != State::kIdle && state_ != State::kInitiatorWaitLinkKeyRequest) {
    FailWithUnexpectedEvent(__func__);
    return std::nullopt;
  }

  BT_ASSERT(peer_.is_alive());

  // If we interrogated the peer and they support SSP, we should be using SSP
  // since we also support SSP.
  if (link_.is_alive() && IsPeerSecureSimplePairingSupported()) {
    bt_log(WARN,
           "gap-bredr",
           "Do not use Legacy Pairing when peer actually supports SSP");
    state_ = State::kFailed;
    SignalStatus(ToResult(HostError::kFailed), __func__);
    return std::nullopt;
  }

  std::optional<sm::LTK> link_key;

  // Determine if we can reuse a current link key or not. The current link key
  // is valid only if the peer is bonded, has a valid link key, and the key
  // meets the expected security requirements. Otherwise we do not return a link
  // key in order to start the PIN code request process.
  if (peer_missing_key_) {
    bt_log(
        INFO,
        "gap-bredr",
        "Peer %s is missing a link key. Ignore our link key and retry pairing",
        bt_str(peer_id_));
  } else if (peer_->bredr() && peer_->bredr()->bonded()) {
    bt_log(INFO,
           "gap-bredr",
           "Recalling link key for bonded peer %s",
           bt_str(peer_id_));

    BT_ASSERT(peer_->bredr()->link_key().has_value());
    link_key = peer_->bredr()->link_key();
    BT_ASSERT(link_key->security().enc_key_size() ==
              hci_spec::kBrEdrLinkKeySize);

    if (link_.is_alive()) {
      const hci_spec::LinkKeyType link_key_type =
          link_key->security().GetLinkKeyType();
      link_->set_link_key(link_key->key(), link_key_type);
    } else {
      // Connection is not complete yet so temporarily store this to later give
      // to HCI link on the HCI_Connection_Complete event.
      link_key_ = link_key->key();
    }
  } else {
    bt_log(INFO, "gap-bredr", "Peer %s is not bonded", bt_str(peer_id_));
  }

  // The link key request may be received outside of Legacy Pairing (e.g. when
  // the peer initiates the authentication procedure and has a valid link key).
  if (state_ == State::kIdle) {
    if (link_key.has_value()) {
      BT_ASSERT(!is_pairing());
      current_pairing_ = Pairing::MakeResponderForBonded();
      state_ = State::kWaitEncryption;
      return link_key->key();
    }
    return std::nullopt;
  }

  BT_ASSERT(is_pairing());

  // TODO(fxbug.dev/348676274): Do not downgrade to LP if peer was
  // previously bonded with SSP
  // TODO(fxbug.dev/348674937): Re-pair with SSP if peer was previously
  // bonded with LP

  if (link_key.has_value() &&
      SecurityPropertiesMeetRequirements(
          link_key->security(), current_pairing_->preferred_security)) {
    // Skip Legacy Pairing and just perform authentication with existing key
    BT_ASSERT(current_pairing_->initiator);
    state_ = State::kInitiatorWaitAuthComplete;
    return link_key->key();
  }

  // Request that the controller perform Legacy Pairing to generate a new key
  state_ = State::kWaitPinCodeRequest;
  return std::nullopt;
}

void LegacyPairingState::OnLinkKeyNotification(const UInt128& link_key,
                                               hci_spec::LinkKeyType key_type) {
  if (state_ != State::kWaitLinkKey) {
    FailWithUnexpectedEvent(__func__);
    return;
  }

  BT_ASSERT(peer_.is_alive());

  // Legacy Pairing generates a Combination key type (Core Spec v5.4, Vol 4,
  // Part E, 7.7.24)
  if (key_type != hci_spec::LinkKeyType::kCombination) {
    bt_log(WARN,
           "gap-bredr",
           "Legacy Pairing requires Combination key type but link %#.4x for "
           "peer id %s has type %s",
           handle(),
           bt_str(peer_id_),
           hci_spec::LinkKeyTypeToString(key_type));
    state_ = State::kFailed;
    SignalStatus(ToResult(HostError::kFailed), __func__);
    return;
  }

  // The resulting link security properties are computed by both the Link
  // Manager (Controller) and the Host subsystem, so check that they agree.
  BT_ASSERT(is_pairing());
  sm::SecurityProperties sec_props = sm::SecurityProperties(key_type);
  current_pairing_->security_properties = sec_props;

  // Set security properties for this BR/EDR connection
  bredr_security_ = sec_props;

  // Link keys resulting from legacy pairing are assigned lowest security level.
  BT_ASSERT(sec_props.level() == sm::SecurityLevel::kNoSecurity);

  // If we interrogated the peer and they support SSP, we should be using SSP
  // since we also support SSP.
  if (link_.is_alive() && IsPeerSecureSimplePairingSupported()) {
    bt_log(WARN,
           "gap-bredr",
           "Do not use Legacy Pairing when peer actually supports SSP");
    state_ = State::kFailed;
    SignalStatus(ToResult(HostError::kFailed), __func__);
    return;
  }

  if (link_.is_alive()) {
    link_->set_link_key(hci_spec::LinkKey(link_key, /*rand=*/0, /*ediv=*/0),
                        key_type);
  } else {
    // Connection is not complete yet so temporarily store this to later give
    // to HCI link on the HCI_Connection_Complete event.
    link_key_ = hci_spec::LinkKey(link_key, /*rand=*/0, /*ediv=*/0);
  }

  if (initiator()) {
    // Initiators will receive a HCI_Authentication_Complete event
    state_ = State::kInitiatorWaitAuthComplete;
    return;
  }

  if (link_.is_alive()) {
    // Responders can now enable encryption after generating a valid link key
    EnableEncryption();
  }
}

void LegacyPairingState::OnAuthenticationComplete(
    pw::bluetooth::emboss::StatusCode status_code) {
  BT_ASSERT(link_.is_alive());

  if (is_pairing() && peer_->bredr() && peer_->bredr()->bonded() &&
      status_code == pw::bluetooth::emboss::StatusCode::PIN_OR_KEY_MISSING) {
    // Even though we have provided our link key, the peer does not have a valid
    // link key. We restart the pairing process again by sending a
    // HCI_Authentication_Requested command.
    bt_log(INFO,
           "gap-bredr",
           "Re-initiating pairing on link %#.4x for peer id %s as remote "
           "device reports no key",
           handle(),
           bt_str(peer_id_));
    peer_missing_key_ = true;
    current_pairing_->allow_automatic = false;
    state_ = State::kInitiatorWaitLinkKeyRequest;
    send_auth_request_callback_();
    return;
  }

  // The pairing process may fail early, which the controller will deliver as an
  // HCI_Authentication_Complete with a non-success status.
  if (const fit::result result = ToResult(status_code);
      bt_is_error(result,
                  INFO,
                  "gap-bredr",
                  "Authentication failed on link %#.4x for peer id %s",
                  handle(),
                  bt_str(peer_id_))) {
    state_ = State::kFailed;
    SignalStatus(result, __func__);
    return;
  }

  // Fail on unexpected HCI_Authentication_Complete events
  if (state_ != State::kInitiatorWaitAuthComplete) {
    FailWithUnexpectedEvent(__func__);
    return;
  }

  // HCI_Authentication_Complete events are only received by initiators
  BT_ASSERT(initiator());

  // After successful authentication, we can now enable encryption
  EnableEncryption();
}

void LegacyPairingState::OnEncryptionChange(hci::Result<bool> result) {
  BT_ASSERT(link_.is_alive());

  if (state_ != State::kWaitEncryption) {
    // Ignore encryption changes when not expecting them because they may be
    // triggered by the peer at any time (Core Spec v5.4, Vol 2, Part F, 4.4)
    bt_log(TRACE,
           "gap-bredr",
           "Ignoring encryption change event with result %s on link %#.4x for "
           "peer id %s in %s state",
           bt_str(result),
           handle(),
           bt_str(peer_id_),
           ToString(state_));
    return;
  }

  pw::bluetooth::emboss::EncryptionStatus encryption_status =
      link_->encryption_status();
  // Update inspect properties
  inspect_properties_.encryption_status.Set(
      EncryptionStatusToString(encryption_status));

  // E0 encryption shall be used for Legacy Pairing when encryption is enabled
  // (Core Spec v5.4, Vol 2, Part C, 4.2.5)
  if (encryption_status != pw::bluetooth::emboss::EncryptionStatus::
                               ON_WITH_E0_FOR_BREDR_OR_AES_FOR_LE) {
    bt_log(WARN,
           "gap-bredr",
           "E0 encryption must be used for legacy pairing when encryption is "
           "enabled");
    state_ = State::kFailed;
    SignalStatus(ToResult(HostError::kFailed), __func__);
    return;
  }

  if (result.is_ok()) {
    // Encryption indicates the end of pairing so reset state for another
    // pairing
    state_ = State::kIdle;
  } else {
    state_ = State::kFailed;
  }

  SignalStatus(result.is_ok() ? hci::Result<>(fit::ok()) : result.take_error(),
               __func__);
}

std::unique_ptr<LegacyPairingState::Pairing>
LegacyPairingState::Pairing::MakeInitiator(
    BrEdrSecurityRequirements security_requirements, bool outgoing_connection) {
  // Private constructor is inaccessible to std::make_unique
  std::unique_ptr<Pairing> pairing(new Pairing(outgoing_connection));
  pairing->initiator = true;
  pairing->preferred_security = security_requirements;
  return pairing;
}

std::unique_ptr<LegacyPairingState::Pairing>
LegacyPairingState::Pairing::MakeResponder(
    pw::bluetooth::emboss::IoCapability peer_iocap, bool outgoing_connection) {
  // Private constructor is inaccessible to std::make_unique
  std::unique_ptr<Pairing> pairing(new Pairing(outgoing_connection));
  pairing->initiator = false;
  pairing->peer_iocap = peer_iocap;
  // Do not try to upgrade security as responder
  pairing->preferred_security = kNoSecurityRequirements;
  return pairing;
}

std::unique_ptr<LegacyPairingState::Pairing>
LegacyPairingState::Pairing::MakeResponderForBonded() {
  std::unique_ptr<Pairing> pairing(new Pairing(/*outgoing_connection=*/false));
  pairing->initiator = false;
  // Do not try to upgrade security as responder
  pairing->preferred_security = kNoSecurityRequirements;
  return pairing;
}

bool LegacyPairingState::IsPeerSecureSimplePairingSupported() const {
  return peer_->features().HasBit(
             /*page=*/0,
             hci_spec::LMPFeature::kSecureSimplePairingControllerSupport) &&
         peer_->features().HasBit(
             /*page=*/1, hci_spec::LMPFeature::kSecureSimplePairingHostSupport);
}

void LegacyPairingState::EnableEncryption() {
  BT_ASSERT(link_.is_alive());

  if (!link_->StartEncryption()) {
    bt_log(
        ERROR,
        "gap-bredr",
        "Failed to enable encryption on link %#.4x for peer id %s in %s state",
        handle(),
        bt_str(peer_id_),
        ToString(state_));
    status_callback_(link_->handle(), ToResult(HostError::kFailed));
    state_ = State::kFailed;
    return;
  }
  state_ = State::kWaitEncryption;
}

void LegacyPairingState::SignalStatus(hci::Result<> status,
                                      const char* caller) {
  bt_log(INFO,
         "gap-bredr",
         "Signaling pairing listeners for peer id %s from %s with status %s",
         bt_str(peer_id_),
         caller,
         bt_str(status));

  // Collect the callbacks before invoking them so that
  // |CompletePairingRequests()| can safely access members.
  auto callbacks_to_signal = CompletePairingRequests(status);

  if (link_.is_alive()) {
    // This LegacyPairingState may be destroyed by these callbacks (e.g. if
    // signaling an error causes a disconnection), so care must be taken not to
    // access any members.
    status_callback_(handle(), status);
  }

  for (auto& cb : callbacks_to_signal) {
    cb();
  }
}

void LegacyPairingState::InitiateNextPairingRequest() {
  BT_ASSERT(state_ == State::kIdle);
  BT_ASSERT(!is_pairing());

  if (!link_.is_alive()) {
    bt_log(WARN,
           "gap-bredr",
           "Do not initiate Legacy Pairing before ACL connection is complete");
    state_ = State::kFailed;
    SignalStatus(ToResult(HostError::kFailed), __func__);
    return;
  }

  BT_ASSERT(peer_.is_alive());

  // If we interrogated the peer and they support SSP, we should be using SSP
  // since we also support SSP.
  if (IsPeerSecureSimplePairingSupported()) {
    bt_log(WARN,
           "gap-bredr",
           "Do not use Legacy Pairing when peer actually supports SSP");
    state_ = State::kFailed;
    SignalStatus(ToResult(HostError::kFailed), __func__);
    return;
  }

  if (request_queue_.empty()) {
    return;
  }

  PairingRequest& request = request_queue_.front();

  current_pairing_ = Pairing::MakeInitiator(request.security_requirements,
                                            outgoing_connection_);
  bt_log(DEBUG,
         "gap-bredr",
         "Initiating queued pairing on link %#.4x for peer id %s",
         handle(),
         bt_str(peer_id_));
  state_ = State::kInitiatorWaitLinkKeyRequest;
  send_auth_request_callback_();
}

std::vector<fit::closure> LegacyPairingState::CompletePairingRequests(
    hci::Result<> status) {
  std::vector<fit::closure> callbacks_to_signal;

  if (!is_pairing()) {
    BT_ASSERT(request_queue_.empty());
    return callbacks_to_signal;
  }

  if (status.is_error()) {
    // On pairing failure, signal all requests
    for (auto& request : request_queue_) {
      callbacks_to_signal.push_back(
          [handle = handle(),
           status,
           cb = std::move(request.status_callback)]() { cb(handle, status); });
    }
    request_queue_.clear();
    current_pairing_ = nullptr;
    return callbacks_to_signal;
  }

  BT_ASSERT(state_ == State::kIdle);

  sm::SecurityProperties security_properties =
      sm::SecurityProperties(hci_spec::LinkKeyType::kCombination);

  // If a new link key was received, notify all callbacks because we always
  // negotiate the best security possible. Even though pairing succeeded, send
  // an error status if the individual request security requirements are not
  // satisfied.
  // TODO(fxbug.dev/42075714): Only notify failure to callbacks of
  // requests that have the same (or none) on-path attack requirements as the
  // current pairing.
  bool link_key_received = current_pairing_->security_properties.has_value();
  if (link_key_received) {
    for (auto& request : request_queue_) {
      auto sec_props_satisfied = SecurityPropertiesMeetRequirements(
          security_properties, request.security_requirements);
      auto request_status = sec_props_satisfied
                                ? status
                                : ToResult(HostError::kInsufficientSecurity);

      callbacks_to_signal.push_back(
          [handle = handle(),
           request_status,
           cb = std::move(request.status_callback)]() {
            cb(handle, request_status);
          });
    }
    request_queue_.clear();
  } else {
    // If no new link key was received, then only authentication with an old key
    // was performed (Legacy Pairing was not required), and unsatisfied requests
    // should initiate a new pairing rather than failing. If any pairing
    // requests are satisfied by the existing key, notify them.
    auto it = request_queue_.begin();
    while (it != request_queue_.end()) {
      if (!SecurityPropertiesMeetRequirements(security_properties,
                                              it->security_requirements)) {
        it++;
        continue;
      }

      callbacks_to_signal.push_back(
          [handle = handle(), status, cb = std::move(it->status_callback)]() {
            cb(handle, status);
          });
      it = request_queue_.erase(it);
    }
  }
  current_pairing_ = nullptr;
  InitiateNextPairingRequest();

  return callbacks_to_signal;
}

void LegacyPairingState::FailWithUnexpectedEvent(const char* handler_name) {
  bt_log(ERROR,
         "gap-bredr",
         "Unexpected %s event while in %s state on link %#.4x for peer id %s",
         handler_name,
         ToString(state_),
         handle(),
         bt_str(peer_id_));
  state_ = State::kFailed;
  SignalStatus(ToResult(HostError::kFailed), __func__);
}

const char* LegacyPairingState::ToString(LegacyPairingState::State state) {
  switch (state) {
    case State::kIdle:
      return "Idle";
    case State::kInitiatorWaitLinkKeyRequest:
      return "InitiatorWaitLinkKeyRequest";
    case State::kWaitPinCodeRequest:
      return "kWaitPinCodeRequest";
    case State::kWaitLinkKey:
      return "WaitLinkKey";
    case State::kInitiatorWaitAuthComplete:
      return "InitiatorWaitAuthComplete";
    case State::kWaitEncryption:
      return "WaitEncryption";
    case State::kFailed:
      return "Failed";
    default:
      break;
  }
  return "";
}

void LegacyPairingState::AttachInspect(inspect::Node& parent,
                                       std::string name) {
  inspect_node_ = parent.CreateChild(name);

  if (link_.is_alive()) {
    inspect_properties_.encryption_status = inspect_node_.CreateString(
        kInspectEncryptionStatusPropertyName,
        EncryptionStatusToString(link_->encryption_status()));
  }

  bredr_security_.AttachInspect(inspect_node_,
                                kInspectSecurityPropertiesPropertyName);
}

}  // namespace bt::gap
