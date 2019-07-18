// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/pairing_state.h"

#include "gtest/gtest.h"
#include "src/connectivity/bluetooth/core/bt-host/common/random.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"

namespace bt {
namespace gap {
namespace {

using hci::AuthRequirements;
using hci::IOCapability;
using hci::kUserConfirmationRequestEventCode;
using hci::kUserPasskeyNotificationEventCode;
using hci::kUserPasskeyRequestEventCode;

const auto kTestPeerIoCap = IOCapability::kDisplayYesNo;
const uint32_t kTestPasskey = 123456;
const auto kTestLinkKeyValue = Random<UInt128>();
const auto kTestAuthenticatedLinkKeyType =
    hci::LinkKeyType::kAuthenticatedCombination192;

void NoOpStatusCallback(hci::ConnectionHandle, hci::Status){};
void NoOpUserConfirmationCallback(bool){};
void NoOpUserPasskeyCallback(std::optional<uint32_t>){};

TEST(GAP_PairingStateTest, PairingStateStartsAsResponder) {
  PairingState pairing_state(NoOpStatusCallback);
  EXPECT_FALSE(pairing_state.initiator());
}

TEST(GAP_PairingStateTest, PairingStateRemainsResponderAfterPeerIoCapResponse) {
  PairingState pairing_state(NoOpStatusCallback);
  pairing_state.OnIoCapabilityResponse(kTestPeerIoCap);
  EXPECT_FALSE(pairing_state.initiator());
}

TEST(GAP_PairingStateTest,
     PairingStateBecomesInitiatorAfterLocalPairingInitiated) {
  PairingState pairing_state(NoOpStatusCallback);
  EXPECT_EQ(PairingState::InitiatorAction::kSendAuthenticationRequest,
            pairing_state.InitiatePairing());
  EXPECT_TRUE(pairing_state.initiator());
}

TEST(GAP_PairingStateTest, PairingStateSendsAuthenticationRequestExactlyOnce) {
  PairingState pairing_state(NoOpStatusCallback);
  EXPECT_EQ(PairingState::InitiatorAction::kSendAuthenticationRequest,
            pairing_state.InitiatePairing());
  EXPECT_TRUE(pairing_state.initiator());

  EXPECT_EQ(PairingState::InitiatorAction::kDoNotSendAuthenticationRequest,
            pairing_state.InitiatePairing());
  EXPECT_TRUE(pairing_state.initiator());
}

TEST(
    GAP_PairingStateTest,
    PairingStateRemainsResponderIfPairingInitiatedWhileResponderPairingInProgress) {
  PairingState pairing_state(NoOpStatusCallback);
  pairing_state.OnIoCapabilityResponse(kTestPeerIoCap);
  ASSERT_FALSE(pairing_state.initiator());

  EXPECT_EQ(PairingState::InitiatorAction::kDoNotSendAuthenticationRequest,
            pairing_state.InitiatePairing());
  EXPECT_FALSE(pairing_state.initiator());
}

// Test helper to inspect StatusCallback invocations.
class TestStatusHandler final {
 public:
  auto MakeStatusCallback() {
    return [this]([[maybe_unused]] hci::ConnectionHandle, hci::Status status) {
      call_count_++;
      status_ = status;
    };
  }

  auto call_count() const { return call_count_; }

  // Returns std::nullopt if |call_count() < 1|, otherwise the most recent
  // status this was called with.
  auto& status() const { return status_; }

 private:
  int call_count_ = 0;
  std::optional<hci::Status> status_;
};

TEST(GAP_PairingStateTest, TestStatusHandlerTracksStatusCallbackInvocations) {
  TestStatusHandler handler;
  EXPECT_EQ(0, handler.call_count());
  EXPECT_FALSE(handler.status());

  PairingState::StatusCallback status_cb = handler.MakeStatusCallback();
  EXPECT_EQ(0, handler.call_count());
  EXPECT_FALSE(handler.status());

  status_cb(hci::ConnectionHandle(0x0A0B),
            hci::Status(hci::StatusCode::kPairingNotAllowed));
  EXPECT_EQ(1, handler.call_count());
  ASSERT_TRUE(handler.status());
  EXPECT_EQ(hci::Status(hci::StatusCode::kPairingNotAllowed),
            *handler.status());
}

// Event injectors. Return values are necessarily ignored in order to make types
// match, so don't use these functions to test return values. Likewise,
// arguments have been filled with test defaults for a successful pairing flow.
void IoCapabilityRequest(PairingState* pairing_state) {
  static_cast<void>(pairing_state->OnIoCapabilityRequest());
}
void IoCapabilityResponse(PairingState* pairing_state) {
  pairing_state->OnIoCapabilityResponse(kTestPeerIoCap);
}
void UserConfirmationRequest(PairingState* pairing_state) {
  pairing_state->OnUserConfirmationRequest(kTestPasskey,
                                           NoOpUserConfirmationCallback);
}
void UserPasskeyRequest(PairingState* pairing_state) {
  pairing_state->OnUserPasskeyRequest(NoOpUserPasskeyCallback);
}
void UserPasskeyNotification(PairingState* pairing_state) {
  pairing_state->OnUserPasskeyNotification(kTestPasskey);
}
void SimplePairingComplete(PairingState* pairing_state) {
  pairing_state->OnSimplePairingComplete(hci::StatusCode::kSuccess);
}
void LinkKeyNotification(PairingState* pairing_state) {
  pairing_state->OnLinkKeyNotification(kTestLinkKeyValue,
                                       kTestAuthenticatedLinkKeyType);
}
void AuthenticationComplete(PairingState* pairing_state) {
  pairing_state->OnAuthenticationComplete(hci::StatusCode::kSuccess);
}

// Test suite fixture that genericizes an injected pairing state event. The
// event being tested should be retrieved with the GetParam method, which
// returns a default event injector. For example:
//
//   PairingState pairing_state;
//   GetParam()(&pairing_state);
//
// This is named so that the instantiated test description looks correct:
//
//   GAP_PairingStateTest/HandlesEvent.<test case>/<index of event>
class HandlesEvent : public ::testing::TestWithParam<void (*)(PairingState*)> {
 protected:
  HandlesEvent() : pairing_state_(status_handler_.MakeStatusCallback()) {}
  ~HandlesEvent() = default;

  const TestStatusHandler& status_handler() const { return status_handler_; }
  PairingState& pairing_state() { return pairing_state_; }

  // Returns an event injector that was passed to INSTANTIATE_TEST_SUITE_P.
  auto* event() const { return GetParam(); }

  void InjectEvent() { event()(&pairing_state()); }

 private:
  TestStatusHandler status_handler_;
  PairingState pairing_state_;
};

// The tests here exercise that PairingState can be successfully advances
// through the expected pairing flow and generates errors when the pairing flow
// occurs out of order. This is intended to cover its internal state machine
// transitions and not the side effects.
INSTANTIATE_TEST_SUITE_P(
    GAP_PairingStateTest, HandlesEvent,
    ::testing::Values(IoCapabilityRequest, IoCapabilityResponse,
                      UserConfirmationRequest, UserPasskeyRequest,
                      UserPasskeyNotification, SimplePairingComplete,
                      LinkKeyNotification, AuthenticationComplete));

TEST_P(HandlesEvent, InIdleState) {
  RETURN_IF_FATAL(InjectEvent());
  if (event() == IoCapabilityResponse) {
    EXPECT_EQ(0, status_handler().call_count());
  } else {
    EXPECT_EQ(1, status_handler().call_count());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
  }
}

TEST_P(HandlesEvent, InInitiatorPairingStartedState) {
  // Advance state machine.
  static_cast<void>(pairing_state().InitiatePairing());

  RETURN_IF_FATAL(InjectEvent());
  if (event() == IoCapabilityRequest || event() == AuthenticationComplete) {
    EXPECT_EQ(0, status_handler().call_count());
  } else {
    EXPECT_EQ(1, status_handler().call_count());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
  }
}

TEST_P(HandlesEvent, InInitiatorWaitIoCapResponseState) {
  // Advance state machine.
  static_cast<void>(pairing_state().InitiatePairing());
  static_cast<void>(pairing_state().OnIoCapabilityRequest());

  RETURN_IF_FATAL(InjectEvent());
  if (event() == IoCapabilityResponse) {
    EXPECT_EQ(0, status_handler().call_count());
  } else {
    EXPECT_EQ(1, status_handler().call_count());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
  }
}

TEST_P(HandlesEvent, InResponderWaitIoCapRequestState) {
  // Advance state machine.
  pairing_state().OnIoCapabilityResponse(kTestPeerIoCap);

  RETURN_IF_FATAL(InjectEvent());
  if (event() == IoCapabilityRequest) {
    EXPECT_EQ(0, status_handler().call_count());
  } else {
    EXPECT_EQ(1, status_handler().call_count());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
  }
}

// TODO(xow): Split into three tests depending on the pairing event expected.
TEST_P(HandlesEvent, InWaitPairingEventStateAsInitiator) {
  // Advance state machine.
  static_cast<void>(pairing_state().InitiatePairing());
  static_cast<void>(pairing_state().OnIoCapabilityRequest());
  pairing_state().OnIoCapabilityResponse(kTestPeerIoCap);
  ASSERT_TRUE(pairing_state().initiator());

  RETURN_IF_FATAL(InjectEvent());
  if (event() == UserConfirmationRequest || event() == UserPasskeyRequest ||
      event() == UserPasskeyNotification) {
    EXPECT_EQ(0, status_handler().call_count());
  } else {
    EXPECT_EQ(1, status_handler().call_count());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
  }
}

// TODO(xow): Split into three tests depending on the pairing event expected.
TEST_P(HandlesEvent, InWaitPairingEventStateAsResponder) {
  // Advance state machine.
  pairing_state().OnIoCapabilityResponse(kTestPeerIoCap);
  static_cast<void>(pairing_state().OnIoCapabilityRequest());
  ASSERT_FALSE(pairing_state().initiator());

  RETURN_IF_FATAL(InjectEvent());
  if (event() == UserConfirmationRequest || event() == UserPasskeyRequest ||
      event() == UserPasskeyNotification) {
    EXPECT_EQ(0, status_handler().call_count());
  } else {
    EXPECT_EQ(1, status_handler().call_count());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
  }
}

TEST_P(HandlesEvent, InWaitPairingCompleteState) {
  // Advance state machine.
  pairing_state().OnIoCapabilityResponse(kTestPeerIoCap);
  static_cast<void>(pairing_state().OnIoCapabilityRequest());
  pairing_state().OnUserConfirmationRequest(kTestPasskey,
                                            NoOpUserConfirmationCallback);

  RETURN_IF_FATAL(InjectEvent());
  if (event() == SimplePairingComplete) {
    EXPECT_EQ(0, status_handler().call_count());
  } else {
    EXPECT_EQ(1, status_handler().call_count());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
  }
}

TEST_P(HandlesEvent, InWaitLinkKeyState) {
  // Advance state machine.
  pairing_state().OnIoCapabilityResponse(kTestPeerIoCap);
  static_cast<void>(pairing_state().OnIoCapabilityRequest());
  pairing_state().OnUserConfirmationRequest(kTestPasskey,
                                            NoOpUserConfirmationCallback);
  pairing_state().OnSimplePairingComplete(hci::StatusCode::kSuccess);

  RETURN_IF_FATAL(InjectEvent());
  if (event() == LinkKeyNotification) {
    EXPECT_EQ(0, status_handler().call_count());
  } else {
    EXPECT_EQ(1, status_handler().call_count());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
  }
}

TEST_P(HandlesEvent, InInitiatorWaitAuthCompleteState) {
  // Advance state machine.
  static_cast<void>(pairing_state().InitiatePairing());
  static_cast<void>(pairing_state().OnIoCapabilityRequest());
  pairing_state().OnIoCapabilityResponse(kTestPeerIoCap);
  pairing_state().OnUserConfirmationRequest(kTestPasskey,
                                            NoOpUserConfirmationCallback);
  pairing_state().OnSimplePairingComplete(hci::StatusCode::kSuccess);
  pairing_state().OnLinkKeyNotification(kTestLinkKeyValue,
                                        kTestAuthenticatedLinkKeyType);
  ASSERT_TRUE(pairing_state().initiator());

  RETURN_IF_FATAL(InjectEvent());
  if (event() == AuthenticationComplete) {
    EXPECT_EQ(0, status_handler().call_count());
  } else {
    EXPECT_EQ(1, status_handler().call_count());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
  }
}

TEST_P(HandlesEvent, InWaitEncryptionStateAsInitiator) {
  // Advance state machine.
  static_cast<void>(pairing_state().InitiatePairing());
  static_cast<void>(pairing_state().OnIoCapabilityRequest());
  pairing_state().OnIoCapabilityResponse(kTestPeerIoCap);
  pairing_state().OnUserConfirmationRequest(kTestPasskey,
                                            NoOpUserConfirmationCallback);
  pairing_state().OnSimplePairingComplete(hci::StatusCode::kSuccess);
  pairing_state().OnLinkKeyNotification(kTestLinkKeyValue,
                                        kTestAuthenticatedLinkKeyType);
  pairing_state().OnAuthenticationComplete(hci::StatusCode::kSuccess);
  ASSERT_TRUE(pairing_state().initiator());

  RETURN_IF_FATAL(InjectEvent());

  // Should not receive anything other than OnEncryptionChange.
  EXPECT_EQ(1, status_handler().call_count());
  ASSERT_TRUE(status_handler().status());
  EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
}

TEST_P(HandlesEvent, InWaitEncryptionStateAsResponder) {
  // Advance state machine.
  pairing_state().OnIoCapabilityResponse(kTestPeerIoCap);
  static_cast<void>(pairing_state().OnIoCapabilityRequest());
  pairing_state().OnUserConfirmationRequest(kTestPasskey,
                                            NoOpUserConfirmationCallback);
  pairing_state().OnSimplePairingComplete(hci::StatusCode::kSuccess);
  pairing_state().OnLinkKeyNotification(kTestLinkKeyValue,
                                        kTestAuthenticatedLinkKeyType);
  ASSERT_FALSE(pairing_state().initiator());

  RETURN_IF_FATAL(InjectEvent());

  // Should not receive anything other than OnEncryptionChange.
  EXPECT_EQ(1, status_handler().call_count());
  ASSERT_TRUE(status_handler().status());
  EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
}

TEST_P(HandlesEvent, InIdleStateAfterOnePairing) {
  // Advance state machine.
  static_cast<void>(pairing_state().InitiatePairing());
  static_cast<void>(pairing_state().OnIoCapabilityRequest());
  pairing_state().OnIoCapabilityResponse(kTestPeerIoCap);
  pairing_state().OnUserConfirmationRequest(kTestPasskey,
                                            NoOpUserConfirmationCallback);
  pairing_state().OnSimplePairingComplete(hci::StatusCode::kSuccess);
  pairing_state().OnLinkKeyNotification(kTestLinkKeyValue,
                                        kTestAuthenticatedLinkKeyType);
  pairing_state().OnAuthenticationComplete(hci::StatusCode::kSuccess);
  ASSERT_TRUE(pairing_state().initiator());

  // Successfully enabling encryption should allow pairing to start again.
  pairing_state().OnEncryptionChange(hci::Status(), true);
  EXPECT_FALSE(pairing_state().initiator());

  RETURN_IF_FATAL(InjectEvent());
  if (event() == IoCapabilityResponse) {
    EXPECT_EQ(0, status_handler().call_count());
  } else {
    EXPECT_EQ(1, status_handler().call_count());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
  }
}

// PairingAction expected answers are inferred from "device A" Authentication
// Stage 1 specs in v5.0 Vol 3, Part C, Sec 5.2.2.6, Table 5.7.
TEST(GAP_PairingStateTest, GetInitiatorPairingAction) {
  EXPECT_EQ(PairingAction::kAutomatic,
            GetInitiatorPairingAction(IOCapability::kDisplayOnly,
                                      IOCapability::kDisplayOnly));
  EXPECT_EQ(PairingAction::kDisplayPasskey,
            GetInitiatorPairingAction(IOCapability::kDisplayOnly,
                                      IOCapability::kDisplayYesNo));
  EXPECT_EQ(PairingAction::kDisplayPasskey,
            GetInitiatorPairingAction(IOCapability::kDisplayOnly,
                                      IOCapability::kKeyboardOnly));
  EXPECT_EQ(PairingAction::kAutomatic,
            GetInitiatorPairingAction(IOCapability::kDisplayOnly,
                                      IOCapability::kNoInputNoOutput));

  EXPECT_EQ(PairingAction::kComparePasskey,
            GetInitiatorPairingAction(IOCapability::kDisplayYesNo,
                                      IOCapability::kDisplayOnly));
  EXPECT_EQ(PairingAction::kDisplayPasskey,
            GetInitiatorPairingAction(IOCapability::kDisplayYesNo,
                                      IOCapability::kDisplayYesNo));
  EXPECT_EQ(PairingAction::kDisplayPasskey,
            GetInitiatorPairingAction(IOCapability::kDisplayYesNo,
                                      IOCapability::kKeyboardOnly));
  EXPECT_EQ(PairingAction::kGetConsent,
            GetInitiatorPairingAction(IOCapability::kDisplayYesNo,
                                      IOCapability::kNoInputNoOutput));

  EXPECT_EQ(PairingAction::kRequestPasskey,
            GetInitiatorPairingAction(IOCapability::kKeyboardOnly,
                                      IOCapability::kDisplayOnly));
  EXPECT_EQ(PairingAction::kRequestPasskey,
            GetInitiatorPairingAction(IOCapability::kKeyboardOnly,
                                      IOCapability::kDisplayYesNo));
  EXPECT_EQ(PairingAction::kRequestPasskey,
            GetInitiatorPairingAction(IOCapability::kKeyboardOnly,
                                      IOCapability::kKeyboardOnly));
  EXPECT_EQ(PairingAction::kAutomatic,
            GetInitiatorPairingAction(IOCapability::kKeyboardOnly,
                                      IOCapability::kNoInputNoOutput));

  EXPECT_EQ(PairingAction::kAutomatic,
            GetInitiatorPairingAction(IOCapability::kNoInputNoOutput,
                                      IOCapability::kDisplayOnly));
  EXPECT_EQ(PairingAction::kAutomatic,
            GetInitiatorPairingAction(IOCapability::kNoInputNoOutput,
                                      IOCapability::kDisplayYesNo));
  EXPECT_EQ(PairingAction::kAutomatic,
            GetInitiatorPairingAction(IOCapability::kNoInputNoOutput,
                                      IOCapability::kKeyboardOnly));
  EXPECT_EQ(PairingAction::kAutomatic,
            GetInitiatorPairingAction(IOCapability::kNoInputNoOutput,
                                      IOCapability::kNoInputNoOutput));
}

// Ibid., but for "device B."
TEST(GAP_PairingStateTest, GetResponderPairingAction) {
  EXPECT_EQ(PairingAction::kAutomatic,
            GetResponderPairingAction(IOCapability::kDisplayOnly,
                                      IOCapability::kDisplayOnly));
  EXPECT_EQ(PairingAction::kComparePasskey,
            GetResponderPairingAction(IOCapability::kDisplayOnly,
                                      IOCapability::kDisplayYesNo));
  EXPECT_EQ(PairingAction::kRequestPasskey,
            GetResponderPairingAction(IOCapability::kDisplayOnly,
                                      IOCapability::kKeyboardOnly));
  EXPECT_EQ(PairingAction::kAutomatic,
            GetResponderPairingAction(IOCapability::kDisplayOnly,
                                      IOCapability::kNoInputNoOutput));

  EXPECT_EQ(PairingAction::kDisplayPasskey,
            GetResponderPairingAction(IOCapability::kDisplayYesNo,
                                      IOCapability::kDisplayOnly));
  EXPECT_EQ(PairingAction::kComparePasskey,
            GetResponderPairingAction(IOCapability::kDisplayYesNo,
                                      IOCapability::kDisplayYesNo));
  EXPECT_EQ(PairingAction::kRequestPasskey,
            GetResponderPairingAction(IOCapability::kDisplayYesNo,
                                      IOCapability::kKeyboardOnly));
  EXPECT_EQ(PairingAction::kAutomatic,
            GetResponderPairingAction(IOCapability::kDisplayYesNo,
                                      IOCapability::kNoInputNoOutput));

  EXPECT_EQ(PairingAction::kDisplayPasskey,
            GetResponderPairingAction(IOCapability::kKeyboardOnly,
                                      IOCapability::kDisplayOnly));
  EXPECT_EQ(PairingAction::kDisplayPasskey,
            GetResponderPairingAction(IOCapability::kKeyboardOnly,
                                      IOCapability::kDisplayYesNo));
  EXPECT_EQ(PairingAction::kRequestPasskey,
            GetResponderPairingAction(IOCapability::kKeyboardOnly,
                                      IOCapability::kKeyboardOnly));
  EXPECT_EQ(PairingAction::kAutomatic,
            GetResponderPairingAction(IOCapability::kKeyboardOnly,
                                      IOCapability::kNoInputNoOutput));

  EXPECT_EQ(PairingAction::kAutomatic,
            GetResponderPairingAction(IOCapability::kNoInputNoOutput,
                                      IOCapability::kDisplayOnly));
  EXPECT_EQ(PairingAction::kGetConsent,
            GetResponderPairingAction(IOCapability::kNoInputNoOutput,
                                      IOCapability::kDisplayYesNo));
  EXPECT_EQ(PairingAction::kGetConsent,
            GetResponderPairingAction(IOCapability::kNoInputNoOutput,
                                      IOCapability::kKeyboardOnly));
  EXPECT_EQ(PairingAction::kAutomatic,
            GetResponderPairingAction(IOCapability::kNoInputNoOutput,
                                      IOCapability::kNoInputNoOutput));
}

// Events are obtained from ibid. association models, mapped to HCI sequences in
// v5.0 Vol 3, Vol 2, Part F, Sec 4.2.10–15.
TEST(GAP_PairingStateTest, GetExpectedEvent) {
  EXPECT_EQ(
      kUserConfirmationRequestEventCode,
      GetExpectedEvent(IOCapability::kDisplayOnly, IOCapability::kDisplayOnly));
  EXPECT_EQ(kUserConfirmationRequestEventCode,
            GetExpectedEvent(IOCapability::kDisplayOnly,
                             IOCapability::kDisplayYesNo));
  EXPECT_EQ(kUserPasskeyNotificationEventCode,
            GetExpectedEvent(IOCapability::kDisplayOnly,
                             IOCapability::kKeyboardOnly));
  EXPECT_EQ(kUserConfirmationRequestEventCode,
            GetExpectedEvent(IOCapability::kDisplayOnly,
                             IOCapability::kNoInputNoOutput));

  EXPECT_EQ(kUserConfirmationRequestEventCode,
            GetExpectedEvent(IOCapability::kDisplayYesNo,
                             IOCapability::kDisplayOnly));
  EXPECT_EQ(kUserConfirmationRequestEventCode,
            GetExpectedEvent(IOCapability::kDisplayYesNo,
                             IOCapability::kDisplayYesNo));
  EXPECT_EQ(kUserPasskeyNotificationEventCode,
            GetExpectedEvent(IOCapability::kDisplayYesNo,
                             IOCapability::kKeyboardOnly));
  EXPECT_EQ(kUserConfirmationRequestEventCode,
            GetExpectedEvent(IOCapability::kDisplayYesNo,
                             IOCapability::kNoInputNoOutput));

  EXPECT_EQ(kUserPasskeyRequestEventCode,
            GetExpectedEvent(IOCapability::kKeyboardOnly,
                             IOCapability::kDisplayOnly));
  EXPECT_EQ(kUserPasskeyRequestEventCode,
            GetExpectedEvent(IOCapability::kKeyboardOnly,
                             IOCapability::kDisplayYesNo));
  EXPECT_EQ(kUserPasskeyRequestEventCode,
            GetExpectedEvent(IOCapability::kKeyboardOnly,
                             IOCapability::kKeyboardOnly));
  EXPECT_EQ(kUserConfirmationRequestEventCode,
            GetExpectedEvent(IOCapability::kKeyboardOnly,
                             IOCapability::kNoInputNoOutput));

  EXPECT_EQ(kUserConfirmationRequestEventCode,
            GetExpectedEvent(IOCapability::kNoInputNoOutput,
                             IOCapability::kDisplayOnly));
  EXPECT_EQ(kUserConfirmationRequestEventCode,
            GetExpectedEvent(IOCapability::kNoInputNoOutput,
                             IOCapability::kDisplayYesNo));
  EXPECT_EQ(kUserConfirmationRequestEventCode,
            GetExpectedEvent(IOCapability::kNoInputNoOutput,
                             IOCapability::kKeyboardOnly));
  EXPECT_EQ(kUserConfirmationRequestEventCode,
            GetExpectedEvent(IOCapability::kNoInputNoOutput,
                             IOCapability::kNoInputNoOutput));
}

// Level of authentication from ibid. table.
TEST(GAP_PairingStateTest, IsPairingAuthenticated) {
  EXPECT_FALSE(IsPairingAuthenticated(IOCapability::kDisplayOnly,
                                      IOCapability::kDisplayOnly));
  EXPECT_FALSE(IsPairingAuthenticated(IOCapability::kDisplayOnly,
                                      IOCapability::kDisplayYesNo));
  EXPECT_TRUE(IsPairingAuthenticated(IOCapability::kDisplayOnly,
                                     IOCapability::kKeyboardOnly));
  EXPECT_FALSE(IsPairingAuthenticated(IOCapability::kDisplayOnly,
                                      IOCapability::kNoInputNoOutput));

  EXPECT_FALSE(IsPairingAuthenticated(IOCapability::kDisplayYesNo,
                                      IOCapability::kDisplayOnly));
  EXPECT_TRUE(IsPairingAuthenticated(IOCapability::kDisplayYesNo,
                                     IOCapability::kDisplayYesNo));
  EXPECT_TRUE(IsPairingAuthenticated(IOCapability::kDisplayYesNo,
                                     IOCapability::kKeyboardOnly));
  EXPECT_FALSE(IsPairingAuthenticated(IOCapability::kDisplayYesNo,
                                      IOCapability::kNoInputNoOutput));

  EXPECT_TRUE(IsPairingAuthenticated(IOCapability::kKeyboardOnly,
                                     IOCapability::kDisplayOnly));
  EXPECT_TRUE(IsPairingAuthenticated(IOCapability::kKeyboardOnly,
                                     IOCapability::kDisplayYesNo));
  EXPECT_TRUE(IsPairingAuthenticated(IOCapability::kKeyboardOnly,
                                     IOCapability::kKeyboardOnly));
  EXPECT_FALSE(IsPairingAuthenticated(IOCapability::kKeyboardOnly,
                                      IOCapability::kNoInputNoOutput));

  EXPECT_FALSE(IsPairingAuthenticated(IOCapability::kNoInputNoOutput,
                                      IOCapability::kDisplayOnly));
  EXPECT_FALSE(IsPairingAuthenticated(IOCapability::kNoInputNoOutput,
                                      IOCapability::kDisplayYesNo));
  EXPECT_FALSE(IsPairingAuthenticated(IOCapability::kNoInputNoOutput,
                                      IOCapability::kKeyboardOnly));
  EXPECT_FALSE(IsPairingAuthenticated(IOCapability::kNoInputNoOutput,
                                      IOCapability::kNoInputNoOutput));
}

TEST(GAP_PairingStateTest, GetInitiatorAuthRequirements) {
  EXPECT_EQ(AuthRequirements::kMITMGeneralBonding,
            GetInitiatorAuthRequirements(IOCapability::kDisplayOnly));
  EXPECT_EQ(AuthRequirements::kMITMGeneralBonding,
            GetInitiatorAuthRequirements(IOCapability::kDisplayYesNo));
  EXPECT_EQ(AuthRequirements::kMITMGeneralBonding,
            GetInitiatorAuthRequirements(IOCapability::kKeyboardOnly));
  EXPECT_EQ(AuthRequirements::kGeneralBonding,
            GetInitiatorAuthRequirements(IOCapability::kNoInputNoOutput));
}

TEST(GAP_PairingStateTest, GetResponderAuthRequirements) {
  EXPECT_EQ(AuthRequirements::kGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kDisplayOnly,
                                         IOCapability::kDisplayOnly));
  EXPECT_EQ(AuthRequirements::kGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kDisplayOnly,
                                         IOCapability::kDisplayYesNo));
  EXPECT_EQ(AuthRequirements::kMITMGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kDisplayOnly,
                                         IOCapability::kKeyboardOnly));
  EXPECT_EQ(AuthRequirements::kGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kDisplayOnly,
                                         IOCapability::kNoInputNoOutput));

  EXPECT_EQ(AuthRequirements::kGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kDisplayYesNo,
                                         IOCapability::kDisplayOnly));
  EXPECT_EQ(AuthRequirements::kMITMGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kDisplayYesNo,
                                         IOCapability::kDisplayYesNo));
  EXPECT_EQ(AuthRequirements::kMITMGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kDisplayYesNo,
                                         IOCapability::kKeyboardOnly));
  EXPECT_EQ(AuthRequirements::kGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kDisplayYesNo,
                                         IOCapability::kNoInputNoOutput));

  EXPECT_EQ(AuthRequirements::kMITMGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kKeyboardOnly,
                                         IOCapability::kDisplayOnly));
  EXPECT_EQ(AuthRequirements::kMITMGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kKeyboardOnly,
                                         IOCapability::kDisplayYesNo));
  EXPECT_EQ(AuthRequirements::kMITMGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kKeyboardOnly,
                                         IOCapability::kKeyboardOnly));
  EXPECT_EQ(AuthRequirements::kGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kKeyboardOnly,
                                         IOCapability::kNoInputNoOutput));

  EXPECT_EQ(AuthRequirements::kGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kNoInputNoOutput,
                                         IOCapability::kDisplayOnly));
  EXPECT_EQ(AuthRequirements::kGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kNoInputNoOutput,
                                         IOCapability::kDisplayYesNo));
  EXPECT_EQ(AuthRequirements::kGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kNoInputNoOutput,
                                         IOCapability::kKeyboardOnly));
  EXPECT_EQ(AuthRequirements::kGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kNoInputNoOutput,
                                         IOCapability::kNoInputNoOutput));
}

}  // namespace
}  // namespace gap
}  // namespace bt
