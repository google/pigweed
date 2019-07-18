// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/pairing_state.h"

#include "src/connectivity/bluetooth/core/bt-host/common/log.h"

namespace bt {
namespace gap {

using hci::AuthRequirements;
using hci::IOCapability;

PairingState::PairingState(StatusCallback status_cb)
    : initiator_(false),
      state_(State::kIdle),
      status_callback_(std::move(status_cb)) {
  ZX_ASSERT(status_callback_);
}

PairingState::InitiatorAction PairingState::InitiatePairing() {
  if (state() == State::kIdle) {
    initiator_ = true;
    state_ = State::kInitiatorPairingStarted;
    return InitiatorAction::kSendAuthenticationRequest;
  }
  return InitiatorAction::kDoNotSendAuthenticationRequest;
}

std::optional<hci::IOCapability> PairingState::OnIoCapabilityRequest() {
  if (state() == State::kInitiatorPairingStarted) {
    ZX_ASSERT(initiator());
    state_ = State::kInitiatorWaitIoCapResponse;
  } else if (state() == State::kResponderWaitIoCapRequest) {
    ZX_ASSERT(!initiator());
    state_ = State::kWaitPairingEvent;

    // TODO(xow): Compute pairing event to wait for.
  } else {
    FailWithUnexpectedEvent(__func__);
    return std::nullopt;
  }

  // TODO(xow): Return local IO Capability.
  return std::nullopt;
}

void PairingState::OnIoCapabilityResponse(hci::IOCapability peer_iocap) {
  // TODO(xow): Store peer IO Capability.
  if (state() == State::kIdle) {
    ZX_ASSERT(!initiator());
    state_ = State::kResponderWaitIoCapRequest;
  } else if (state() == State::kInitiatorWaitIoCapResponse) {
    ZX_ASSERT(initiator());
    state_ = State::kWaitPairingEvent;

    // TODO(xow): Compute pairing event to wait for.
  } else {
    FailWithUnexpectedEvent(__func__);
  }
}

void PairingState::OnUserConfirmationRequest(uint32_t numeric_value,
                                             UserConfirmationCallback cb) {
  if (state() != State::kWaitPairingEvent) {
    FailWithUnexpectedEvent(__func__);
    cb(false);
    return;
  }

  // TODO(xow): Return actual user response.
  state_ = State::kWaitPairingComplete;
  cb(true);
}

void PairingState::OnUserPasskeyRequest(UserPasskeyCallback cb) {
  if (state() != State::kWaitPairingEvent) {
    FailWithUnexpectedEvent(__func__);
    cb(std::nullopt);
    return;
  }

  // TODO(xow): Return actual user response.
  state_ = State::kWaitPairingComplete;
  cb(0);
}

void PairingState::OnUserPasskeyNotification(uint32_t numeric_value) {
  if (state() != State::kWaitPairingEvent) {
    FailWithUnexpectedEvent(__func__);
    return;
  }

  // TODO(xow): Display passkey to user.
  state_ = State::kWaitPairingComplete;
}

void PairingState::OnSimplePairingComplete(hci::StatusCode status_code) {
  if (state() != State::kWaitPairingComplete) {
    FailWithUnexpectedEvent(__func__);
    return;
  }

  // TODO(xow): Check |status_code|.
  state_ = State::kWaitLinkKey;
}

void PairingState::OnLinkKeyNotification(const UInt128& link_key,
                                         hci::LinkKeyType key_type) {
  if (state() != State::kWaitLinkKey) {
    FailWithUnexpectedEvent(__func__);
    return;
  }

  // TODO(xow): Store link key.
  if (initiator()) {
    state_ = State::kInitiatorWaitAuthComplete;
  } else {
    EnableEncryption();
  }
}

void PairingState::OnAuthenticationComplete(hci::StatusCode status_code) {
  if (state() != State::kInitiatorPairingStarted &&
      state() != State::kInitiatorWaitAuthComplete) {
    FailWithUnexpectedEvent(__func__);
    return;
  }
  ZX_ASSERT(initiator());

  // TODO(xow): Check |status_code|.
  EnableEncryption();
}

void PairingState::OnEncryptionChange(hci::Status status, bool enabled) {
  if (state() != State::kWaitEncryption) {
    bt_log(INFO, "gap-bredr",
           "%s(%s, %s) in state \"%s\", before pairing completed", __func__,
           bt_str(status), enabled ? "true" : "false", ToString(state()));
    return;
  }

  // TODO(xow): Notify |status_callback_|.
  // TODO(xow): Notify |InitiatePairing| callers.
  if (status) {
    // Reset state for another pairing.
    state_ = State::kIdle;
    initiator_ = false;
  } else {
    FailWithUnexpectedEvent(__func__);
  }
}

const char* PairingState::ToString(PairingState::State state) {
  switch (state) {
    case State::kIdle:
      return "Idle";
    case State::kInitiatorPairingStarted:
      return "InitiatorPairingStarted";
    case State::kInitiatorWaitIoCapResponse:
      return "InitiatorWaitIoCapResponse";
    case State::kResponderWaitIoCapRequest:
      return "ResponderWaitIoCapRequest";
    case State::kWaitPairingEvent:
      return "WaitPairingEvent";
    case State::kWaitPairingComplete:
      return "WaitPairingComplete";
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

void PairingState::EnableEncryption() {
  // TODO(xow): Start encryption.
  state_ = State::kWaitEncryption;
}

void PairingState::FailWithUnexpectedEvent(const char* handler_name) {
  // TODO(xow): Reply with the handle of the link for this pairing.
  bt_log(ERROR, "gap-bredr", "Unexpected event %s while in state \"%s\"",
         handler_name, ToString(state()));
  status_callback_(0x0000, hci::Status(HostError::kNotSupported));
  state_ = State::kFailed;
}

PairingAction GetInitiatorPairingAction(IOCapability initiator_cap,
                                        IOCapability responder_cap) {
  if (initiator_cap == IOCapability::kNoInputNoOutput) {
    return PairingAction::kAutomatic;
  }
  if (responder_cap == IOCapability::kNoInputNoOutput) {
    if (initiator_cap == IOCapability::kDisplayYesNo) {
      return PairingAction::kGetConsent;
    }
    return PairingAction::kAutomatic;
  }
  if (initiator_cap == IOCapability::kKeyboardOnly) {
    return PairingAction::kRequestPasskey;
  }
  if (responder_cap == IOCapability::kDisplayOnly) {
    if (initiator_cap == IOCapability::kDisplayYesNo) {
      return PairingAction::kComparePasskey;
    }
    return PairingAction::kAutomatic;
  }
  return PairingAction::kDisplayPasskey;
}

PairingAction GetResponderPairingAction(IOCapability initiator_cap,
                                        IOCapability responder_cap) {
  if (initiator_cap == IOCapability::kNoInputNoOutput &&
      responder_cap == IOCapability::kKeyboardOnly) {
    return PairingAction::kGetConsent;
  }
  if (initiator_cap == IOCapability::kDisplayYesNo &&
      responder_cap == IOCapability::kDisplayYesNo) {
    return PairingAction::kComparePasskey;
  }
  return GetInitiatorPairingAction(responder_cap, initiator_cap);
}

hci::EventCode GetExpectedEvent(IOCapability local_cap, IOCapability peer_cap) {
  if (local_cap == IOCapability::kNoInputNoOutput ||
      peer_cap == IOCapability::kNoInputNoOutput) {
    return hci::kUserConfirmationRequestEventCode;
  }
  if (local_cap == IOCapability::kKeyboardOnly) {
    return hci::kUserPasskeyRequestEventCode;
  }
  if (peer_cap == IOCapability::kKeyboardOnly) {
    return hci::kUserPasskeyNotificationEventCode;
  }
  return hci::kUserConfirmationRequestEventCode;
}

bool IsPairingAuthenticated(IOCapability local_cap, IOCapability peer_cap) {
  if (local_cap == IOCapability::kNoInputNoOutput ||
      peer_cap == IOCapability::kNoInputNoOutput) {
    return false;
  }
  if (local_cap == IOCapability::kDisplayYesNo &&
      peer_cap == IOCapability::kDisplayYesNo) {
    return true;
  }
  if (local_cap == IOCapability::kKeyboardOnly ||
      peer_cap == IOCapability::kKeyboardOnly) {
    return true;
  }
  return false;
}

AuthRequirements GetInitiatorAuthRequirements(IOCapability local_cap) {
  if (local_cap == IOCapability::kNoInputNoOutput) {
    return AuthRequirements::kGeneralBonding;
  }
  return AuthRequirements::kMITMGeneralBonding;
}

AuthRequirements GetResponderAuthRequirements(IOCapability local_cap,
                                              IOCapability peer_cap) {
  if (IsPairingAuthenticated(local_cap, peer_cap)) {
    return AuthRequirements::kMITMGeneralBonding;
  }
  return AuthRequirements::kGeneralBonding;
}

}  // namespace gap
}  // namespace bt
