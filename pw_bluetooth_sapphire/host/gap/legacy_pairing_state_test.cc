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

#include <gmock/gmock.h>

#include "pw_bluetooth_sapphire/internal/host/gap/peer_cache.h"
#include "pw_bluetooth_sapphire/internal/host/hci/fake_bredr_connection.h"
#include "pw_bluetooth_sapphire/internal/host/sm/types.h"
#include "pw_bluetooth_sapphire/internal/host/testing/controller_test.h"
#include "pw_bluetooth_sapphire/internal/host/testing/inspect_util.h"
#include "pw_bluetooth_sapphire/internal/host/testing/mock_controller.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_helpers.h"
#include "pw_bluetooth_sapphire/internal/host/testing/test_packets.h"
#include "pw_bluetooth_sapphire/internal/host/transport/error.h"
#include "pw_unit_test/framework.h"

namespace bt::gap {
namespace {

using namespace inspect::testing;

using hci::testing::FakeBrEdrConnection;

const hci_spec::ConnectionHandle kTestHandle(0x0A0B);
const DeviceAddress kLocalAddress(DeviceAddress::Type::kBREDR,
                                  {0x22, 0x11, 0x00, 0xCC, 0xBB, 0xAA});
const DeviceAddress kPeerAddress(DeviceAddress::Type::kBREDR,
                                 {0x99, 0x88, 0x77, 0xFF, 0xEE, 0xDD});
const sm::IOCapability kTestLocalIoCap = sm::IOCapability::kDisplayYesNo;
const auto kTestLinkKeyValue = UInt128{0x00,
                                       0x00,
                                       0x00,
                                       0x00,
                                       0x00,
                                       0x00,
                                       0x00,
                                       0x00,
                                       0x00,
                                       0x00,
                                       0x00,
                                       0x00,
                                       0x00,
                                       0x00,
                                       0x00,
                                       0x01};
const hci_spec::LinkKey kTestLinkKey(kTestLinkKeyValue, 0, 0);
const hci_spec::LinkKeyType kTestLegacyLinkKeyType =
    hci_spec::LinkKeyType::kCombination;

void NoOpStatusCallback(hci_spec::ConnectionHandle, hci::Result<>) {}

class NoOpPairingDelegate final : public PairingDelegate {
 public:
  explicit NoOpPairingDelegate(sm::IOCapability io_capability)
      : io_capability_(io_capability), weak_self_(this) {}

  PairingDelegate::WeakPtr GetWeakPtr() { return weak_self_.GetWeakPtr(); }

  // PairingDelegate overrides that do nothing.
  ~NoOpPairingDelegate() override = default;
  sm::IOCapability io_capability() const override { return io_capability_; }
  void CompletePairing(PeerId peer_id, sm::Result<> status) override {}
  void ConfirmPairing(PeerId peer_id, ConfirmCallback confirm) override {}
  void DisplayPasskey(PeerId peer_id,
                      uint32_t passkey,
                      DisplayMethod method,
                      ConfirmCallback confirm) override {}
  void RequestPasskey(PeerId peer_id,
                      PasskeyResponseCallback respond) override {}

 private:
  const sm::IOCapability io_capability_;
  WeakSelf<PairingDelegate> weak_self_;
};

using TestBase = testing::FakeDispatcherControllerTest<testing::MockController>;
class LegacyPairingStateTest : public TestBase {
 public:
  LegacyPairingStateTest() = default;
  virtual ~LegacyPairingStateTest() = default;

  void SetUp() override {
    TestBase::SetUp();
    InitializeACLDataChannel();

    peer_cache_ = std::make_unique<PeerCache>(dispatcher());
    peer_ = peer_cache_->NewPeer(kPeerAddress, /*connectable=*/true);

    auth_request_count_ = 0;
    send_auth_request_callback_ = [this]() { auth_request_count_++; };

    connection_ = std::make_unique<FakeBrEdrConnection>(
        kTestHandle,
        kLocalAddress,
        kPeerAddress,
        pw::bluetooth::emboss::ConnectionRole::CENTRAL,
        transport()->GetWeakPtr());
  }

  void TearDown() override {
    peer_ = nullptr;
    peer_cache_ = nullptr;

    EXPECT_CMD_PACKET_OUT(test_device(),
                          testing::DisconnectPacket(kTestHandle));
    connection_.reset();

    TestBase::TearDown();
  }

  fit::closure MakeAuthRequestCallback() {
    return send_auth_request_callback_.share();
  }

  PeerCache* peer_cache() const { return peer_cache_.get(); }

  Peer* peer() const { return peer_; }

  // Incomplete connection used to create LegacyPairingState in
  // *BeforeAclConnectionCompletes tests
  FakeBrEdrConnection::WeakPtr incomplete_connection() const {
    return FakeBrEdrConnection::WeakPtr();
  }

  FakeBrEdrConnection* connection() const { return connection_.get(); }

  uint8_t auth_request_count() const { return auth_request_count_; }

 private:
  std::unique_ptr<PeerCache> peer_cache_;
  Peer* peer_;
  std::unique_ptr<FakeBrEdrConnection> connection_;
  uint8_t auth_request_count_;
  fit::closure send_auth_request_callback_;
};

// Test helper to inspect StatusCallback invocations.
class TestStatusHandler final {
 public:
  auto MakeStatusCallback() {
    return [this](hci_spec::ConnectionHandle handle, hci::Result<> status) {
      call_count_++;
      handle_ = handle;
      status_ = status;
    };
  }

  uint8_t call_count() const { return call_count_; }

  // Returns std::nullopt if |call_count_ < 1|, otherwise values from the most
  // recent callback invocation.
  auto& handle() const { return handle_; }
  auto& status() const { return status_; }

 private:
  uint8_t call_count_ = 0;
  std::optional<hci_spec::ConnectionHandle> handle_;
  std::optional<hci::Result<>> status_;
};

TEST_F(LegacyPairingStateTest, PairingStateStartsAsResponder) {
  LegacyPairingState pairing_state(peer()->GetWeakPtr(),
                                   connection()->GetWeakPtr(),
                                   /*outgoing_connection=*/false,
                                   MakeAuthRequestCallback(),
                                   NoOpStatusCallback);
  EXPECT_FALSE(pairing_state.initiator());
}

TEST_F(LegacyPairingStateTest,
       NeverInitiateLegacyPairingBeforeAclConnectionCompletes) {
  LegacyPairingState pairing_state(peer()->GetWeakPtr(),
                                   incomplete_connection(),
                                   /*outgoing_connection=*/false,
                                   MakeAuthRequestCallback(),
                                   NoOpStatusCallback);
  EXPECT_FALSE(pairing_state.initiator());

  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  pairing_state.InitiatePairing(NoOpStatusCallback);

  // |auth_cb| is only called if initiation was successful
  EXPECT_EQ(0, auth_request_count());
  EXPECT_FALSE(pairing_state.initiator());
}

TEST_F(LegacyPairingStateTest, NeverInitiateLegacyPairingWhenPeerSupportsSSP) {
  LegacyPairingState pairing_state(peer()->GetWeakPtr(),
                                   connection()->GetWeakPtr(),
                                   /*outgoing_connection=*/false,
                                   MakeAuthRequestCallback(),
                                   NoOpStatusCallback);
  EXPECT_FALSE(pairing_state.initiator());

  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  // Set peer's feature bits to indicate support for SSP
  peer()->SetFeaturePage(
      0,
      static_cast<uint64_t>(
          hci_spec::LMPFeature::kSecureSimplePairingControllerSupport));
  peer()->SetFeaturePage(
      1,
      static_cast<uint64_t>(
          hci_spec::LMPFeature::kSecureSimplePairingHostSupport));

  pairing_state.InitiatePairing(NoOpStatusCallback);

  // |auth_cb| is only called if initiation was successful
  EXPECT_EQ(0, auth_request_count());
  EXPECT_FALSE(pairing_state.initiator());
}

TEST_F(LegacyPairingStateTest,
       SkipPairingIfExistingKeyMeetsSecurityRequirements) {
  LegacyPairingState pairing_state(peer()->GetWeakPtr(),
                                   connection()->GetWeakPtr(),
                                   /*outgoing_connection=*/false,
                                   MakeAuthRequestCallback(),
                                   NoOpStatusCallback);
  EXPECT_FALSE(pairing_state.initiator());

  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  connection()->set_link_key(kTestLinkKey, kTestLegacyLinkKeyType);

  TestStatusHandler initiator_status_handler;
  pairing_state.InitiatePairing(initiator_status_handler.MakeStatusCallback());

  // |auth_cb| is only called if initiation was successful
  EXPECT_EQ(0, auth_request_count());
  EXPECT_FALSE(pairing_state.initiator());
  ASSERT_EQ(1, initiator_status_handler.call_count());
  EXPECT_EQ(fit::ok(), *initiator_status_handler.status());
}

TEST_F(
    LegacyPairingStateTest,
    PairingResponderOnLinkKeyRequestReturnsLinkKeyWhenBondDataExistsBeforeAclConnectionCompletes) {
  LegacyPairingState pairing_state(peer()->GetWeakPtr(),
                                   incomplete_connection(),
                                   /*outgoing_connection=*/false,
                                   MakeAuthRequestCallback(),
                                   NoOpStatusCallback);
  EXPECT_FALSE(pairing_state.initiator());

  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  peer()->MutBrEdr().SetBondData(
      sm::LTK(sm::SecurityProperties(kTestLegacyLinkKeyType), kTestLinkKey));
  EXPECT_FALSE(connection()->ltk().has_value());

  std::optional<hci_spec::LinkKey> reply_key = pairing_state.OnLinkKeyRequest();
  ASSERT_TRUE(reply_key.has_value());
  EXPECT_EQ(kTestLinkKey, reply_key.value());

  // Connection not complete yet so link key is stored in LegacyPairingState and
  // not the connection
  EXPECT_FALSE(connection()->ltk().has_value());
  EXPECT_TRUE(pairing_state.link_key().has_value());
  EXPECT_EQ(kTestLinkKey, pairing_state.link_key().value());
}

TEST_F(
    LegacyPairingStateTest,
    PairingResponderOnLinkKeyRequestReturnsLinkKeyWhenBondDataExistsAfterAclConnectionComplete) {
  LegacyPairingState pairing_state(peer()->GetWeakPtr(),
                                   connection()->GetWeakPtr(),
                                   /*outgoing_connection=*/false,
                                   MakeAuthRequestCallback(),
                                   NoOpStatusCallback);
  EXPECT_FALSE(pairing_state.initiator());

  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  peer()->MutBrEdr().SetBondData(
      sm::LTK(sm::SecurityProperties(kTestLegacyLinkKeyType), kTestLinkKey));
  EXPECT_FALSE(connection()->ltk().has_value());

  std::optional<hci_spec::LinkKey> reply_key = pairing_state.OnLinkKeyRequest();
  ASSERT_TRUE(reply_key.has_value());
  EXPECT_EQ(kTestLinkKey, reply_key.value());

  // Connection was complete so link key was stored in connection
  EXPECT_TRUE(connection()->ltk().has_value());
  EXPECT_FALSE(pairing_state.link_key().has_value());
}

TEST_F(
    LegacyPairingStateTest,
    PairingInitiatorOnLinkKeyRequestReturnsReturnsLinkKeyWhenBondDataExists) {
  LegacyPairingState pairing_state(peer()->GetWeakPtr(),
                                   connection()->GetWeakPtr(),
                                   /*outgoing_connection=*/false,
                                   MakeAuthRequestCallback(),
                                   NoOpStatusCallback);
  EXPECT_FALSE(pairing_state.initiator());

  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  peer()->MutBrEdr().SetBondData(
      sm::LTK(sm::SecurityProperties(kTestLegacyLinkKeyType), kTestLinkKey));
  EXPECT_FALSE(connection()->ltk().has_value());

  pairing_state.InitiatePairing(NoOpStatusCallback);
  EXPECT_EQ(1, auth_request_count());
  EXPECT_TRUE(pairing_state.initiator());

  std::optional<hci_spec::LinkKey> reply_key = pairing_state.OnLinkKeyRequest();
  ASSERT_TRUE(reply_key.has_value());
  EXPECT_EQ(kTestLinkKey, reply_key.value());

  // Connection was complete so link key was stored in the connection
  EXPECT_TRUE(connection()->ltk().has_value());
  EXPECT_FALSE(pairing_state.link_key().has_value());
}

TEST_F(
    LegacyPairingStateTest,
    PairingResponderOnLinkKeyRequestReturnsNullWhenBondDataDoesNotExistBeforeAclComplete) {
  LegacyPairingState pairing_state(peer()->GetWeakPtr(),
                                   incomplete_connection(),
                                   /*outgoing_connection=*/false,
                                   MakeAuthRequestCallback(),
                                   NoOpStatusCallback);
  EXPECT_FALSE(pairing_state.initiator());

  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  std::optional<hci_spec::LinkKey> reply_key = pairing_state.OnLinkKeyRequest();
  EXPECT_FALSE(reply_key.has_value());
}

TEST_F(
    LegacyPairingStateTest,
    PairingResponderOnLinkKeyRequestReturnsNullWhenBondDataDoesNotExistAfterAclComplete) {
  LegacyPairingState pairing_state(peer()->GetWeakPtr(),
                                   connection()->GetWeakPtr(),
                                   /*outgoing_connection=*/false,
                                   MakeAuthRequestCallback(),
                                   NoOpStatusCallback);
  EXPECT_FALSE(pairing_state.initiator());

  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  std::optional<hci_spec::LinkKey> reply_key = pairing_state.OnLinkKeyRequest();
  EXPECT_FALSE(reply_key.has_value());
}

TEST_F(LegacyPairingStateTest,
       PairingInitiatorOnLinkKeyRequestReturnsNullWhenBondDataDoesNotExist) {
  LegacyPairingState pairing_state(peer()->GetWeakPtr(),
                                   connection()->GetWeakPtr(),
                                   /*outgoing_connection=*/false,
                                   MakeAuthRequestCallback(),
                                   NoOpStatusCallback);
  EXPECT_FALSE(pairing_state.initiator());

  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  pairing_state.InitiatePairing(NoOpStatusCallback);
  EXPECT_EQ(1, auth_request_count());
  EXPECT_TRUE(pairing_state.initiator());

  std::optional<hci_spec::LinkKey> reply_key = pairing_state.OnLinkKeyRequest();
  EXPECT_FALSE(reply_key.has_value());
}

TEST_F(LegacyPairingStateTest, OnLinkKeyRequestReceivedMissingPeerAsserts) {
  LegacyPairingState pairing_state(peer()->GetWeakPtr(),
                                   connection()->GetWeakPtr(),
                                   /*outgoing_connection=*/false,
                                   MakeAuthRequestCallback(),
                                   NoOpStatusCallback);
  EXPECT_FALSE(pairing_state.initiator());

  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  pairing_state.InitiatePairing(NoOpStatusCallback);
  EXPECT_TRUE(pairing_state.initiator());

  EXPECT_TRUE(peer_cache()->RemoveDisconnectedPeer(peer()->identifier()));

  ASSERT_DEATH_IF_SUPPORTED(
      {
        [[maybe_unused]] std::optional<hci_spec::LinkKey> reply_key =
            pairing_state.OnLinkKeyRequest();
      },
      ".*peer.*");
}

TEST_F(
    LegacyPairingStateTest,
    PairingInitiatorFailsPairingWhenAuthenticationCompleteWithErrorCodeReceivedEarly) {
  TestStatusHandler status_handler;

  LegacyPairingState pairing_state(peer()->GetWeakPtr(),
                                   connection()->GetWeakPtr(),
                                   /*outgoing_connection=*/false,
                                   MakeAuthRequestCallback(),
                                   status_handler.MakeStatusCallback());
  EXPECT_FALSE(pairing_state.initiator());

  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  pairing_state.InitiatePairing(NoOpStatusCallback);
  EXPECT_TRUE(pairing_state.initiator());

  EXPECT_EQ(std::nullopt, pairing_state.OnLinkKeyRequest());
  EXPECT_EQ(0, status_handler.call_count());

  pairing_state.OnAuthenticationComplete(
      pw::bluetooth::emboss::StatusCode::AUTHENTICATION_FAILURE);
  ASSERT_EQ(1, status_handler.call_count());
  EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::AUTHENTICATION_FAILURE),
            status_handler.status().value());
}

TEST_F(LegacyPairingStateTest,
       InitatorPairingStateSendsAuthenticationRequestOnceForDuplicateRequest) {
  LegacyPairingState pairing_state(peer()->GetWeakPtr(),
                                   connection()->GetWeakPtr(),
                                   /*outgoing_connection=*/false,
                                   MakeAuthRequestCallback(),
                                   NoOpStatusCallback);
  EXPECT_FALSE(pairing_state.initiator());

  NoOpPairingDelegate pairing_delegate(kTestLocalIoCap);
  pairing_state.SetPairingDelegate(pairing_delegate.GetWeakPtr());

  pairing_state.InitiatePairing(NoOpStatusCallback);
  EXPECT_EQ(1, auth_request_count());
  EXPECT_TRUE(pairing_state.initiator());

  pairing_state.InitiatePairing(NoOpStatusCallback);
  EXPECT_EQ(1, auth_request_count());
  EXPECT_TRUE(pairing_state.initiator());
}

// Event injectors. Return values are necessarily ignored in order to make types
// match, so don't use these functions to test return values. Likewise,
// arguments have been filled with test defaults for a successful pairing flow.
void LinkKeyRequest(LegacyPairingState* pairing_state) {
  static_cast<void>(pairing_state->OnLinkKeyRequest());
}
void LinkKeyNotification(LegacyPairingState* pairing_state) {
  pairing_state->OnLinkKeyNotification(kTestLinkKeyValue,
                                       kTestLegacyLinkKeyType);
}
void AuthenticationComplete(LegacyPairingState* pairing_state) {
  pairing_state->OnAuthenticationComplete(
      pw::bluetooth::emboss::StatusCode::SUCCESS);
}

// Test suite fixture that genericizes an injected pairing state event. The
// event being tested should be retrieved with the GetParam method, which
// returns a default event injector. For example:
//
//   LegacyPairingState pairing_state;
//   GetParam()(&pairing_state);
//
// This is named so that the instantiated test description looks correct:
//
//   LegacyPairingStateTest/HandlesLegacyEvent.<test case>/<index of event>
class HandlesLegacyEvent
    : public LegacyPairingStateTest,
      public ::testing::WithParamInterface<void (*)(LegacyPairingState*)> {
 public:
  void SetUp() override {
    LegacyPairingStateTest::SetUp();

    pairing_delegate_ = std::make_unique<NoOpPairingDelegate>(kTestLocalIoCap);
    pairing_state_ = std::make_unique<LegacyPairingState>(
        peer()->GetWeakPtr(),
        connection()->GetWeakPtr(),
        /*outgoing_connection=*/false,
        MakeAuthRequestCallback(),
        status_handler_.MakeStatusCallback());
    pairing_state().SetPairingDelegate(pairing_delegate_->GetWeakPtr());
  }

  void TearDown() override {
    pairing_state_.reset();
    LegacyPairingStateTest::TearDown();
  }

  const TestStatusHandler& status_handler() const { return status_handler_; }
  LegacyPairingState& pairing_state() { return *pairing_state_; }

  // Returns an event injector that was passed to INSTANTIATE_TEST_SUITE_P.
  static auto* event() { return GetParam(); }

  void InjectEvent() { event()(&pairing_state()); }

 private:
  TestStatusHandler status_handler_;
  std::unique_ptr<NoOpPairingDelegate> pairing_delegate_;
  std::unique_ptr<LegacyPairingState> pairing_state_;
};

// The tests here and below exercise that LegacyPairingState successfully
// advances through the expected pairing flow and generates errors when the
// pairing flow occurs out of order. This is intended to cover its internal
// state machine transitions and not the side effects.
INSTANTIATE_TEST_SUITE_P(LegacyPairingStateTest,
                         HandlesLegacyEvent,
                         ::testing::Values(LinkKeyRequest,
                                           LinkKeyNotification,
                                           AuthenticationComplete));

TEST_P(HandlesLegacyEvent, InIdleState) {
  RETURN_IF_FATAL(InjectEvent());
  if (event() == LinkKeyRequest) {
    EXPECT_EQ(0, status_handler().call_count());
  } else {
    EXPECT_EQ(1, status_handler().call_count());
    ASSERT_TRUE(status_handler().handle());
    EXPECT_EQ(kTestHandle, *status_handler().handle());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(ToResult(HostError::kFailed), status_handler().status());
  }
}

TEST_P(HandlesLegacyEvent, InInitiatorWaitLinkKeyRequestState) {
  // Advance state machine
  pairing_state().InitiatePairing(NoOpStatusCallback);

  RETURN_IF_FATAL(InjectEvent());
  if (event() == LinkKeyRequest) {
    EXPECT_EQ(0, status_handler().call_count());
  } else {
    EXPECT_EQ(1, status_handler().call_count());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(ToResult(HostError::kFailed), status_handler().status());
  }
}

TEST_P(HandlesLegacyEvent, InInitiatorWaitAuthCompleteSkippingLegacyPairing) {
  peer()->MutBrEdr().SetBondData(
      sm::LTK(sm::SecurityProperties(kTestLegacyLinkKeyType), kTestLinkKey));

  // Advance state machine
  pairing_state().InitiatePairing(NoOpStatusCallback);
  EXPECT_NE(std::nullopt, pairing_state().OnLinkKeyRequest());

  RETURN_IF_FATAL(InjectEvent());
  if (event() == AuthenticationComplete) {
    EXPECT_EQ(0, status_handler().call_count());
    EXPECT_EQ(1, connection()->start_encryption_count());
  } else {
    EXPECT_EQ(1, status_handler().call_count());
    ASSERT_TRUE(status_handler().status());
    EXPECT_EQ(ToResult(HostError::kFailed), status_handler().status());
  }
}

TEST_F(LegacyPairingStateTest,
       TestStatusHandlerTracksStatusCallbackInvocations) {
  TestStatusHandler handler;
  EXPECT_EQ(0, handler.call_count());
  EXPECT_FALSE(handler.status());

  LegacyPairingState::StatusCallback status_cb = handler.MakeStatusCallback();
  EXPECT_EQ(0, handler.call_count());
  EXPECT_FALSE(handler.status());

  status_cb(kTestHandle,
            ToResult(pw::bluetooth::emboss::StatusCode::PAIRING_NOT_ALLOWED));
  EXPECT_EQ(1, handler.call_count());
  ASSERT_TRUE(handler.handle());
  EXPECT_EQ(kTestHandle, *handler.handle());
  ASSERT_TRUE(handler.status());
  EXPECT_EQ(ToResult(pw::bluetooth::emboss::StatusCode::PAIRING_NOT_ALLOWED),
            *handler.status());
}

#ifndef NINSPECT
TEST_F(LegacyPairingStateTest, Inspect) {
  TestStatusHandler status_handler;

  inspect::Inspector inspector;

  LegacyPairingState pairing_state(peer()->GetWeakPtr(),
                                   connection()->GetWeakPtr(),
                                   /*outgoing_connection=*/false,
                                   MakeAuthRequestCallback(),
                                   status_handler.MakeStatusCallback());

  pairing_state.AttachInspect(inspector.GetRoot(), "pairing_state");

  auto security_properties_matcher =
      AllOf(NodeMatches(AllOf(NameMatches("security_properties"),
                              PropertyList(UnorderedElementsAre(
                                  StringIs("level", "not secure"),
                                  BoolIs("encrypted", false),
                                  BoolIs("secure_connections", false),
                                  BoolIs("authenticated", false),
                                  StringIs("key_type", "kCombination"))))));

  auto pairing_state_matcher =
      AllOf(NodeMatches(AllOf(NameMatches("pairing_state"),
                              PropertyList(UnorderedElementsAre(
                                  StringIs("encryption_status", "OFF"))))),
            ChildrenMatch(UnorderedElementsAre(security_properties_matcher)));

  inspect::Hierarchy hierarchy = bt::testing::ReadInspect(inspector);
  EXPECT_THAT(hierarchy, ChildrenMatch(ElementsAre(pairing_state_matcher)));
}
#endif  // NINSPECT

}  // namespace
}  // namespace bt::gap
