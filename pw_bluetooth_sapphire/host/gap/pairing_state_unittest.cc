// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/pairing_state.h"

#include "gtest/gtest.h"
#include "src/connectivity/bluetooth/core/bt-host/common/random.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/fake_connection.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"

namespace bt {
namespace gap {
namespace {

using hci::AuthRequirements;
using hci::IOCapability;
using hci::kUserConfirmationRequestEventCode;
using hci::kUserPasskeyNotificationEventCode;
using hci::kUserPasskeyRequestEventCode;
using hci::testing::FakeConnection;

const PeerId kTestPeerId(0xf00f1becf00fface);
const hci::ConnectionHandle kTestHandle(0x0A0B);
const DeviceAddress kLocalAddress(DeviceAddress::Type::kBREDR,
                                  {0x22, 0x11, 0x00, 0xCC, 0xBB, 0xAA});
const DeviceAddress kPeerAddress(DeviceAddress::Type::kBREDR, {0x99, 0x88, 0x77, 0xFF, 0xEE, 0xDD});
const auto kTestLocalIoCap = sm::IOCapability::kDisplayYesNo;
const auto kTestPeerIoCap = IOCapability::kDisplayOnly;
const uint32_t kTestPasskey = 123456;
const auto kTestLinkKeyValue = Random<UInt128>();
const auto kTestUnauthenticatedLinkKeyType = hci::LinkKeyType::kUnauthenticatedCombination192;
const auto kTestAuthenticatedLinkKeyType = hci::LinkKeyType::kAuthenticatedCombination192;
const auto kTestLegacyLinkKeyType = hci::LinkKeyType::kCombination;

void NoOpStatusCallback(hci::ConnectionHandle, hci::Status){};
void NoOpUserConfirmationCallback(bool){};
void NoOpUserPasskeyCallback(std::optional<uint32_t>){};

class NoOpPairingDelegate final : public PairingDelegate {
 public:
  NoOpPairingDelegate(sm::IOCapability io_capability)
      : io_capability_(io_capability), weak_ptr_factory_(this) {}

  fxl::WeakPtr<NoOpPairingDelegate> GetWeakPtr() { return weak_ptr_factory_.GetWeakPtr(); }

  // PairingDelegate overrides that do nothing.
  ~NoOpPairingDelegate() override = default;
  sm::IOCapability io_capability() const override { return io_capability_; }
  void CompletePairing(PeerId peer_id, sm::Status status) override {}
  void ConfirmPairing(PeerId peer_id, ConfirmCallback confirm) override {}
  void DisplayPasskey(PeerId peer_id, uint32_t passkey, DisplayMethod method,
                      ConfirmCallback confirm) override {}
  void RequestPasskey(PeerId peer_id, PasskeyResponseCallback respond) override {}

 private:
  const sm::IOCapability io_capability_;
  fxl::WeakPtrFactory<NoOpPairingDelegate> weak_ptr_factory_;
};

FakeConnection MakeFakeConnection() {
  return FakeConnection(kTestHandle, hci::Connection::LinkType::kACL,
                        hci::Connection::Role::kMaster, kLocalAddress, kPeerAddress);
}

TEST(GAP_PairingStateTest, PairingStateStartsAsResponder) {
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, NoOpStatusCallback);
  EXPECT_FALSE(pairing_state.initiator());
}

TEST(GAP_PairingStateTest, PairingStateRemainsResponderAfterPeerIoCapResponse) {
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, NoOpStatusCallback);
  pairing_state.OnIoCapabilityResponse(kTestPeerIoCap);
  EXPECT_FALSE(pairing_state.initiator());
}

TEST(GAP_PairingStateTest, PairingStateBecomesInitiatorAfterLocalPairingInitiated) {
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, NoOpStatusCallback);
  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());
  EXPECT_EQ(PairingState::InitiatorAction::kSendAuthenticationRequest,
            pairing_state.InitiatePairing(NoOpStatusCallback));
  EXPECT_TRUE(pairing_state.initiator());
}

TEST(GAP_PairingStateTest, PairingStateSendsAuthenticationRequestExactlyOnce) {
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, NoOpStatusCallback);
  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());
  EXPECT_EQ(PairingState::InitiatorAction::kSendAuthenticationRequest,
            pairing_state.InitiatePairing(NoOpStatusCallback));
  EXPECT_TRUE(pairing_state.initiator());

  EXPECT_EQ(PairingState::InitiatorAction::kDoNotSendAuthenticationRequest,
            pairing_state.InitiatePairing(NoOpStatusCallback));
  EXPECT_TRUE(pairing_state.initiator());
}

TEST(GAP_PairingStateTest,
     PairingStateRemainsResponderIfPairingInitiatedWhileResponderPairingInProgress) {
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, NoOpStatusCallback);
  pairing_state.OnIoCapabilityResponse(kTestPeerIoCap);
  ASSERT_FALSE(pairing_state.initiator());

  EXPECT_EQ(PairingState::InitiatorAction::kDoNotSendAuthenticationRequest,
            pairing_state.InitiatePairing(NoOpStatusCallback));
  EXPECT_FALSE(pairing_state.initiator());
}

TEST(GAP_PairingStateTest, StatusCallbackMayDestroyPairingState) {
  auto connection = MakeFakeConnection();
  std::unique_ptr<PairingState> pairing_state;
  bool cb_called = false;
  auto status_cb = [&pairing_state, &cb_called](hci::ConnectionHandle handle, hci::Status status) {
    EXPECT_FALSE(status.is_success());
    cb_called = true;

    // Note that this lambda is owned by the PairingState so its captures are invalid after this.
    pairing_state = nullptr;
  };
  pairing_state = std::make_unique<PairingState>(kTestPeerId, &connection, status_cb);

  // Unexpected event that should cause the status callback to be called with an error.
  pairing_state->OnUserPasskeyNotification(kTestPasskey);

  EXPECT_TRUE(cb_called);
}

TEST(GAP_PairingStateTest, InitiatorCallbackMayDestroyPairingState) {
  auto connection = MakeFakeConnection();
  std::unique_ptr<PairingState> pairing_state =
      std::make_unique<PairingState>(kTestPeerId, &connection, NoOpStatusCallback);
  bool cb_called = false;
  auto status_cb = [&pairing_state, &cb_called](hci::ConnectionHandle handle, hci::Status status) {
    EXPECT_FALSE(status.is_success());
    cb_called = true;

    // Note that this lambda is owned by the PairingState so its captures are invalid after this.
    pairing_state = nullptr;
  };
  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state->SetPairingDelegate(pairing_delegate.GetWeakPtr());
  static_cast<void>(pairing_state->InitiatePairing(status_cb));

  // Unexpected event that should cause the status callback to be called with an error.
  pairing_state->OnUserPasskeyNotification(kTestPasskey);

  EXPECT_TRUE(cb_called);
}

// Test helper to inspect StatusCallback invocations.
class TestStatusHandler final {
 public:
  auto MakeStatusCallback() {
    return [this](hci::ConnectionHandle handle, hci::Status status) {
      call_count_++;
      handle_ = handle;
      status_ = status;
    };
  }

  auto call_count() const { return call_count_; }

  // Returns std::nullopt if |call_count() < 1|, otherwise values from the most
  // recent callback invocation.
  auto& handle() const { return handle_; }
  auto& status() const { return status_; }

 private:
  int call_count_ = 0;
  std::optional<hci::ConnectionHandle> handle_;
  std::optional<hci::Status> status_;
};

TEST(GAP_PairingStateTest, TestStatusHandlerTracksStatusCallbackInvocations) {
  TestStatusHandler handler;
  EXPECT_EQ(0, handler.call_count());
  EXPECT_FALSE(handler.status());

  PairingState::StatusCallback status_cb = handler.MakeStatusCallback();
  EXPECT_EQ(0, handler.call_count());
  EXPECT_FALSE(handler.status());

  status_cb(hci::ConnectionHandle(0x0A0B), hci::Status(hci::StatusCode::kPairingNotAllowed));
  EXPECT_EQ(1, handler.call_count());
  ASSERT_TRUE(handler.handle());
  EXPECT_EQ(hci::ConnectionHandle(0x0A0B), *handler.handle());
  ASSERT_TRUE(handler.status());
  EXPECT_EQ(hci::Status(hci::StatusCode::kPairingNotAllowed), *handler.status());
}

TEST(GAP_PairingStateTest, InitiatingPairingAfterErrorTriggersStatusCallbackWithError) {
  TestStatusHandler link_status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, link_status_handler.MakeStatusCallback());

  // Unexpected event that should cause the status callback to be called with an error.
  pairing_state.OnUserPasskeyNotification(kTestPasskey);

  EXPECT_EQ(1, link_status_handler.call_count());
  ASSERT_TRUE(link_status_handler.handle());
  EXPECT_EQ(kTestHandle, *link_status_handler.handle());
  ASSERT_TRUE(link_status_handler.status());
  EXPECT_EQ(hci::Status(HostError::kNotSupported), *link_status_handler.status());

  // Try to initiate pairing again.
  TestStatusHandler pairing_status_handler;
  static_cast<void>(pairing_state.InitiatePairing(pairing_status_handler.MakeStatusCallback()));

  // The status callback for this attempt should be notified with an error.
  EXPECT_EQ(1, pairing_status_handler.call_count());
  ASSERT_TRUE(pairing_status_handler.handle());
  EXPECT_EQ(kTestHandle, *pairing_status_handler.handle());
  ASSERT_TRUE(pairing_status_handler.status());
  EXPECT_EQ(hci::Status(HostError::kCanceled), *pairing_status_handler.status());
}

TEST(GAP_PairingStateTest, UnexpectedEncryptionChangeDoesNotTriggerStatusCallback) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, status_handler.MakeStatusCallback());
  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  static_cast<void>(pairing_state.InitiatePairing(NoOpStatusCallback));
  static_cast<void>(pairing_state.OnIoCapabilityRequest());
  pairing_state.OnIoCapabilityResponse(kTestPeerIoCap);

  ASSERT_EQ(0, connection.start_encryption_count());
  ASSERT_EQ(0, status_handler.call_count());

  connection.TriggerEncryptionChangeCallback(hci::Status(), true);
  EXPECT_EQ(0, status_handler.call_count());
}

// Inject events that occur during the course of a successful pairing as an initiator, but not
// including enabling link encryption.
void AdvanceToEncryptionAsInitiator(PairingState* pairing_state) {
  static_cast<void>(pairing_state->OnIoCapabilityRequest());
  pairing_state->OnIoCapabilityResponse(kTestPeerIoCap);
  pairing_state->OnUserConfirmationRequest(kTestPasskey, NoOpUserConfirmationCallback);
  pairing_state->OnSimplePairingComplete(hci::StatusCode::kSuccess);
  pairing_state->OnLinkKeyNotification(kTestLinkKeyValue, kTestUnauthenticatedLinkKeyType);
  pairing_state->OnAuthenticationComplete(hci::StatusCode::kSuccess);
}

TEST(GAP_PairingStateTest, SuccessfulEncryptionChangeTriggersStatusCallback) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, status_handler.MakeStatusCallback());
  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  static_cast<void>(pairing_state.InitiatePairing(NoOpStatusCallback));
  AdvanceToEncryptionAsInitiator(&pairing_state);

  ASSERT_EQ(0, status_handler.call_count());

  EXPECT_EQ(1, connection.start_encryption_count());
  connection.TriggerEncryptionChangeCallback(hci::Status(), true);
  EXPECT_EQ(1, status_handler.call_count());
  ASSERT_TRUE(status_handler.handle());
  EXPECT_EQ(kTestHandle, *status_handler.handle());
  ASSERT_TRUE(status_handler.status());
  EXPECT_EQ(hci::Status(), *status_handler.status());
}

TEST(GAP_PairingStateTest, EncryptionChangeErrorTriggersStatusCallbackWithError) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, status_handler.MakeStatusCallback());
  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  static_cast<void>(pairing_state.InitiatePairing(NoOpStatusCallback));
  AdvanceToEncryptionAsInitiator(&pairing_state);

  ASSERT_EQ(0, status_handler.call_count());

  EXPECT_EQ(1, connection.start_encryption_count());
  connection.TriggerEncryptionChangeCallback(hci::Status(HostError::kInsufficientSecurity), false);
  EXPECT_EQ(1, status_handler.call_count());
  ASSERT_TRUE(status_handler.handle());
  EXPECT_EQ(kTestHandle, *status_handler.handle());
  ASSERT_TRUE(status_handler.status());
  EXPECT_EQ(hci::Status(HostError::kInsufficientSecurity), *status_handler.status());
}

TEST(GAP_PairingStateTest, EncryptionChangeToDisabledTriggersStatusCallbackWithError) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, status_handler.MakeStatusCallback());
  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  static_cast<void>(pairing_state.InitiatePairing(NoOpStatusCallback));
  AdvanceToEncryptionAsInitiator(&pairing_state);

  ASSERT_EQ(0, status_handler.call_count());

  EXPECT_EQ(1, connection.start_encryption_count());
  connection.TriggerEncryptionChangeCallback(hci::Status(), false);
  EXPECT_EQ(1, status_handler.call_count());
  ASSERT_TRUE(status_handler.handle());
  EXPECT_EQ(kTestHandle, *status_handler.handle());
  ASSERT_TRUE(status_handler.status());
  EXPECT_EQ(hci::Status(HostError::kFailed), *status_handler.status());
}

TEST(GAP_PairingStateTest, EncryptionChangeToEnableCallsInitiatorCallbacks) {
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, NoOpStatusCallback);
  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  TestStatusHandler status_handler_0;
  static_cast<void>(pairing_state.InitiatePairing(status_handler_0.MakeStatusCallback()));
  AdvanceToEncryptionAsInitiator(&pairing_state);
  EXPECT_TRUE(pairing_state.initiator());

  // Try to initiate pairing while pairing is in progress.
  TestStatusHandler status_handler_1;
  static_cast<void>(pairing_state.InitiatePairing(status_handler_1.MakeStatusCallback()));

  EXPECT_TRUE(pairing_state.initiator());
  ASSERT_EQ(0, status_handler_0.call_count());
  ASSERT_EQ(0, status_handler_1.call_count());

  connection.TriggerEncryptionChangeCallback(hci::Status(), true);
  EXPECT_EQ(1, status_handler_0.call_count());
  EXPECT_EQ(1, status_handler_1.call_count());
  ASSERT_TRUE(status_handler_0.handle());
  EXPECT_EQ(kTestHandle, *status_handler_0.handle());
  ASSERT_TRUE(status_handler_0.status());
  EXPECT_EQ(hci::Status(), *status_handler_0.status());
  ASSERT_TRUE(status_handler_1.handle());
  EXPECT_EQ(kTestHandle, *status_handler_1.handle());
  ASSERT_TRUE(status_handler_1.status());
  EXPECT_EQ(hci::Status(), *status_handler_1.status());

  // Errors for a new pairing shouldn't invoke the initiators' callbacks.
  pairing_state.OnUserPasskeyNotification(kTestPasskey);
  EXPECT_EQ(1, status_handler_0.call_count());
  EXPECT_EQ(1, status_handler_1.call_count());
}

TEST(GAP_PairingStateTest, InitiatingPairingOnResponderWaitsForPairingToFinish) {
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, NoOpStatusCallback);
  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine as pairing responder.
  pairing_state.OnIoCapabilityResponse(kTestPeerIoCap);
  ASSERT_FALSE(pairing_state.initiator());
  static_cast<void>(pairing_state.OnIoCapabilityRequest());
  pairing_state.OnUserConfirmationRequest(kTestPasskey, NoOpUserConfirmationCallback);

  // Try to initiate pairing while pairing is in progress.
  TestStatusHandler status_handler;
  static_cast<void>(pairing_state.InitiatePairing(status_handler.MakeStatusCallback()));
  EXPECT_FALSE(pairing_state.initiator());

  // Keep advancing state machine.
  pairing_state.OnSimplePairingComplete(hci::StatusCode::kSuccess);
  pairing_state.OnLinkKeyNotification(kTestLinkKeyValue, kTestUnauthenticatedLinkKeyType);

  EXPECT_FALSE(pairing_state.initiator());
  ASSERT_EQ(0, status_handler.call_count());

  // The attempt to initiate pairing should have its status callback notified.
  connection.TriggerEncryptionChangeCallback(hci::Status(), true);
  EXPECT_EQ(1, status_handler.call_count());
  ASSERT_TRUE(status_handler.handle());
  EXPECT_EQ(kTestHandle, *status_handler.handle());
  ASSERT_TRUE(status_handler.status());
  EXPECT_EQ(hci::Status(), *status_handler.status());

  // Errors for a new pairing shouldn't invoke the attempted initiator's callback.
  pairing_state.OnUserPasskeyNotification(kTestPasskey);
  EXPECT_EQ(1, status_handler.call_count());
}

TEST(GAP_PairingStateTest, PairingStateRejectsPairingInitiationWithoutPairingDelegate) {
  auto connection = MakeFakeConnection();
  TestStatusHandler owner_status_handler;
  PairingState pairing_state(kTestPeerId, &connection, owner_status_handler.MakeStatusCallback());

  TestStatusHandler initiator_status_handler;
  EXPECT_EQ(PairingState::InitiatorAction::kDoNotSendAuthenticationRequest,
            pairing_state.InitiatePairing(initiator_status_handler.MakeStatusCallback()));
  EXPECT_FALSE(pairing_state.initiator());

  // The owning link doesn't get an error for a failure to initiator pairing.
  EXPECT_EQ(0, owner_status_handler.call_count());

  // But the initiator should get an error.
  EXPECT_EQ(1, initiator_status_handler.call_count());
  ASSERT_TRUE(initiator_status_handler.status());
  EXPECT_EQ(hci::Status(HostError::kNotReady), *initiator_status_handler.status());
}

TEST(GAP_PairingStateTest, PairingStateRejectsPairingResponseWithoutPairingDelegate) {
  auto connection = MakeFakeConnection();
  TestStatusHandler status_handler;
  PairingState pairing_state(kTestPeerId, &connection, status_handler.MakeStatusCallback());
  pairing_state.OnIoCapabilityResponse(kTestPeerIoCap);

  // PairingState rejects the pairing with a negative reply.
  EXPECT_FALSE(pairing_state.OnIoCapabilityRequest());
  EXPECT_FALSE(pairing_state.initiator());

  // The owner should be alerted with an error.
  EXPECT_EQ(1, status_handler.call_count());
  ASSERT_TRUE(status_handler.status());
  EXPECT_EQ(hci::Status(HostError::kNotReady), *status_handler.status());
}

TEST(GAP_PairingStateTest, UnexpectedLinkKeyAuthenticationRaisesError) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, status_handler.MakeStatusCallback());
  NoOpPairingDelegate pairing_delegate(sm::IOCapability::kDisplayOnly);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  pairing_state.OnIoCapabilityResponse(IOCapability::kDisplayYesNo);
  ASSERT_FALSE(pairing_state.initiator());
  static_cast<void>(pairing_state.OnIoCapabilityRequest());
  pairing_state.OnUserConfirmationRequest(kTestPasskey, NoOpUserConfirmationCallback);
  pairing_state.OnSimplePairingComplete(hci::StatusCode::kSuccess);

  // Provide an authenticated link key when this should have resulted in an
  // unauthenticated link key.
  pairing_state.OnLinkKeyNotification(kTestLinkKeyValue, kTestAuthenticatedLinkKeyType);

  EXPECT_EQ(1, status_handler.call_count());
  ASSERT_TRUE(status_handler.handle());
  EXPECT_EQ(kTestHandle, *status_handler.handle());
  ASSERT_TRUE(status_handler.status());
  EXPECT_EQ(hci::Status(HostError::kInsufficientSecurity), *status_handler.status());
}

TEST(GAP_PairingStateTest, LegacyPairingLinkKeyRaisesError) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, status_handler.MakeStatusCallback());
  NoOpPairingDelegate pairing_delegate(sm::IOCapability::kNoInputNoOutput);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  pairing_state.OnIoCapabilityResponse(IOCapability::kDisplayYesNo);
  ASSERT_FALSE(pairing_state.initiator());
  static_cast<void>(pairing_state.OnIoCapabilityRequest());
  pairing_state.OnUserConfirmationRequest(kTestPasskey, NoOpUserConfirmationCallback);
  pairing_state.OnSimplePairingComplete(hci::StatusCode::kSuccess);

  // Provide a legacy pairing link key type.
  pairing_state.OnLinkKeyNotification(kTestLinkKeyValue, kTestLegacyLinkKeyType);

  EXPECT_EQ(1, status_handler.call_count());
  ASSERT_TRUE(status_handler.handle());
  EXPECT_EQ(kTestHandle, *status_handler.handle());
  ASSERT_TRUE(status_handler.status());
  EXPECT_EQ(hci::Status(HostError::kInsufficientSecurity), *status_handler.status());
}

TEST(GAP_PairingStateTest, PairingSetsConnectionLinkKey) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, status_handler.MakeStatusCallback());
  NoOpPairingDelegate pairing_delegate(sm::IOCapability::kNoInputNoOutput);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  pairing_state.OnIoCapabilityResponse(IOCapability::kDisplayYesNo);
  ASSERT_FALSE(pairing_state.initiator());
  static_cast<void>(pairing_state.OnIoCapabilityRequest());
  pairing_state.OnUserConfirmationRequest(kTestPasskey, NoOpUserConfirmationCallback);
  pairing_state.OnSimplePairingComplete(hci::StatusCode::kSuccess);

  ASSERT_FALSE(connection.ltk());
  pairing_state.OnLinkKeyNotification(kTestLinkKeyValue, kTestUnauthenticatedLinkKeyType);
  ASSERT_TRUE(connection.ltk());
  EXPECT_EQ(kTestLinkKeyValue, connection.ltk()->value());

  EXPECT_EQ(0, status_handler.call_count());
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
  pairing_state->OnUserConfirmationRequest(kTestPasskey, NoOpUserConfirmationCallback);
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
  pairing_state->OnLinkKeyNotification(kTestLinkKeyValue, kTestUnauthenticatedLinkKeyType);
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
  HandlesEvent()
      : connection_(MakeFakeConnection()),
        pairing_delegate_(kTestLocalIoCap),
        pairing_state_(kTestPeerId, &connection_, status_handler_.MakeStatusCallback()) {
    // TODO(xow): Ignore PairingDelegate method matches for these tests.
    pairing_state().SetPairingDelegate(pairing_delegate_.GetWeakPtr());
  }
  ~HandlesEvent() = default;

  const FakeConnection& connection() const { return connection_; }
  const TestStatusHandler& status_handler() const { return status_handler_; }
  PairingState& pairing_state() { return pairing_state_; }

  // Returns an event injector that was passed to INSTANTIATE_TEST_SUITE_P.
  auto* event() const { return GetParam(); }

  void InjectEvent() { event()(&pairing_state()); }

 private:
  FakeConnection connection_;
  TestStatusHandler status_handler_;
  NoOpPairingDelegate pairing_delegate_;
  PairingState pairing_state_;
};

// The tests here exercise that PairingState can be successfully advances
// through the expected pairing flow and generates errors when the pairing flow
// occurs out of order. This is intended to cover its internal state machine
// transitions and not the side effects.
INSTANTIATE_TEST_SUITE_P(GAP_PairingStateTest, HandlesEvent,
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
    ASSERT_TRUE(status_handler().handle());
    EXPECT_EQ(kTestHandle, *status_handler().handle());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
  }
}

TEST_P(HandlesEvent, InInitiatorPairingStartedState) {
  // Advance state machine.
  static_cast<void>(pairing_state().InitiatePairing(NoOpStatusCallback));

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
  static_cast<void>(pairing_state().InitiatePairing(NoOpStatusCallback));
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

TEST_P(HandlesEvent, InErrorStateAfterIoCapRequestRejectedWithoutPairingDelegate) {
  // Clear the default pairing delegate set by the fixture.
  pairing_state().SetPairingDelegate(fxl::WeakPtr<PairingDelegate>());

  // Advance state machine.
  pairing_state().OnIoCapabilityResponse(kTestPeerIoCap);
  EXPECT_FALSE(pairing_state().OnIoCapabilityRequest());

  // PairingState no longer accepts events because being not ready to pair has raised an error.
  RETURN_IF_FATAL(InjectEvent());
  EXPECT_LE(1, status_handler().call_count());
  ASSERT_TRUE(status_handler().status());
  EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
}

TEST_P(HandlesEvent, InWaitUserConfirmationStateAsInitiator) {
  NoOpPairingDelegate pairing_delegate(sm::IOCapability::kDisplayOnly);
  pairing_state().SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  static_cast<void>(pairing_state().InitiatePairing(NoOpStatusCallback));
  static_cast<void>(pairing_state().OnIoCapabilityRequest());
  pairing_state().OnIoCapabilityResponse(IOCapability::kDisplayYesNo);
  ASSERT_TRUE(pairing_state().initiator());

  RETURN_IF_FATAL(InjectEvent());
  if (event() == UserConfirmationRequest) {
    EXPECT_EQ(0, status_handler().call_count());
  } else {
    EXPECT_EQ(1, status_handler().call_count());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
  }
}

TEST_P(HandlesEvent, InWaitUserPasskeyRequestStateAsInitiator) {
  NoOpPairingDelegate pairing_delegate(sm::IOCapability::kKeyboardOnly);
  pairing_state().SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  static_cast<void>(pairing_state().InitiatePairing(NoOpStatusCallback));
  static_cast<void>(pairing_state().OnIoCapabilityRequest());
  pairing_state().OnIoCapabilityResponse(IOCapability::kDisplayOnly);
  ASSERT_TRUE(pairing_state().initiator());

  RETURN_IF_FATAL(InjectEvent());
  if (event() == UserPasskeyRequest) {
    EXPECT_EQ(0, status_handler().call_count());
  } else {
    EXPECT_EQ(1, status_handler().call_count());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
  }
}

TEST_P(HandlesEvent, InWaitUserPasskeyNotificationStateAsInitiator) {
  NoOpPairingDelegate pairing_delegate(sm::IOCapability::kDisplayOnly);
  pairing_state().SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  static_cast<void>(pairing_state().InitiatePairing(NoOpStatusCallback));
  static_cast<void>(pairing_state().OnIoCapabilityRequest());
  pairing_state().OnIoCapabilityResponse(IOCapability::kKeyboardOnly);
  ASSERT_TRUE(pairing_state().initiator());

  RETURN_IF_FATAL(InjectEvent());
  if (event() == UserPasskeyNotification) {
    EXPECT_EQ(0, status_handler().call_count());
  } else {
    EXPECT_EQ(1, status_handler().call_count());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
  }
}

// TODO(xow): Split into three tests depending on the pairing event expected.
TEST_P(HandlesEvent, InWaitUserConfirmationStateAsResponder) {
  NoOpPairingDelegate pairing_delegate(sm::IOCapability::kDisplayOnly);
  pairing_state().SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  pairing_state().OnIoCapabilityResponse(IOCapability::kDisplayYesNo);
  static_cast<void>(pairing_state().OnIoCapabilityRequest());
  ASSERT_FALSE(pairing_state().initiator());

  RETURN_IF_FATAL(InjectEvent());
  if (event() == UserConfirmationRequest) {
    EXPECT_EQ(0, status_handler().call_count());
  } else {
    EXPECT_EQ(1, status_handler().call_count());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
  }
}

TEST_P(HandlesEvent, InWaitUserPasskeyRequestStateAsResponder) {
  NoOpPairingDelegate pairing_delegate(sm::IOCapability::kKeyboardOnly);
  pairing_state().SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  pairing_state().OnIoCapabilityResponse(IOCapability::kDisplayOnly);
  static_cast<void>(pairing_state().OnIoCapabilityRequest());
  ASSERT_FALSE(pairing_state().initiator());

  RETURN_IF_FATAL(InjectEvent());
  if (event() == UserPasskeyRequest) {
    EXPECT_EQ(0, status_handler().call_count());
  } else {
    EXPECT_EQ(1, status_handler().call_count());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
  }
}

TEST_P(HandlesEvent, InWaitUserNotificationStateAsResponder) {
  NoOpPairingDelegate pairing_delegate(sm::IOCapability::kDisplayOnly);
  pairing_state().SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  pairing_state().OnIoCapabilityResponse(IOCapability::kKeyboardOnly);
  static_cast<void>(pairing_state().OnIoCapabilityRequest());
  ASSERT_FALSE(pairing_state().initiator());

  RETURN_IF_FATAL(InjectEvent());
  if (event() == UserPasskeyNotification) {
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
  pairing_state().OnUserConfirmationRequest(kTestPasskey, NoOpUserConfirmationCallback);

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
  pairing_state().OnUserConfirmationRequest(kTestPasskey, NoOpUserConfirmationCallback);
  pairing_state().OnSimplePairingComplete(hci::StatusCode::kSuccess);
  EXPECT_EQ(0, connection().start_encryption_count());

  RETURN_IF_FATAL(InjectEvent());
  if (event() == LinkKeyNotification) {
    EXPECT_EQ(0, status_handler().call_count());
    EXPECT_EQ(1, connection().start_encryption_count());
  } else {
    EXPECT_EQ(1, status_handler().call_count());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
  }
}

TEST_P(HandlesEvent, InInitiatorWaitAuthCompleteState) {
  // Advance state machine.
  static_cast<void>(pairing_state().InitiatePairing(NoOpStatusCallback));
  static_cast<void>(pairing_state().OnIoCapabilityRequest());
  pairing_state().OnIoCapabilityResponse(kTestPeerIoCap);
  pairing_state().OnUserConfirmationRequest(kTestPasskey, NoOpUserConfirmationCallback);
  pairing_state().OnSimplePairingComplete(hci::StatusCode::kSuccess);
  pairing_state().OnLinkKeyNotification(kTestLinkKeyValue, kTestUnauthenticatedLinkKeyType);
  ASSERT_TRUE(pairing_state().initiator());
  EXPECT_EQ(0, connection().start_encryption_count());

  RETURN_IF_FATAL(InjectEvent());
  if (event() == AuthenticationComplete) {
    EXPECT_EQ(0, status_handler().call_count());
    EXPECT_EQ(1, connection().start_encryption_count());
  } else {
    EXPECT_EQ(1, status_handler().call_count());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
  }
}

TEST_P(HandlesEvent, InWaitEncryptionStateAsInitiator) {
  // Advance state machine.
  static_cast<void>(pairing_state().InitiatePairing(NoOpStatusCallback));
  static_cast<void>(pairing_state().OnIoCapabilityRequest());
  pairing_state().OnIoCapabilityResponse(kTestPeerIoCap);
  pairing_state().OnUserConfirmationRequest(kTestPasskey, NoOpUserConfirmationCallback);
  pairing_state().OnSimplePairingComplete(hci::StatusCode::kSuccess);
  pairing_state().OnLinkKeyNotification(kTestLinkKeyValue, kTestUnauthenticatedLinkKeyType);
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
  pairing_state().OnUserConfirmationRequest(kTestPasskey, NoOpUserConfirmationCallback);
  pairing_state().OnSimplePairingComplete(hci::StatusCode::kSuccess);
  pairing_state().OnLinkKeyNotification(kTestLinkKeyValue, kTestUnauthenticatedLinkKeyType);
  ASSERT_FALSE(pairing_state().initiator());

  RETURN_IF_FATAL(InjectEvent());

  // Should not receive anything other than OnEncryptionChange.
  EXPECT_EQ(1, status_handler().call_count());
  ASSERT_TRUE(status_handler().status());
  EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
}

TEST_P(HandlesEvent, InIdleStateAfterOnePairing) {
  // Advance state machine.
  static_cast<void>(pairing_state().InitiatePairing(NoOpStatusCallback));
  static_cast<void>(pairing_state().OnIoCapabilityRequest());
  pairing_state().OnIoCapabilityResponse(kTestPeerIoCap);
  pairing_state().OnUserConfirmationRequest(kTestPasskey, NoOpUserConfirmationCallback);
  pairing_state().OnSimplePairingComplete(hci::StatusCode::kSuccess);
  pairing_state().OnLinkKeyNotification(kTestLinkKeyValue, kTestUnauthenticatedLinkKeyType);
  pairing_state().OnAuthenticationComplete(hci::StatusCode::kSuccess);
  ASSERT_TRUE(pairing_state().initiator());

  // Successfully enabling encryption should allow pairing to start again.
  pairing_state().OnEncryptionChange(hci::Status(), true);
  EXPECT_EQ(1, status_handler().call_count());
  ASSERT_TRUE(status_handler().status());
  EXPECT_TRUE(status_handler().status()->is_success());
  EXPECT_FALSE(pairing_state().initiator());

  RETURN_IF_FATAL(InjectEvent());
  if (event() == IoCapabilityResponse) {
    EXPECT_EQ(1, status_handler().call_count());
  } else {
    EXPECT_EQ(2, status_handler().call_count());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
  }
}

TEST_P(HandlesEvent, InFailedStateAfterPairingFailed) {
  // Advance state machine.
  pairing_state().OnIoCapabilityResponse(kTestPeerIoCap);
  static_cast<void>(pairing_state().OnIoCapabilityRequest());
  pairing_state().OnUserConfirmationRequest(kTestPasskey, NoOpUserConfirmationCallback);

  // Inject failure status.
  pairing_state().OnSimplePairingComplete(hci::StatusCode::kAuthenticationFailure);
  EXPECT_EQ(1, status_handler().call_count());
  ASSERT_TRUE(status_handler().status());
  EXPECT_FALSE(status_handler().status()->is_success());

  RETURN_IF_FATAL(InjectEvent());
  EXPECT_EQ(2, status_handler().call_count());
  ASSERT_TRUE(status_handler().status());
  EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
}

TEST_P(HandlesEvent, InFailedStateAfterAuthenticationFailed) {
  // Advance state machine.
  static_cast<void>(pairing_state().InitiatePairing(NoOpStatusCallback));
  static_cast<void>(pairing_state().OnIoCapabilityRequest());
  pairing_state().OnIoCapabilityResponse(kTestPeerIoCap);
  pairing_state().OnUserConfirmationRequest(kTestPasskey, NoOpUserConfirmationCallback);
  pairing_state().OnSimplePairingComplete(hci::StatusCode::kSuccess);
  pairing_state().OnLinkKeyNotification(kTestLinkKeyValue, kTestUnauthenticatedLinkKeyType);

  // Inject failure status.
  pairing_state().OnAuthenticationComplete(hci::StatusCode::kAuthenticationFailure);
  EXPECT_EQ(1, status_handler().call_count());
  ASSERT_TRUE(status_handler().status());
  EXPECT_FALSE(status_handler().status()->is_success());

  RETURN_IF_FATAL(InjectEvent());
  EXPECT_EQ(2, status_handler().call_count());
  ASSERT_TRUE(status_handler().status());
  EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
}

// PairingAction expected answers are inferred from "device A" Authentication
// Stage 1 specs in v5.0 Vol 3, Part C, Sec 5.2.2.6, Table 5.7.
TEST(GAP_PairingStateTest, GetInitiatorPairingAction) {
  EXPECT_EQ(PairingAction::kAutomatic,
            GetInitiatorPairingAction(IOCapability::kDisplayOnly, IOCapability::kDisplayOnly));
  EXPECT_EQ(PairingAction::kDisplayPasskey,
            GetInitiatorPairingAction(IOCapability::kDisplayOnly, IOCapability::kDisplayYesNo));
  EXPECT_EQ(PairingAction::kDisplayPasskey,
            GetInitiatorPairingAction(IOCapability::kDisplayOnly, IOCapability::kKeyboardOnly));
  EXPECT_EQ(PairingAction::kAutomatic,
            GetInitiatorPairingAction(IOCapability::kDisplayOnly, IOCapability::kNoInputNoOutput));

  EXPECT_EQ(PairingAction::kComparePasskey,
            GetInitiatorPairingAction(IOCapability::kDisplayYesNo, IOCapability::kDisplayOnly));
  EXPECT_EQ(PairingAction::kDisplayPasskey,
            GetInitiatorPairingAction(IOCapability::kDisplayYesNo, IOCapability::kDisplayYesNo));
  EXPECT_EQ(PairingAction::kDisplayPasskey,
            GetInitiatorPairingAction(IOCapability::kDisplayYesNo, IOCapability::kKeyboardOnly));
  EXPECT_EQ(PairingAction::kGetConsent,
            GetInitiatorPairingAction(IOCapability::kDisplayYesNo, IOCapability::kNoInputNoOutput));

  EXPECT_EQ(PairingAction::kRequestPasskey,
            GetInitiatorPairingAction(IOCapability::kKeyboardOnly, IOCapability::kDisplayOnly));
  EXPECT_EQ(PairingAction::kRequestPasskey,
            GetInitiatorPairingAction(IOCapability::kKeyboardOnly, IOCapability::kDisplayYesNo));
  EXPECT_EQ(PairingAction::kRequestPasskey,
            GetInitiatorPairingAction(IOCapability::kKeyboardOnly, IOCapability::kKeyboardOnly));
  EXPECT_EQ(PairingAction::kAutomatic,
            GetInitiatorPairingAction(IOCapability::kKeyboardOnly, IOCapability::kNoInputNoOutput));

  EXPECT_EQ(PairingAction::kAutomatic,
            GetInitiatorPairingAction(IOCapability::kNoInputNoOutput, IOCapability::kDisplayOnly));
  EXPECT_EQ(PairingAction::kAutomatic,
            GetInitiatorPairingAction(IOCapability::kNoInputNoOutput, IOCapability::kDisplayYesNo));
  EXPECT_EQ(PairingAction::kAutomatic,
            GetInitiatorPairingAction(IOCapability::kNoInputNoOutput, IOCapability::kKeyboardOnly));
  EXPECT_EQ(PairingAction::kAutomatic, GetInitiatorPairingAction(IOCapability::kNoInputNoOutput,
                                                                 IOCapability::kNoInputNoOutput));
}

// Ibid., but for "device B."
TEST(GAP_PairingStateTest, GetResponderPairingAction) {
  EXPECT_EQ(PairingAction::kAutomatic,
            GetResponderPairingAction(IOCapability::kDisplayOnly, IOCapability::kDisplayOnly));
  EXPECT_EQ(PairingAction::kComparePasskey,
            GetResponderPairingAction(IOCapability::kDisplayOnly, IOCapability::kDisplayYesNo));
  EXPECT_EQ(PairingAction::kRequestPasskey,
            GetResponderPairingAction(IOCapability::kDisplayOnly, IOCapability::kKeyboardOnly));
  EXPECT_EQ(PairingAction::kAutomatic,
            GetResponderPairingAction(IOCapability::kDisplayOnly, IOCapability::kNoInputNoOutput));

  EXPECT_EQ(PairingAction::kDisplayPasskey,
            GetResponderPairingAction(IOCapability::kDisplayYesNo, IOCapability::kDisplayOnly));
  EXPECT_EQ(PairingAction::kComparePasskey,
            GetResponderPairingAction(IOCapability::kDisplayYesNo, IOCapability::kDisplayYesNo));
  EXPECT_EQ(PairingAction::kRequestPasskey,
            GetResponderPairingAction(IOCapability::kDisplayYesNo, IOCapability::kKeyboardOnly));
  EXPECT_EQ(PairingAction::kAutomatic,
            GetResponderPairingAction(IOCapability::kDisplayYesNo, IOCapability::kNoInputNoOutput));

  EXPECT_EQ(PairingAction::kDisplayPasskey,
            GetResponderPairingAction(IOCapability::kKeyboardOnly, IOCapability::kDisplayOnly));
  EXPECT_EQ(PairingAction::kDisplayPasskey,
            GetResponderPairingAction(IOCapability::kKeyboardOnly, IOCapability::kDisplayYesNo));
  EXPECT_EQ(PairingAction::kRequestPasskey,
            GetResponderPairingAction(IOCapability::kKeyboardOnly, IOCapability::kKeyboardOnly));
  EXPECT_EQ(PairingAction::kAutomatic,
            GetResponderPairingAction(IOCapability::kKeyboardOnly, IOCapability::kNoInputNoOutput));

  EXPECT_EQ(PairingAction::kAutomatic,
            GetResponderPairingAction(IOCapability::kNoInputNoOutput, IOCapability::kDisplayOnly));
  EXPECT_EQ(PairingAction::kGetConsent,
            GetResponderPairingAction(IOCapability::kNoInputNoOutput, IOCapability::kDisplayYesNo));
  EXPECT_EQ(PairingAction::kGetConsent,
            GetResponderPairingAction(IOCapability::kNoInputNoOutput, IOCapability::kKeyboardOnly));
  EXPECT_EQ(PairingAction::kAutomatic, GetResponderPairingAction(IOCapability::kNoInputNoOutput,
                                                                 IOCapability::kNoInputNoOutput));
}

// Events are obtained from ibid. association models, mapped to HCI sequences in
// v5.0 Vol 3, Vol 2, Part F, Sec 4.2.10–15.
TEST(GAP_PairingStateTest, GetExpectedEvent) {
  EXPECT_EQ(kUserConfirmationRequestEventCode,
            GetExpectedEvent(IOCapability::kDisplayOnly, IOCapability::kDisplayOnly));
  EXPECT_EQ(kUserConfirmationRequestEventCode,
            GetExpectedEvent(IOCapability::kDisplayOnly, IOCapability::kDisplayYesNo));
  EXPECT_EQ(kUserPasskeyNotificationEventCode,
            GetExpectedEvent(IOCapability::kDisplayOnly, IOCapability::kKeyboardOnly));
  EXPECT_EQ(kUserConfirmationRequestEventCode,
            GetExpectedEvent(IOCapability::kDisplayOnly, IOCapability::kNoInputNoOutput));

  EXPECT_EQ(kUserConfirmationRequestEventCode,
            GetExpectedEvent(IOCapability::kDisplayYesNo, IOCapability::kDisplayOnly));
  EXPECT_EQ(kUserConfirmationRequestEventCode,
            GetExpectedEvent(IOCapability::kDisplayYesNo, IOCapability::kDisplayYesNo));
  EXPECT_EQ(kUserPasskeyNotificationEventCode,
            GetExpectedEvent(IOCapability::kDisplayYesNo, IOCapability::kKeyboardOnly));
  EXPECT_EQ(kUserConfirmationRequestEventCode,
            GetExpectedEvent(IOCapability::kDisplayYesNo, IOCapability::kNoInputNoOutput));

  EXPECT_EQ(kUserPasskeyRequestEventCode,
            GetExpectedEvent(IOCapability::kKeyboardOnly, IOCapability::kDisplayOnly));
  EXPECT_EQ(kUserPasskeyRequestEventCode,
            GetExpectedEvent(IOCapability::kKeyboardOnly, IOCapability::kDisplayYesNo));
  EXPECT_EQ(kUserPasskeyRequestEventCode,
            GetExpectedEvent(IOCapability::kKeyboardOnly, IOCapability::kKeyboardOnly));
  EXPECT_EQ(kUserConfirmationRequestEventCode,
            GetExpectedEvent(IOCapability::kKeyboardOnly, IOCapability::kNoInputNoOutput));

  EXPECT_EQ(kUserConfirmationRequestEventCode,
            GetExpectedEvent(IOCapability::kNoInputNoOutput, IOCapability::kDisplayOnly));
  EXPECT_EQ(kUserConfirmationRequestEventCode,
            GetExpectedEvent(IOCapability::kNoInputNoOutput, IOCapability::kDisplayYesNo));
  EXPECT_EQ(kUserConfirmationRequestEventCode,
            GetExpectedEvent(IOCapability::kNoInputNoOutput, IOCapability::kKeyboardOnly));
  EXPECT_EQ(kUserConfirmationRequestEventCode,
            GetExpectedEvent(IOCapability::kNoInputNoOutput, IOCapability::kNoInputNoOutput));
}

// Level of authentication from ibid. table.
TEST(GAP_PairingStateTest, IsPairingAuthenticated) {
  EXPECT_FALSE(IsPairingAuthenticated(IOCapability::kDisplayOnly, IOCapability::kDisplayOnly));
  EXPECT_FALSE(IsPairingAuthenticated(IOCapability::kDisplayOnly, IOCapability::kDisplayYesNo));
  EXPECT_TRUE(IsPairingAuthenticated(IOCapability::kDisplayOnly, IOCapability::kKeyboardOnly));
  EXPECT_FALSE(IsPairingAuthenticated(IOCapability::kDisplayOnly, IOCapability::kNoInputNoOutput));

  EXPECT_FALSE(IsPairingAuthenticated(IOCapability::kDisplayYesNo, IOCapability::kDisplayOnly));
  EXPECT_TRUE(IsPairingAuthenticated(IOCapability::kDisplayYesNo, IOCapability::kDisplayYesNo));
  EXPECT_TRUE(IsPairingAuthenticated(IOCapability::kDisplayYesNo, IOCapability::kKeyboardOnly));
  EXPECT_FALSE(IsPairingAuthenticated(IOCapability::kDisplayYesNo, IOCapability::kNoInputNoOutput));

  EXPECT_TRUE(IsPairingAuthenticated(IOCapability::kKeyboardOnly, IOCapability::kDisplayOnly));
  EXPECT_TRUE(IsPairingAuthenticated(IOCapability::kKeyboardOnly, IOCapability::kDisplayYesNo));
  EXPECT_TRUE(IsPairingAuthenticated(IOCapability::kKeyboardOnly, IOCapability::kKeyboardOnly));
  EXPECT_FALSE(IsPairingAuthenticated(IOCapability::kKeyboardOnly, IOCapability::kNoInputNoOutput));

  EXPECT_FALSE(IsPairingAuthenticated(IOCapability::kNoInputNoOutput, IOCapability::kDisplayOnly));
  EXPECT_FALSE(IsPairingAuthenticated(IOCapability::kNoInputNoOutput, IOCapability::kDisplayYesNo));
  EXPECT_FALSE(IsPairingAuthenticated(IOCapability::kNoInputNoOutput, IOCapability::kKeyboardOnly));
  EXPECT_FALSE(
      IsPairingAuthenticated(IOCapability::kNoInputNoOutput, IOCapability::kNoInputNoOutput));
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
            GetResponderAuthRequirements(IOCapability::kDisplayOnly, IOCapability::kDisplayOnly));
  EXPECT_EQ(AuthRequirements::kGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kDisplayOnly, IOCapability::kDisplayYesNo));
  EXPECT_EQ(AuthRequirements::kMITMGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kDisplayOnly, IOCapability::kKeyboardOnly));
  EXPECT_EQ(
      AuthRequirements::kGeneralBonding,
      GetResponderAuthRequirements(IOCapability::kDisplayOnly, IOCapability::kNoInputNoOutput));

  EXPECT_EQ(AuthRequirements::kGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kDisplayYesNo, IOCapability::kDisplayOnly));
  EXPECT_EQ(AuthRequirements::kMITMGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kDisplayYesNo, IOCapability::kDisplayYesNo));
  EXPECT_EQ(AuthRequirements::kMITMGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kDisplayYesNo, IOCapability::kKeyboardOnly));
  EXPECT_EQ(
      AuthRequirements::kGeneralBonding,
      GetResponderAuthRequirements(IOCapability::kDisplayYesNo, IOCapability::kNoInputNoOutput));

  EXPECT_EQ(AuthRequirements::kMITMGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kKeyboardOnly, IOCapability::kDisplayOnly));
  EXPECT_EQ(AuthRequirements::kMITMGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kKeyboardOnly, IOCapability::kDisplayYesNo));
  EXPECT_EQ(AuthRequirements::kMITMGeneralBonding,
            GetResponderAuthRequirements(IOCapability::kKeyboardOnly, IOCapability::kKeyboardOnly));
  EXPECT_EQ(
      AuthRequirements::kGeneralBonding,
      GetResponderAuthRequirements(IOCapability::kKeyboardOnly, IOCapability::kNoInputNoOutput));

  EXPECT_EQ(
      AuthRequirements::kGeneralBonding,
      GetResponderAuthRequirements(IOCapability::kNoInputNoOutput, IOCapability::kDisplayOnly));
  EXPECT_EQ(
      AuthRequirements::kGeneralBonding,
      GetResponderAuthRequirements(IOCapability::kNoInputNoOutput, IOCapability::kDisplayYesNo));
  EXPECT_EQ(
      AuthRequirements::kGeneralBonding,
      GetResponderAuthRequirements(IOCapability::kNoInputNoOutput, IOCapability::kKeyboardOnly));
  EXPECT_EQ(
      AuthRequirements::kGeneralBonding,
      GetResponderAuthRequirements(IOCapability::kNoInputNoOutput, IOCapability::kNoInputNoOutput));
}

}  // namespace
}  // namespace gap
}  // namespace bt
