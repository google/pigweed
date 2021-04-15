// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/gap/pairing_state.h"

#include <gtest/gtest.h>

#include "lib/gtest/test_loop_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/fake_pairing_delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/peer_cache.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/fake_connection.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/types.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_peer.h"

namespace bt::gap {
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
const auto kTestLinkKeyValue = UInt128{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
const hci::LinkKey kTestLinkKey(kTestLinkKeyValue, 0, 0);
const auto kTestUnauthenticatedLinkKeyType = hci::LinkKeyType::kUnauthenticatedCombination192;
const auto kTestAuthenticatedLinkKeyType = hci::LinkKeyType::kAuthenticatedCombination192;
const auto kTestLegacyLinkKeyType = hci::LinkKeyType::kCombination;
const auto kTestChangedLinkKeyType = hci::LinkKeyType::kChangedCombination;
const BrEdrSecurityRequirements kNoSecurityRequirements{.authentication = false,
                                                        .secure_connections = false};

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

class GAP_PairingStateTest : public ::gtest::TestLoopFixture {
 public:
  GAP_PairingStateTest() = default;
  virtual ~GAP_PairingStateTest() = default;

  void SetUp() override {
    peer_cache_ = std::make_unique<PeerCache>();
    peer_ = peer_cache_->NewPeer(kPeerAddress, true);

    auth_request_count_ = 0;
    send_auth_request_callback_ = [this]() { auth_request_count_++; };
  }

  void TearDown() override {
    peer_ = nullptr;
    peer_cache_ = nullptr;
  }

  fit::closure MakeAuthRequestCallback() { return send_auth_request_callback_.share(); }

  PeerCache* peer_cache() const { return peer_cache_.get(); }
  Peer* peer() const { return peer_; }
  size_t auth_request_count() const { return auth_request_count_; }

 private:
  inspect::Inspector inspector_;
  std::unique_ptr<PeerCache> peer_cache_;
  Peer* peer_;
  size_t auth_request_count_;
  fit::closure send_auth_request_callback_;
};

TEST_F(GAP_PairingStateTest, PairingStateStartsAsResponder) {
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             NoOpStatusCallback);
  EXPECT_FALSE(pairing_state.initiator());
}

TEST_F(GAP_PairingStateTest, PairingStateRemainsResponderAfterPeerIoCapResponse) {
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             NoOpStatusCallback);
  pairing_state.OnIoCapabilityResponse(kTestPeerIoCap);
  EXPECT_EQ(0u, auth_request_count());
  EXPECT_FALSE(pairing_state.initiator());
}

TEST_F(GAP_PairingStateTest, PairingStateBecomesInitiatorAfterLocalPairingInitiated) {
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             NoOpStatusCallback);
  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());
  pairing_state.InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback);
  EXPECT_EQ(1u, auth_request_count());
  EXPECT_TRUE(pairing_state.initiator());
}

TEST_F(GAP_PairingStateTest, PairingStateSendsAuthenticationRequestExactlyOnce) {
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             NoOpStatusCallback);
  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  pairing_state.InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback);
  EXPECT_EQ(1u, auth_request_count());
  EXPECT_TRUE(pairing_state.initiator());

  pairing_state.InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback);
  EXPECT_EQ(1u, auth_request_count());
  EXPECT_TRUE(pairing_state.initiator());
}

TEST_F(GAP_PairingStateTest,
       PairingStateRemainsResponderIfPairingInitiatedWhileResponderPairingInProgress) {
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             NoOpStatusCallback);
  pairing_state.OnIoCapabilityResponse(kTestPeerIoCap);
  ASSERT_FALSE(pairing_state.initiator());

  pairing_state.InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback);
  EXPECT_EQ(0u, auth_request_count());
  EXPECT_FALSE(pairing_state.initiator());
}

TEST_F(GAP_PairingStateTest, StatusCallbackMayDestroyPairingState) {
  auto connection = MakeFakeConnection();
  std::unique_ptr<PairingState> pairing_state;
  bool cb_called = false;
  auto status_cb = [&pairing_state, &cb_called](hci::ConnectionHandle handle, hci::Status status) {
    EXPECT_FALSE(status.is_success());
    cb_called = true;

    // Note that this lambda is owned by the PairingState so its captures are invalid after this.
    pairing_state = nullptr;
  };

  pairing_state = std::make_unique<PairingState>(kTestPeerId, &connection, peer_cache(),
                                                 MakeAuthRequestCallback(), status_cb);

  // Unexpected event that should cause the status callback to be called with an error.
  pairing_state->OnUserPasskeyNotification(kTestPasskey);

  EXPECT_TRUE(cb_called);
}

TEST_F(GAP_PairingStateTest, InitiatorCallbackMayDestroyPairingState) {
  auto connection = MakeFakeConnection();
  std::unique_ptr<PairingState> pairing_state = std::make_unique<PairingState>(
      kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(), NoOpStatusCallback);
  bool cb_called = false;
  auto status_cb = [&pairing_state, &cb_called](hci::ConnectionHandle handle, hci::Status status) {
    EXPECT_FALSE(status.is_success());
    cb_called = true;

    // Note that this lambda is owned by the PairingState so its captures are invalid after this.
    pairing_state = nullptr;
  };
  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state->SetPairingDelegate(pairing_delegate.GetWeakPtr());
  pairing_state->InitiatePairing(kNoSecurityRequirements, status_cb);

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

TEST_F(GAP_PairingStateTest, TestStatusHandlerTracksStatusCallbackInvocations) {
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

TEST_F(GAP_PairingStateTest, InitiatingPairingAfterErrorTriggersStatusCallbackWithError) {
  TestStatusHandler link_status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             link_status_handler.MakeStatusCallback());

  // Unexpected event that should cause the status callback to be called with an error.
  pairing_state.OnUserPasskeyNotification(kTestPasskey);

  EXPECT_EQ(1, link_status_handler.call_count());
  ASSERT_TRUE(link_status_handler.handle());
  EXPECT_EQ(kTestHandle, *link_status_handler.handle());
  ASSERT_TRUE(link_status_handler.status());
  EXPECT_EQ(hci::Status(HostError::kNotSupported), *link_status_handler.status());

  // Try to initiate pairing again.
  TestStatusHandler pairing_status_handler;
  pairing_state.InitiatePairing(kNoSecurityRequirements,
                                pairing_status_handler.MakeStatusCallback());

  // The status callback for this attempt should be notified with an error.
  EXPECT_EQ(1, pairing_status_handler.call_count());
  ASSERT_TRUE(pairing_status_handler.handle());
  EXPECT_EQ(kTestHandle, *pairing_status_handler.handle());
  ASSERT_TRUE(pairing_status_handler.status());
  EXPECT_EQ(hci::Status(HostError::kNotReady), *pairing_status_handler.status());
}

TEST_F(GAP_PairingStateTest, UnexpectedEncryptionChangeDoesNotTriggerStatusCallback) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  pairing_state.InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback);
  static_cast<void>(pairing_state.OnLinkKeyRequest(kPeerAddress));
  static_cast<void>(pairing_state.OnIoCapabilityRequest());
  pairing_state.OnIoCapabilityResponse(kTestPeerIoCap);

  ASSERT_EQ(0, connection.start_encryption_count());
  ASSERT_EQ(0, status_handler.call_count());

  connection.TriggerEncryptionChangeCallback(hci::Status(), true);
  EXPECT_EQ(0, status_handler.call_count());
}

TEST_F(GAP_PairingStateTest, PeerMayNotChangeLinkKeyWhenNotEncrypted) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  ASSERT_FALSE(connection.ltk().has_value());

  pairing_state.OnLinkKeyNotification(kTestLinkKeyValue, kTestChangedLinkKeyType);

  EXPECT_FALSE(connection.ltk().has_value());
  EXPECT_EQ(1, status_handler.call_count());
  ASSERT_TRUE(status_handler.handle());
  EXPECT_EQ(kTestHandle, *status_handler.handle());
  ASSERT_TRUE(status_handler.status());
  EXPECT_EQ(HostError::kInsufficientSecurity, status_handler.status()->error());
}

TEST_F(GAP_PairingStateTest, PeerMayChangeLinkKeyWhenInIdleState) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  connection.set_bredr_link_key(hci::LinkKey(UInt128(), 0, 0), kTestAuthenticatedLinkKeyType);

  pairing_state.OnLinkKeyNotification(kTestLinkKeyValue, kTestChangedLinkKeyType);

  ASSERT_TRUE(connection.ltk().has_value());
  EXPECT_EQ(kTestLinkKeyValue, connection.ltk().value().value());
  ASSERT_TRUE(connection.ltk_type().has_value());
  EXPECT_EQ(kTestChangedLinkKeyType, connection.ltk_type().value());
  EXPECT_EQ(0, status_handler.call_count());
}

// Inject events that occur during the course of a successful pairing as an initiator, but not
// including enabling link encryption.
void AdvanceToEncryptionAsInitiator(PairingState* pairing_state) {
  static_cast<void>(pairing_state->OnLinkKeyRequest(kPeerAddress));
  static_cast<void>(pairing_state->OnIoCapabilityRequest());
  pairing_state->OnIoCapabilityResponse(kTestPeerIoCap);
  pairing_state->OnUserConfirmationRequest(kTestPasskey, NoOpUserConfirmationCallback);
  pairing_state->OnSimplePairingComplete(hci::StatusCode::kSuccess);
  pairing_state->OnLinkKeyNotification(kTestLinkKeyValue, kTestUnauthenticatedLinkKeyType);
  pairing_state->OnAuthenticationComplete(hci::StatusCode::kSuccess);
}

TEST_F(GAP_PairingStateTest, SuccessfulEncryptionChangeTriggersStatusCallback) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  pairing_state.InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback);
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

TEST_F(GAP_PairingStateTest, EncryptionChangeErrorTriggersStatusCallbackWithError) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  static_cast<void>(pairing_state.InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback));
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

TEST_F(GAP_PairingStateTest, EncryptionChangeToDisabledTriggersStatusCallbackWithError) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  pairing_state.InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback);
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

TEST_F(GAP_PairingStateTest, EncryptionChangeToEnableCallsInitiatorCallbacks) {
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             NoOpStatusCallback);
  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  TestStatusHandler status_handler_0;
  pairing_state.InitiatePairing(kNoSecurityRequirements, status_handler_0.MakeStatusCallback());
  AdvanceToEncryptionAsInitiator(&pairing_state);
  EXPECT_TRUE(pairing_state.initiator());

  // Try to initiate pairing while pairing is in progress.
  TestStatusHandler status_handler_1;
  static_cast<void>(pairing_state.InitiatePairing(kNoSecurityRequirements,
                                                  status_handler_1.MakeStatusCallback()));

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

TEST_F(GAP_PairingStateTest, InitiatingPairingOnResponderWaitsForPairingToFinish) {
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             NoOpStatusCallback);
  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine as pairing responder.
  pairing_state.OnIoCapabilityResponse(kTestPeerIoCap);
  ASSERT_FALSE(pairing_state.initiator());
  static_cast<void>(pairing_state.OnIoCapabilityRequest());
  pairing_state.OnUserConfirmationRequest(kTestPasskey, NoOpUserConfirmationCallback);

  // Try to initiate pairing while pairing is in progress.
  TestStatusHandler status_handler;
  pairing_state.InitiatePairing(kNoSecurityRequirements, status_handler.MakeStatusCallback());
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

TEST_F(GAP_PairingStateTest, UnresolvedPairingCallbackIsCalledOnDestruction) {
  auto connection = MakeFakeConnection();
  TestStatusHandler overall_status, request_status;
  {
    PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                               overall_status.MakeStatusCallback());
    NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
    pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

    // Advance state machine as pairing responder.
    pairing_state.OnIoCapabilityResponse(kTestPeerIoCap);
    ASSERT_FALSE(pairing_state.initiator());
    static_cast<void>(pairing_state.OnIoCapabilityRequest());
    pairing_state.OnUserConfirmationRequest(kTestPasskey, NoOpUserConfirmationCallback);

    // Try to initiate pairing while pairing is in progress.
    pairing_state.InitiatePairing(kNoSecurityRequirements, request_status.MakeStatusCallback());
    EXPECT_FALSE(pairing_state.initiator());

    // Keep advancing state machine.
    pairing_state.OnSimplePairingComplete(hci::StatusCode::kSuccess);
    pairing_state.OnLinkKeyNotification(kTestLinkKeyValue, kTestUnauthenticatedLinkKeyType);

    // as pairing_state falls out of scope, we expect additional pairing callbacks to be called
    ASSERT_EQ(0, overall_status.call_count());
    ASSERT_EQ(0, request_status.call_count());
  }

  ASSERT_EQ(0, overall_status.call_count());

  ASSERT_EQ(1, request_status.call_count());
  ASSERT_TRUE(request_status.handle());
  EXPECT_EQ(kTestHandle, *request_status.handle());
  EXPECT_EQ(hci::Status(HostError::kLinkDisconnected), *request_status.status());
}

TEST_F(GAP_PairingStateTest, PairingStateRejectsPairingInitiationWithoutPairingDelegate) {
  auto connection = MakeFakeConnection();
  TestStatusHandler owner_status_handler;
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             owner_status_handler.MakeStatusCallback());

  TestStatusHandler initiator_status_handler;
  pairing_state.InitiatePairing(kNoSecurityRequirements,
                                initiator_status_handler.MakeStatusCallback());
  EXPECT_FALSE(pairing_state.initiator());

  // The owning link doesn't get an error for a failure to initiator pairing.
  EXPECT_EQ(0, owner_status_handler.call_count());

  // But the initiator should get an error.
  EXPECT_EQ(1, initiator_status_handler.call_count());
  ASSERT_TRUE(initiator_status_handler.status());
  EXPECT_EQ(hci::Status(HostError::kNotReady), *initiator_status_handler.status());
}

TEST_F(GAP_PairingStateTest, PairingStateRejectsPairingResponseWithoutPairingDelegate) {
  auto connection = MakeFakeConnection();
  TestStatusHandler status_handler;
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  pairing_state.OnIoCapabilityResponse(kTestPeerIoCap);

  // PairingState rejects the pairing with a negative reply.
  EXPECT_FALSE(pairing_state.OnIoCapabilityRequest());
  EXPECT_FALSE(pairing_state.initiator());

  // The owner should be alerted with an error.
  EXPECT_EQ(1, status_handler.call_count());
  ASSERT_TRUE(status_handler.status());
  EXPECT_EQ(hci::Status(HostError::kNotReady), *status_handler.status());
}

TEST_F(GAP_PairingStateTest, UnexpectedLinkKeyAuthenticationRaisesError) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
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

TEST_F(GAP_PairingStateTest, LegacyPairingLinkKeyRaisesError) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
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

TEST_F(GAP_PairingStateTest, PairingSetsConnectionLinkKey) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
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

TEST_F(GAP_PairingStateTest, NumericComparisonPairingComparesPasskeyOnInitiatorDisplayYesNoSide) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  FakePairingDelegate pairing_delegate(sm::IOCapability::kDisplayYesNo);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  pairing_state.InitiatePairing(kNoSecurityRequirements, status_handler.MakeStatusCallback());
  ASSERT_TRUE(pairing_state.initiator());
  static_cast<void>(pairing_state.OnLinkKeyRequest(kPeerAddress));
  EXPECT_EQ(IOCapability::kDisplayYesNo, *pairing_state.OnIoCapabilityRequest());

  pairing_state.OnIoCapabilityResponse(IOCapability::kDisplayYesNo);

  pairing_delegate.SetDisplayPasskeyCallback(
      [](PeerId peer_id, uint32_t value, PairingDelegate::DisplayMethod method, auto cb) {
        EXPECT_EQ(kTestPeerId, peer_id);
        EXPECT_EQ(kTestPasskey, value);
        EXPECT_EQ(PairingDelegate::DisplayMethod::kComparison, method);
        ASSERT_TRUE(cb);
        cb(true);
      });
  bool confirmed = false;
  pairing_state.OnUserConfirmationRequest(kTestPasskey,
                                          [&confirmed](bool confirm) { confirmed = confirm; });
  EXPECT_TRUE(confirmed);

  pairing_delegate.SetCompletePairingCallback([](PeerId peer_id, sm::Status status) {
    EXPECT_EQ(kTestPeerId, peer_id);
    EXPECT_TRUE(status);
  });
  pairing_state.OnSimplePairingComplete(hci::StatusCode::kSuccess);

  EXPECT_EQ(0, status_handler.call_count());
}

TEST_F(GAP_PairingStateTest, NumericComparisonPairingComparesPasskeyOnResponderDisplayYesNoSide) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  FakePairingDelegate pairing_delegate(sm::IOCapability::kDisplayYesNo);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  pairing_state.OnIoCapabilityResponse(IOCapability::kDisplayYesNo);
  ASSERT_FALSE(pairing_state.initiator());
  EXPECT_EQ(IOCapability::kDisplayYesNo, *pairing_state.OnIoCapabilityRequest());

  pairing_delegate.SetDisplayPasskeyCallback(
      [](PeerId peer_id, uint32_t value, PairingDelegate::DisplayMethod method, auto cb) {
        EXPECT_EQ(kTestPeerId, peer_id);
        EXPECT_EQ(kTestPasskey, value);
        EXPECT_EQ(PairingDelegate::DisplayMethod::kComparison, method);
        ASSERT_TRUE(cb);
        cb(true);
      });
  bool confirmed = false;
  pairing_state.OnUserConfirmationRequest(kTestPasskey,
                                          [&confirmed](bool confirm) { confirmed = confirm; });
  EXPECT_TRUE(confirmed);

  pairing_delegate.SetCompletePairingCallback([](PeerId peer_id, sm::Status status) {
    EXPECT_EQ(kTestPeerId, peer_id);
    EXPECT_TRUE(status);
  });
  pairing_state.OnSimplePairingComplete(hci::StatusCode::kSuccess);

  EXPECT_EQ(0, status_handler.call_count());
}

// v5.0, Vol 3, Part C, Sec 5.2.2.6 call this "Numeric Comparison with automatic
// confirmation on device B only and Yes/No confirmation on whether to pair on
// device A. Device A does not show the confirmation value." and it should
// result in user consent.
TEST_F(GAP_PairingStateTest, NumericComparisonWithoutValueRequestsConsentFromDisplayYesNoSide) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  FakePairingDelegate pairing_delegate(sm::IOCapability::kDisplayYesNo);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  pairing_state.OnIoCapabilityResponse(IOCapability::kNoInputNoOutput);
  ASSERT_FALSE(pairing_state.initiator());
  EXPECT_EQ(IOCapability::kDisplayYesNo, *pairing_state.OnIoCapabilityRequest());

  pairing_delegate.SetConfirmPairingCallback([](PeerId peer_id, auto cb) {
    EXPECT_EQ(kTestPeerId, peer_id);
    ASSERT_TRUE(cb);
    cb(true);
  });
  bool confirmed = false;
  pairing_state.OnUserConfirmationRequest(kTestPasskey,
                                          [&confirmed](bool confirm) { confirmed = confirm; });
  EXPECT_TRUE(confirmed);

  pairing_delegate.SetCompletePairingCallback([](PeerId peer_id, sm::Status status) {
    EXPECT_EQ(kTestPeerId, peer_id);
    EXPECT_TRUE(status);
  });
  pairing_state.OnSimplePairingComplete(hci::StatusCode::kSuccess);

  EXPECT_EQ(0, status_handler.call_count());
}

TEST_F(GAP_PairingStateTest, PasskeyEntryPairingDisplaysPasskeyToDisplayOnlySide) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  FakePairingDelegate pairing_delegate(sm::IOCapability::kDisplayOnly);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  pairing_state.OnIoCapabilityResponse(IOCapability::kKeyboardOnly);
  ASSERT_FALSE(pairing_state.initiator());
  EXPECT_EQ(IOCapability::kDisplayOnly, *pairing_state.OnIoCapabilityRequest());

  pairing_delegate.SetDisplayPasskeyCallback(
      [](PeerId peer_id, uint32_t value, PairingDelegate::DisplayMethod method, auto cb) {
        EXPECT_EQ(kTestPeerId, peer_id);
        EXPECT_EQ(kTestPasskey, value);
        EXPECT_EQ(PairingDelegate::DisplayMethod::kPeerEntry, method);
        EXPECT_TRUE(cb);
      });
  pairing_state.OnUserPasskeyNotification(kTestPasskey);

  pairing_delegate.SetCompletePairingCallback([](PeerId peer_id, sm::Status status) {
    EXPECT_EQ(kTestPeerId, peer_id);
    EXPECT_TRUE(status);
  });
  pairing_state.OnSimplePairingComplete(hci::StatusCode::kSuccess);

  EXPECT_EQ(0, status_handler.call_count());
}

TEST_F(GAP_PairingStateTest, PasskeyEntryPairingRequestsPasskeyFromKeyboardOnlySide) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  FakePairingDelegate pairing_delegate(sm::IOCapability::kKeyboardOnly);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  pairing_state.OnIoCapabilityResponse(IOCapability::kDisplayOnly);
  ASSERT_FALSE(pairing_state.initiator());
  EXPECT_EQ(IOCapability::kKeyboardOnly, *pairing_state.OnIoCapabilityRequest());

  pairing_delegate.SetRequestPasskeyCallback([](PeerId peer_id, auto cb) {
    EXPECT_EQ(kTestPeerId, peer_id);
    ASSERT_TRUE(cb);
    cb(kTestPasskey);
  });
  bool cb_called = false;
  std::optional<uint32_t> passkey;
  auto passkey_cb = [&cb_called, &passkey](std::optional<uint32_t> pairing_state_passkey) {
    cb_called = true;
    passkey = pairing_state_passkey;
  };

  pairing_state.OnUserPasskeyRequest(std::move(passkey_cb));
  EXPECT_TRUE(cb_called);
  ASSERT_TRUE(passkey);
  EXPECT_EQ(kTestPasskey, *passkey);

  pairing_delegate.SetCompletePairingCallback([](PeerId peer_id, sm::Status status) {
    EXPECT_EQ(kTestPeerId, peer_id);
    EXPECT_TRUE(status);
  });
  pairing_state.OnSimplePairingComplete(hci::StatusCode::kSuccess);

  EXPECT_EQ(0, status_handler.call_count());
}

TEST_F(GAP_PairingStateTest, JustWorksPairingDoesNotRequestUserAction) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  FakePairingDelegate pairing_delegate(sm::IOCapability::kNoInputNoOutput);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  pairing_state.OnIoCapabilityResponse(IOCapability::kNoInputNoOutput);
  ASSERT_FALSE(pairing_state.initiator());
  EXPECT_EQ(IOCapability::kNoInputNoOutput, *pairing_state.OnIoCapabilityRequest());

  bool confirmed = false;
  pairing_state.OnUserConfirmationRequest(kTestPasskey,
                                          [&confirmed](bool confirm) { confirmed = confirm; });
  EXPECT_TRUE(confirmed);

  pairing_delegate.SetCompletePairingCallback([](PeerId peer_id, sm::Status status) {
    EXPECT_EQ(kTestPeerId, peer_id);
    EXPECT_TRUE(status);
  });
  pairing_state.OnSimplePairingComplete(hci::StatusCode::kSuccess);

  EXPECT_EQ(0, status_handler.call_count());
}

// Event injectors. Return values are necessarily ignored in order to make types
// match, so don't use these functions to test return values. Likewise,
// arguments have been filled with test defaults for a successful pairing flow.
void LinkKeyRequest(PairingState* pairing_state) {
  static_cast<void>(pairing_state->OnLinkKeyRequest(kPeerAddress));
}
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
class HandlesEvent : public GAP_PairingStateTest,
                     public ::testing::WithParamInterface<void (*)(PairingState*)> {
 public:
  HandlesEvent() : connection_(MakeFakeConnection()){};
  virtual ~HandlesEvent() = default;

  void SetUp() override {
    GAP_PairingStateTest::SetUp();
    pairing_delegate_ = std::make_unique<NoOpPairingDelegate>(kTestLocalIoCap);
    pairing_state_ = std::make_unique<PairingState>(kTestPeerId, &connection_, peer_cache(),
                                                    MakeAuthRequestCallback(),
                                                    status_handler_.MakeStatusCallback());
    pairing_state().SetPairingDelegate(pairing_delegate_->GetWeakPtr());
  }

  const FakeConnection& connection() const { return connection_; }
  const TestStatusHandler& status_handler() const { return status_handler_; }
  PairingState& pairing_state() { return *pairing_state_; }

  // Returns an event injector that was passed to INSTANTIATE_TEST_SUITE_P.
  auto* event() const { return GetParam(); }

  void InjectEvent() { event()(&pairing_state()); }

 private:
  FakeConnection connection_;
  TestStatusHandler status_handler_;
  std::unique_ptr<NoOpPairingDelegate> pairing_delegate_;
  std::unique_ptr<PairingState> pairing_state_;
};

// The tests here exercise that PairingState can be successfully advances
// through the expected pairing flow and generates errors when the pairing flow
// occurs out of order. This is intended to cover its internal state machine
// transitions and not the side effects.
INSTANTIATE_TEST_SUITE_P(GAP_PairingStateTest, HandlesEvent,
                         ::testing::Values(LinkKeyRequest, IoCapabilityRequest,
                                           IoCapabilityResponse, UserConfirmationRequest,
                                           UserPasskeyRequest, UserPasskeyNotification,
                                           SimplePairingComplete, LinkKeyNotification,
                                           AuthenticationComplete));

TEST_P(HandlesEvent, InIdleState) {
  RETURN_IF_FATAL(InjectEvent());
  if (event() == LinkKeyRequest || event() == IoCapabilityResponse) {
    EXPECT_EQ(0, status_handler().call_count());
  } else {
    EXPECT_EQ(1, status_handler().call_count());
    ASSERT_TRUE(status_handler().handle());
    EXPECT_EQ(kTestHandle, *status_handler().handle());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
  }
}

TEST_P(HandlesEvent, InInitiatorWaitLinkKeyRequestState) {
  // Advance state machine.
  static_cast<void>(pairing_state().InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback));

  RETURN_IF_FATAL(InjectEvent());
  if (event() == LinkKeyRequest) {
    EXPECT_EQ(0, status_handler().call_count());
  } else {
    EXPECT_EQ(1, status_handler().call_count());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
  }
}

TEST_P(HandlesEvent, InInitiatorWaitIoCapRequest) {
  // Advance state machine.
  pairing_state().InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback);
  static_cast<void>(pairing_state().OnLinkKeyRequest(kPeerAddress));

  RETURN_IF_FATAL(InjectEvent());
  if (event() == IoCapabilityRequest) {
    EXPECT_EQ(0, status_handler().call_count());
  } else {
    EXPECT_EQ(1, status_handler().call_count());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
  }
}

TEST_P(HandlesEvent, InInitiatorWaitAuthCompleteSkippingSimplePairing) {
  peer()->MutBrEdr().SetBondData(
      sm::LTK(sm::SecurityProperties(kTestUnauthenticatedLinkKeyType), kTestLinkKey));

  // Advance state machine.
  pairing_state().InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback);
  EXPECT_NE(std::nullopt, pairing_state().OnLinkKeyRequest(kPeerAddress));

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

TEST_P(HandlesEvent, InInitiatorWaitIoCapResponseState) {
  // Advance state machine.
  static_cast<void>(pairing_state().InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback));
  static_cast<void>(pairing_state().OnLinkKeyRequest(kPeerAddress));
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
  if (event() == LinkKeyRequest || event() == IoCapabilityResponse) {
    // Peer attempted to pair again, which raises an additional "not ready" error.
    EXPECT_EQ(hci::Status(HostError::kNotReady), status_handler().status());
  } else {
    EXPECT_EQ(hci::Status(HostError::kNotSupported), status_handler().status());
  }
}

TEST_P(HandlesEvent, InWaitUserConfirmationStateAsInitiator) {
  NoOpPairingDelegate pairing_delegate(sm::IOCapability::kDisplayOnly);
  pairing_state().SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Advance state machine.
  pairing_state().InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback);
  static_cast<void>(pairing_state().OnLinkKeyRequest(kPeerAddress));
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
  pairing_state().InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback);
  static_cast<void>(pairing_state().OnLinkKeyRequest(kPeerAddress));
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
  pairing_state().InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback);
  static_cast<void>(pairing_state().OnLinkKeyRequest(kPeerAddress));
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

TEST_P(HandlesEvent, InInitiatorWaitAuthCompleteStateAfterSimplePairing) {
  // Advance state machine.
  pairing_state().InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback);
  static_cast<void>(pairing_state().OnLinkKeyRequest(kPeerAddress));
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
  pairing_state().InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback);
  static_cast<void>(pairing_state().OnLinkKeyRequest(kPeerAddress));
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
  static_cast<void>(pairing_state().InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback));
  static_cast<void>(pairing_state().OnLinkKeyRequest(kPeerAddress));
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
  if (event() == LinkKeyRequest || event() == IoCapabilityResponse) {
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
  static_cast<void>(pairing_state().InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback));
  static_cast<void>(pairing_state().OnLinkKeyRequest(kPeerAddress));
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
TEST_F(GAP_PairingStateTest, GetInitiatorPairingAction) {
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
TEST_F(GAP_PairingStateTest, GetResponderPairingAction) {
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
TEST_F(GAP_PairingStateTest, GetExpectedEvent) {
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
TEST_F(GAP_PairingStateTest, IsPairingAuthenticated) {
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

TEST_F(GAP_PairingStateTest, GetInitiatorAuthRequirements) {
  EXPECT_EQ(AuthRequirements::kMITMGeneralBonding,
            GetInitiatorAuthRequirements(IOCapability::kDisplayOnly));
  EXPECT_EQ(AuthRequirements::kMITMGeneralBonding,
            GetInitiatorAuthRequirements(IOCapability::kDisplayYesNo));
  EXPECT_EQ(AuthRequirements::kMITMGeneralBonding,
            GetInitiatorAuthRequirements(IOCapability::kKeyboardOnly));
  EXPECT_EQ(AuthRequirements::kGeneralBonding,
            GetInitiatorAuthRequirements(IOCapability::kNoInputNoOutput));
}

TEST_F(GAP_PairingStateTest, GetResponderAuthRequirements) {
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

TEST_F(GAP_PairingStateTest, SkipPairingIfExistingKeyMeetsSecurityRequirements) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  NoOpPairingDelegate pairing_delegate(sm::IOCapability::kNoInputNoOutput);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  connection.set_bredr_link_key(kTestLinkKey, kTestAuthenticatedLinkKeyType);

  constexpr BrEdrSecurityRequirements kSecurityRequirements{.authentication = true,
                                                            .secure_connections = false};
  TestStatusHandler initiator_status_handler;
  pairing_state.InitiatePairing(kSecurityRequirements,
                                initiator_status_handler.MakeStatusCallback());
  EXPECT_EQ(0u, auth_request_count());
  EXPECT_FALSE(pairing_state.initiator());
  EXPECT_EQ(0, status_handler.call_count());
  ASSERT_EQ(1, initiator_status_handler.call_count());
  EXPECT_TRUE(initiator_status_handler.status()->is_success());
}

TEST_F(GAP_PairingStateTest,
       InitiatorAuthRequiredCausesOnLinkKeyRequestToReturnNullIfUnauthenticatedKeyExists) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  NoOpPairingDelegate pairing_delegate(sm::IOCapability::kNoInputNoOutput);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  BrEdrSecurityRequirements security{.authentication = true, .secure_connections = false};
  pairing_state.InitiatePairing(security, status_handler.MakeStatusCallback());

  peer()->MutBrEdr().SetBondData(
      sm::LTK(sm::SecurityProperties(kTestUnauthenticatedLinkKeyType), kTestLinkKey));

  EXPECT_EQ(std::nullopt, pairing_state.OnLinkKeyRequest(kPeerAddress));
  EXPECT_EQ(0, status_handler.call_count());
}

TEST_F(GAP_PairingStateTest,
       InitiatorNoSecurityRequirementsCausesOnLinkKeyRequestToReturnExistingKey) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  NoOpPairingDelegate pairing_delegate(sm::IOCapability::kNoInputNoOutput);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  pairing_state.InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback);

  peer()->MutBrEdr().SetBondData(
      sm::LTK(sm::SecurityProperties(kTestUnauthenticatedLinkKeyType), kTestLinkKey));
  EXPECT_FALSE(connection.ltk().has_value());

  auto reply_key = pairing_state.OnLinkKeyRequest(kPeerAddress);
  ASSERT_TRUE(reply_key.has_value());
  EXPECT_EQ(kTestLinkKey, reply_key.value());
  EXPECT_EQ(0, status_handler.call_count());
  EXPECT_TRUE(connection.ltk().has_value());
}

TEST_F(GAP_PairingStateTest, InitiatorOnLinkKeyRequestReturnsNullIfBondDataDoesNotExist) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  NoOpPairingDelegate pairing_delegate(sm::IOCapability::kNoInputNoOutput);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  pairing_state.InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback);

  auto reply_key = pairing_state.OnLinkKeyRequest(kPeerAddress);
  EXPECT_FALSE(reply_key.has_value());
  EXPECT_EQ(0, status_handler.call_count());
}

TEST_F(GAP_PairingStateTest, IdleStateOnLinkKeyRequestReturnsLinkKeyWhenBondDataExists) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  NoOpPairingDelegate pairing_delegate(sm::IOCapability::kNoInputNoOutput);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  peer()->MutBrEdr().SetBondData(
      sm::LTK(sm::SecurityProperties(kTestUnauthenticatedLinkKeyType), kTestLinkKey));
  EXPECT_FALSE(connection.ltk().has_value());

  auto reply_key = pairing_state.OnLinkKeyRequest(kPeerAddress);
  ASSERT_TRUE(reply_key.has_value());
  EXPECT_EQ(kTestLinkKey, reply_key.value());
  EXPECT_EQ(0, status_handler.call_count());
  EXPECT_TRUE(connection.ltk().has_value());
}

TEST_F(GAP_PairingStateTest, IdleStateOnLinkKeyRequestReturnsNullWhenBondDataDoesNotExist) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  NoOpPairingDelegate pairing_delegate(sm::IOCapability::kNoInputNoOutput);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  auto reply_key = pairing_state.OnLinkKeyRequest(kPeerAddress);
  EXPECT_FALSE(reply_key.has_value());
  EXPECT_EQ(0, status_handler.call_count());
}

TEST_F(GAP_PairingStateTest, SimplePairingCompleteWithErrorCodeReceivedEarlyFailsPairing) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  NoOpPairingDelegate pairing_delegate(sm::IOCapability::kNoInputNoOutput);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  pairing_state.InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback);

  EXPECT_EQ(std::nullopt, pairing_state.OnLinkKeyRequest(kPeerAddress));
  EXPECT_EQ(IOCapability::kNoInputNoOutput, *pairing_state.OnIoCapabilityRequest());
  EXPECT_EQ(0, status_handler.call_count());

  const auto status_code = hci::StatusCode::kPairingNotAllowed;
  pairing_state.OnSimplePairingComplete(status_code);
  ASSERT_EQ(1, status_handler.call_count());
  EXPECT_EQ(hci::Status(status_code), status_handler.status().value());
}

TEST_F(GAP_PairingStateTest, OnLinkKeyRequestReceivedMissingPeerFailsPairing) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  NoOpPairingDelegate pairing_delegate(sm::IOCapability::kNoInputNoOutput);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  pairing_state.InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback);

  EXPECT_TRUE(peer_cache()->RemoveDisconnectedPeer(peer()->identifier()));

  auto reply_key = pairing_state.OnLinkKeyRequest(kPeerAddress);
  EXPECT_FALSE(reply_key.has_value());
  ASSERT_EQ(1, status_handler.call_count());
  EXPECT_EQ(hci::Status(HostError::kFailed), status_handler.status().value());
}

TEST_F(GAP_PairingStateTest, AuthenticationCompleteWithErrorCodeReceivedEarlyFailsPairing) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  NoOpPairingDelegate pairing_delegate(sm::IOCapability::kNoInputNoOutput);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  pairing_state.InitiatePairing(kNoSecurityRequirements, NoOpStatusCallback);

  EXPECT_EQ(std::nullopt, pairing_state.OnLinkKeyRequest(kPeerAddress));
  EXPECT_EQ(0, status_handler.call_count());

  const auto status_code = hci::StatusCode::kAuthenticationFailure;
  pairing_state.OnAuthenticationComplete(status_code);
  ASSERT_EQ(1, status_handler.call_count());
  EXPECT_EQ(hci::Status(status_code), status_handler.status().value());
}

TEST_F(GAP_PairingStateTest,
       MultipleQueuedPairingRequestsWithSameSecurityRequirementsCompleteAtSameTimeWithSuccess) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  NoOpPairingDelegate pairing_delegate(sm::IOCapability::kNoInputNoOutput);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  TestStatusHandler initiate_status_handler_0;
  pairing_state.InitiatePairing(kNoSecurityRequirements,
                                initiate_status_handler_0.MakeStatusCallback());
  EXPECT_EQ(1u, auth_request_count());

  TestStatusHandler initiate_status_handler_1;
  pairing_state.InitiatePairing(kNoSecurityRequirements,
                                initiate_status_handler_1.MakeStatusCallback());
  EXPECT_EQ(1u, auth_request_count());

  AdvanceToEncryptionAsInitiator(&pairing_state);
  EXPECT_EQ(0, status_handler.call_count());
  EXPECT_EQ(1, connection.start_encryption_count());

  connection.TriggerEncryptionChangeCallback(hci::Status(), true);
  EXPECT_EQ(1, status_handler.call_count());
  ASSERT_TRUE(status_handler.status());
  EXPECT_TRUE(status_handler.status()->is_success());
  ASSERT_EQ(1, initiate_status_handler_0.call_count());
  EXPECT_TRUE(initiate_status_handler_0.status()->is_success());
  ASSERT_EQ(1, initiate_status_handler_1.call_count());
  EXPECT_TRUE(initiate_status_handler_1.status()->is_success());
}

TEST_F(
    GAP_PairingStateTest,
    MultipleQueuedPairingRequestsWithAuthSecurityRequirementsCompleteAtSameTimeWithInsufficientSecurityFailure) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  NoOpPairingDelegate pairing_delegate(sm::IOCapability::kNoInputNoOutput);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  constexpr BrEdrSecurityRequirements kSecurityRequirements{.authentication = true,
                                                            .secure_connections = false};

  TestStatusHandler initiate_status_handler_0;
  pairing_state.InitiatePairing(kSecurityRequirements,
                                initiate_status_handler_0.MakeStatusCallback());
  EXPECT_EQ(1u, auth_request_count());

  TestStatusHandler initiate_status_handler_1;
  pairing_state.InitiatePairing(kSecurityRequirements,
                                initiate_status_handler_1.MakeStatusCallback());
  EXPECT_EQ(1u, auth_request_count());

  // Pair with unauthenticated link key.
  AdvanceToEncryptionAsInitiator(&pairing_state);
  EXPECT_EQ(0, status_handler.call_count());
  EXPECT_EQ(1, connection.start_encryption_count());

  connection.TriggerEncryptionChangeCallback(hci::Status(), true);
  EXPECT_EQ(1, status_handler.call_count());
  ASSERT_TRUE(status_handler.status());
  EXPECT_TRUE(status_handler.status()->is_success());
  ASSERT_EQ(1, initiate_status_handler_0.call_count());
  EXPECT_EQ(hci::Status(HostError::kInsufficientSecurity),
            initiate_status_handler_0.status().value());
  ASSERT_EQ(1, initiate_status_handler_1.call_count());
  EXPECT_EQ(hci::Status(HostError::kInsufficientSecurity),
            initiate_status_handler_1.status().value());
}

TEST_F(GAP_PairingStateTest,
       AuthPairingRequestDuringInitiatorNoAuthPairingFailsQueuedAuthPairingRequest) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  NoOpPairingDelegate pairing_delegate_no_io(sm::IOCapability::kNoInputNoOutput);
  pairing_state.SetPairingDelegate(pairing_delegate_no_io.GetWeakPtr());

  TestStatusHandler initiate_status_handler_0;
  pairing_state.InitiatePairing(kNoSecurityRequirements,
                                initiate_status_handler_0.MakeStatusCallback());

  TestStatusHandler initiate_status_handler_1;
  constexpr BrEdrSecurityRequirements kSecurityRequirements{.authentication = true,
                                                            .secure_connections = false};
  pairing_state.InitiatePairing(kSecurityRequirements,
                                initiate_status_handler_1.MakeStatusCallback());

  // Pair with unauthenticated link key.
  AdvanceToEncryptionAsInitiator(&pairing_state);

  EXPECT_EQ(0, status_handler.call_count());
  EXPECT_EQ(1, connection.start_encryption_count());

  FakePairingDelegate fake_pairing_delegate(sm::IOCapability::kDisplayYesNo);
  pairing_state.SetPairingDelegate(fake_pairing_delegate.GetWeakPtr());

  connection.TriggerEncryptionChangeCallback(hci::Status(), true);
  EXPECT_EQ(1, status_handler.call_count());
  ASSERT_TRUE(status_handler.status());
  EXPECT_TRUE(status_handler.status()->is_success());
  ASSERT_EQ(1, initiate_status_handler_0.call_count());
  EXPECT_TRUE(initiate_status_handler_0.status()->is_success());
  ASSERT_EQ(1, initiate_status_handler_1.call_count());
  EXPECT_EQ(hci::Status(HostError::kInsufficientSecurity),
            initiate_status_handler_1.status().value());

  // Pairing for second request should not start.
  EXPECT_FALSE(pairing_state.initiator());
}

TEST_F(GAP_PairingStateTest,
       InitiatingPairingDuringAuthenticationWithExistingUnauthenticatedLinkKey) {
  TestStatusHandler status_handler;
  auto connection = MakeFakeConnection();
  PairingState pairing_state(kTestPeerId, &connection, peer_cache(), MakeAuthRequestCallback(),
                             status_handler.MakeStatusCallback());
  FakePairingDelegate fake_pairing_delegate(sm::IOCapability::kDisplayYesNo);
  pairing_state.SetPairingDelegate(fake_pairing_delegate.GetWeakPtr());

  peer()->MutBrEdr().SetBondData(
      sm::LTK(sm::SecurityProperties(kTestUnauthenticatedLinkKeyType), kTestLinkKey));

  TestStatusHandler initiator_status_handler_0;
  pairing_state.InitiatePairing(kNoSecurityRequirements,
                                initiator_status_handler_0.MakeStatusCallback());
  EXPECT_EQ(1u, auth_request_count());

  TestStatusHandler initiator_status_handler_1;
  constexpr BrEdrSecurityRequirements kSecurityRequirements{.authentication = true,
                                                            .secure_connections = false};
  pairing_state.InitiatePairing(kSecurityRequirements,
                                initiator_status_handler_1.MakeStatusCallback());
  EXPECT_EQ(1u, auth_request_count());

  // Authenticate with link key.
  EXPECT_NE(std::nullopt, pairing_state.OnLinkKeyRequest(kPeerAddress));
  EXPECT_TRUE(connection.ltk().has_value());
  pairing_state.OnAuthenticationComplete(hci::StatusCode::kSuccess);

  EXPECT_EQ(0, status_handler.call_count());
  EXPECT_EQ(1, connection.start_encryption_count());

  connection.TriggerEncryptionChangeCallback(hci::Status(), true);
  ASSERT_EQ(1, status_handler.call_count());
  EXPECT_TRUE(status_handler.status()->is_success());
  ASSERT_EQ(1, initiator_status_handler_0.call_count());
  EXPECT_TRUE(initiator_status_handler_0.status()->is_success());
  EXPECT_EQ(0, initiator_status_handler_1.call_count());

  fake_pairing_delegate.SetDisplayPasskeyCallback([](PeerId peer_id, uint32_t value,
                                                     PairingDelegate::DisplayMethod method,
                                                     auto cb) { cb(true); });
  fake_pairing_delegate.SetCompletePairingCallback(
      [](PeerId peer_id, sm::Status status) { EXPECT_TRUE(status); });

  // Pairing for second request should start.
  EXPECT_EQ(2u, auth_request_count());
  EXPECT_TRUE(pairing_state.initiator());
  EXPECT_EQ(std::nullopt, pairing_state.OnLinkKeyRequest(kPeerAddress));
  EXPECT_EQ(IOCapability::kDisplayYesNo, *pairing_state.OnIoCapabilityRequest());
  pairing_state.OnIoCapabilityResponse(IOCapability::kDisplayYesNo);

  bool confirmed = false;
  pairing_state.OnUserConfirmationRequest(kTestPasskey,
                                          [&confirmed](bool confirm) { confirmed = confirm; });
  EXPECT_TRUE(confirmed);

  pairing_state.OnSimplePairingComplete(hci::StatusCode::kSuccess);
  pairing_state.OnLinkKeyNotification(kTestLinkKeyValue, kTestAuthenticatedLinkKeyType);
  pairing_state.OnAuthenticationComplete(hci::StatusCode::kSuccess);
  EXPECT_EQ(2, connection.start_encryption_count());

  connection.TriggerEncryptionChangeCallback(hci::Status(), true);
  ASSERT_EQ(2, status_handler.call_count());
  EXPECT_TRUE(status_handler.status()->is_success());
  EXPECT_EQ(1, initiator_status_handler_0.call_count());
  ASSERT_EQ(1, initiator_status_handler_1.call_count());
  EXPECT_TRUE(initiator_status_handler_1.status()->is_success());

  // No further pairing should occur.
  EXPECT_EQ(2u, auth_request_count());
  EXPECT_FALSE(pairing_state.initiator());
}

}  // namespace
}  // namespace bt::gap
