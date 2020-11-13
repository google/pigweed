// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "profile_server.h"

#include <fuchsia/bluetooth/bredr/cpp/fidl_test_base.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/fidl/adapter_test_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/fake_pairing_delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_peer.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_packets.h"

namespace bthost {
namespace {

namespace fidlbredr = fuchsia::bluetooth::bredr;

namespace {

void NopAdvertiseCallback(fidlbredr::Profile_Advertise_Result){};

const bt::DeviceAddress kTestDevAddr(bt::DeviceAddress::Type::kBREDR, {1});
constexpr bt::l2cap::PSM kPSM = bt::l2cap::kAVDTP;

fidlbredr::ServiceDefinition MakeFIDLServiceDefinition() {
  fidlbredr::ServiceDefinition def;
  def.mutable_service_class_uuids()->emplace_back(
      fidl_helpers::UuidToFidl(bt::sdp::profile::kAudioSink));

  fidlbredr::ProtocolDescriptor l2cap_proto;
  l2cap_proto.protocol = fidlbredr::ProtocolIdentifier::L2CAP;
  fidlbredr::DataElement l2cap_data_el;
  l2cap_data_el.set_uint16(fidlbredr::PSM_AVDTP);
  l2cap_proto.params.emplace_back(std::move(l2cap_data_el));

  def.mutable_protocol_descriptor_list()->emplace_back(std::move(l2cap_proto));

  fidlbredr::ProtocolDescriptor avdtp_proto;
  avdtp_proto.protocol = fidlbredr::ProtocolIdentifier::AVDTP;
  fidlbredr::DataElement avdtp_data_el;
  avdtp_data_el.set_uint16(0x0103);  // Version 1.3
  avdtp_proto.params.emplace_back(std::move(avdtp_data_el));

  def.mutable_protocol_descriptor_list()->emplace_back(std::move(avdtp_proto));

  fidlbredr::ProfileDescriptor prof_desc;
  prof_desc.profile_id = fidlbredr::ServiceClassProfileIdentifier::ADVANCED_AUDIO_DISTRIBUTION;
  prof_desc.major_version = 1;
  prof_desc.minor_version = 3;
  def.mutable_profile_descriptors()->emplace_back(prof_desc);

  return def;
}

}  // namespace

using TestingBase = bthost::testing::AdapterTestFixture;
class FIDL_ProfileServerTest : public TestingBase {
 public:
  FIDL_ProfileServerTest() = default;
  ~FIDL_ProfileServerTest() override = default;

 protected:
  void SetUp() override {
    TestingBase::SetUp();
    fidl::InterfaceHandle<fidlbredr::Profile> profile_handle;
    client_.Bind(std::move(profile_handle));
    server_ =
        std::make_unique<ProfileServer>(adapter()->AsWeakPtr(), client_.NewRequest(dispatcher()));
  }

  void TearDown() override {
    RunLoopUntilIdle();
    client_ = nullptr;
    server_ = nullptr;
    TestingBase::TearDown();
  }

  ProfileServer* server() const { return server_.get(); }

  fidlbredr::ProfilePtr& client() { return client_; }

  bt::gap::PeerCache* peer_cache() const { return adapter()->peer_cache(); }

 private:
  std::unique_ptr<ProfileServer> server_;
  fidlbredr::ProfilePtr client_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FIDL_ProfileServerTest);
};

// TODO(fxbug.dev/64167): Replace GMock usage with homegrown mocks.
class MockConnectionReceiver : public fidlbredr::testing::ConnectionReceiver_TestBase {
 public:
  MockConnectionReceiver(fidl::InterfaceRequest<ConnectionReceiver> request,
                         async_dispatcher_t* dispatcher)
      : binding_(this, std::move(request), dispatcher) {}

  ~MockConnectionReceiver() override = default;

  MOCK_METHOD(void, Connected,
              (fuchsia::bluetooth::PeerId peer_id, fidlbredr::Channel channel,
               std::vector<fidlbredr::ProtocolDescriptor> protocol),
              (override));

 private:
  fidl::Binding<ConnectionReceiver> binding_;

  void NotImplemented_(const std::string& name) override {
    FAIL() << name << " is not implemented";
  }
};

TEST_F(FIDL_ProfileServerTest, ErrorOnInvalidDefinition) {
  fidl::InterfaceHandle<fuchsia::bluetooth::bredr::ConnectionReceiver> receiver_handle;
  auto request = receiver_handle.NewRequest();

  std::vector<fidlbredr::ServiceDefinition> services;
  fidlbredr::ServiceDefinition def;
  // Empty service definition is not allowed - it must contain at least a service UUID.

  services.emplace_back(std::move(def));

  auto cb = [](auto response) {
    EXPECT_TRUE(response.is_err());
    EXPECT_EQ(response.err(), fuchsia::bluetooth::ErrorCode::INVALID_ARGUMENTS);
  };

  client()->Advertise(std::move(services), fidlbredr::ChannelParameters(),
                      std::move(receiver_handle), std::move(cb));

  RunLoopUntilIdle();

  // Server should close because it's not a good definition.
  zx_signals_t signals;
  request.channel().wait_one(ZX_CHANNEL_PEER_CLOSED, zx::time(0), &signals);
  EXPECT_TRUE(signals & ZX_CHANNEL_PEER_CLOSED);
}

TEST_F(FIDL_ProfileServerTest, ErrorOnMultipleAdvertiseRequests) {
  fidl::InterfaceHandle<fuchsia::bluetooth::bredr::ConnectionReceiver> receiver_handle1;
  auto request1 = receiver_handle1.NewRequest();

  std::vector<fidlbredr::ServiceDefinition> services1;
  services1.emplace_back(MakeFIDLServiceDefinition());

  // First callback should never be called since the first advertisement is valid.
  size_t cb1_count = 0;
  auto cb1 = [&](auto) { cb1_count++; };

  client()->Advertise(std::move(services1), fidlbredr::ChannelParameters(),
                      std::move(receiver_handle1), std::move(cb1));

  RunLoopUntilIdle();

  ASSERT_EQ(cb1_count, 0u);

  fidl::InterfaceHandle<fuchsia::bluetooth::bredr::ConnectionReceiver> receiver_handle2;
  auto request2 = receiver_handle2.NewRequest();

  std::vector<fidlbredr::ServiceDefinition> services2;
  services2.emplace_back(MakeFIDLServiceDefinition());

  // Second callback should error because the second advertisement is requesting a taken PSM.
  size_t cb2_count = 0;
  auto cb2 = [&](auto response) {
    cb2_count++;
    EXPECT_TRUE(response.is_err());
    EXPECT_EQ(response.err(), fuchsia::bluetooth::ErrorCode::INVALID_ARGUMENTS);
  };

  client()->Advertise(std::move(services2), fidlbredr::ChannelParameters(),
                      std::move(receiver_handle2), std::move(cb2));

  RunLoopUntilIdle();

  ASSERT_EQ(cb1_count, 0u);
  ASSERT_EQ(cb2_count, 1u);

  // Second channel should close.
  zx_signals_t signals;
  request2.channel().wait_one(ZX_CHANNEL_PEER_CLOSED, zx::time(0), &signals);
  EXPECT_TRUE(signals & ZX_CHANNEL_PEER_CLOSED);

  // Unregister the first advertisement.
  request1 = receiver_handle1.NewRequest();
  RunLoopUntilIdle();
  EXPECT_EQ(cb1_count, 1u);
}

TEST_F(FIDL_ProfileServerTest, ErrorOnInvalidConnectParametersNoPSM) {
  // Random peer, since we don't expect the connection.
  fuchsia::bluetooth::PeerId peer_id{123};

  // No PSM provided - this is invalid.
  fidlbredr::L2capParameters l2cap_params;
  fidlbredr::ChannelParameters channel_params;
  l2cap_params.set_parameters(std::move(channel_params));

  fidlbredr::ConnectParameters connection;
  connection.set_l2cap(std::move(l2cap_params));

  // Expect an error result.
  auto sock_cb = [](auto result) {
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.err(), fuchsia::bluetooth::ErrorCode::INVALID_ARGUMENTS);
  };

  client()->Connect(peer_id, std::move(connection), std::move(sock_cb));
  RunLoopUntilIdle();
}

TEST_F(FIDL_ProfileServerTest, ErrorOnInvalidConnectParametersRfcomm) {
  // Random peer, since we don't expect the connection.
  fuchsia::bluetooth::PeerId peer_id{123};

  // RFCOMM Parameters are provided - this is not supported.
  fidlbredr::RfcommParameters rfcomm_params;
  fidlbredr::ConnectParameters connection;
  connection.set_rfcomm(std::move(rfcomm_params));

  // Expect an error result.
  auto sock_cb = [](auto result) {
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.err(), fuchsia::bluetooth::ErrorCode::INVALID_ARGUMENTS);
  };

  client()->Connect(peer_id, std::move(connection), std::move(sock_cb));
  RunLoopUntilIdle();
}

TEST_F(FIDL_ProfileServerTest, UnregisterAdvertisementTriggersCallback) {
  fidl::InterfaceHandle<fuchsia::bluetooth::bredr::ConnectionReceiver> receiver_handle;
  auto request = receiver_handle.NewRequest();

  std::vector<fidlbredr::ServiceDefinition> services;
  services.emplace_back(MakeFIDLServiceDefinition());

  size_t cb_count = 0;
  auto cb = [&](auto result) {
    cb_count++;
    EXPECT_TRUE(result.is_response());
  };

  client()->Advertise(std::move(services), fidlbredr::ChannelParameters(),
                      std::move(receiver_handle), std::move(cb));

  RunLoopUntilIdle();

  // Advertisement is still active, callback shouldn't get triggered.
  ASSERT_EQ(cb_count, 0u);

  // Overwrite the server end of the ConnectionReceiver.
  request = receiver_handle.NewRequest();
  RunLoopUntilIdle();

  // Profile server should drop the advertisement and notify the callback of termination.
  ASSERT_EQ(cb_count, 1u);
}

class FIDL_ProfileServerTest_ConnectedPeer : public FIDL_ProfileServerTest {
 public:
  FIDL_ProfileServerTest_ConnectedPeer() = default;
  ~FIDL_ProfileServerTest_ConnectedPeer() override = default;

 protected:
  void SetUp() override {
    FIDL_ProfileServerTest::SetUp();
    peer_ = peer_cache()->NewPeer(kTestDevAddr, true);
    auto fake_peer = std::make_unique<bt::testing::FakePeer>(kTestDevAddr);
    test_device()->AddPeer(std::move(fake_peer));

    bt::testing::FakeController::Settings settings;
    settings.ApplyDualModeDefaults();
    test_device()->set_settings(settings);

    bt::hci::Status status(bt::HostError::kFailed);
    auto connect_cb = [this, &status](auto cb_status, auto cb_conn_ref) {
      ASSERT_TRUE(cb_conn_ref);
      status = cb_status;
      connection_ = std::move(cb_conn_ref);
    };

    EXPECT_TRUE(adapter()->bredr()->Connect(peer_->identifier(), connect_cb));
    EXPECT_EQ(bt::gap::Peer::ConnectionState::kInitializing, peer_->bredr()->connection_state());

    RunLoopUntilIdle();
    EXPECT_TRUE(status);
    ASSERT_TRUE(connection_);
    EXPECT_EQ(peer_->identifier(), connection_->peer_id());
    EXPECT_EQ(bt::gap::Peer::ConnectionState::kConnected, peer_->bredr()->connection_state());
  }

  void TearDown() override {
    connection_ = nullptr;
    peer_ = nullptr;
    FIDL_ProfileServerTest::TearDown();
  }

  bt::gap::BrEdrConnection* connection() const { return connection_; }

  bt::gap::Peer* peer() const { return peer_; }

 private:
  bt::gap::BrEdrConnection* connection_;
  bt::gap::Peer* peer_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FIDL_ProfileServerTest_ConnectedPeer);
};

TEST_F(FIDL_ProfileServerTest_ConnectedPeer, ConnectL2capChannelParameters) {
  auto pairing_delegate =
      std::make_unique<bt::gap::FakePairingDelegate>(bt::sm::IOCapability::kDisplayYesNo);
  adapter()->SetPairingDelegate(pairing_delegate->GetWeakPtr());
  // Approve pairing requests.
  pairing_delegate->SetConfirmPairingCallback(
      [](bt::PeerId, auto confirm_cb) { confirm_cb(true); });
  pairing_delegate->SetCompletePairingCallback(
      [&](bt::PeerId, bt::sm::Status status) { EXPECT_TRUE(status.is_success()); });

  bt::l2cap::ChannelParameters expected_params;
  expected_params.mode = bt::l2cap::ChannelMode::kEnhancedRetransmission;
  expected_params.max_rx_sdu_size = bt::l2cap::kMinACLMTU;
  l2cap()->ExpectOutboundL2capChannel(connection()->link().handle(), kPSM, 0x40, 0x41,
                                      expected_params);

  fidlbredr::ChannelParameters fidl_params;
  fidl_params.set_channel_mode(fidlbredr::ChannelMode::ENHANCED_RETRANSMISSION);
  fidl_params.set_max_rx_sdu_size(bt::l2cap::kMinACLMTU);

  // Expect a non-empty channel result.
  std::optional<fidlbredr::Channel> channel;
  auto chan_cb = [&channel](auto result) {
    EXPECT_TRUE(result.is_response());
    channel = std::move(result.response().channel);
  };
  // Initiates pairing

  fuchsia::bluetooth::PeerId peer_id{peer()->identifier().value()};

  fidlbredr::L2capParameters l2cap_params;
  l2cap_params.set_psm(kPSM);
  l2cap_params.set_parameters(std::move(fidl_params));

  fidlbredr::ConnectParameters connection;
  connection.set_l2cap(std::move(l2cap_params));

  client()->Connect(peer_id, std::move(connection), std::move(chan_cb));
  RunLoopUntilIdle();

  ASSERT_TRUE(channel.has_value());
  EXPECT_TRUE(channel->has_socket());
  EXPECT_FALSE(channel->IsEmpty());
  EXPECT_EQ(channel->channel_mode(), fidl_params.channel_mode());
  // FakeL2cap returns channels with max tx sdu size of kDefaultMTU.
  EXPECT_EQ(channel->max_tx_sdu_size(), bt::l2cap::kDefaultMTU);
  EXPECT_FALSE(channel->has_ext_direction());
}

TEST_F(FIDL_ProfileServerTest_ConnectedPeer,
       ConnectWithAuthenticationRequiredButLinkKeyNotAuthenticatedFails) {
  auto pairing_delegate =
      std::make_unique<bt::gap::FakePairingDelegate>(bt::sm::IOCapability::kNoInputNoOutput);
  adapter()->SetPairingDelegate(pairing_delegate->GetWeakPtr());
  pairing_delegate->SetCompletePairingCallback(
      [&](bt::PeerId, bt::sm::Status status) { EXPECT_TRUE(status.is_success()); });

  fidlbredr::SecurityRequirements security;
  security.set_authentication_required(true);
  fidlbredr::ChannelParameters chan_params;
  chan_params.set_security_requirements(std::move(security));
  fidlbredr::L2capParameters l2cap_params;
  l2cap_params.set_psm(kPSM);
  l2cap_params.set_parameters(std::move(chan_params));
  fidlbredr::ConnectParameters conn_params;
  conn_params.set_l2cap(std::move(l2cap_params));

  size_t sock_cb_count = 0;
  auto sock_cb = [&](auto result) {
    sock_cb_count++;
    ASSERT_TRUE(result.is_err());
    EXPECT_EQ(fuchsia::bluetooth::ErrorCode::FAILED, result.err());
  };

  fuchsia::bluetooth::PeerId peer_id{peer()->identifier().value()};

  // Initiates pairing.
  // FakeController will create an unauthenticated key.
  client()->Connect(peer_id, std::move(conn_params), std::move(sock_cb));
  RunLoopUntilIdle();

  EXPECT_EQ(1u, sock_cb_count);
}

// Tests receiving an empty Channel results in an error propagated through the callback.
TEST_F(FIDL_ProfileServerTest_ConnectedPeer, ConnectEmptyChannelResponse) {
  auto pairing_delegate =
      std::make_unique<bt::gap::FakePairingDelegate>(bt::sm::IOCapability::kDisplayYesNo);
  adapter()->SetPairingDelegate(pairing_delegate->GetWeakPtr());
  // Approve pairing requests.
  pairing_delegate->SetConfirmPairingCallback(
      [](bt::PeerId, auto confirm_cb) { confirm_cb(true); });
  pairing_delegate->SetCompletePairingCallback(
      [&](bt::PeerId, bt::sm::Status status) { EXPECT_TRUE(status.is_success()); });

  // Make the l2cap channel creation fail.
  l2cap()->set_simulate_open_channel_failure(true);

  bt::l2cap::ChannelParameters expected_params;
  expected_params.mode = bt::l2cap::ChannelMode::kEnhancedRetransmission;
  expected_params.max_rx_sdu_size = bt::l2cap::kMinACLMTU;
  l2cap()->ExpectOutboundL2capChannel(connection()->link().handle(), kPSM, 0x40, 0x41,
                                      expected_params);

  fidlbredr::ChannelParameters fidl_params;
  fidl_params.set_channel_mode(fidlbredr::ChannelMode::ENHANCED_RETRANSMISSION);
  fidl_params.set_max_rx_sdu_size(bt::l2cap::kMinACLMTU);
  auto sock_cb = [](auto result) {
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(fuchsia::bluetooth::ErrorCode::FAILED, result.err());
  };
  // Initiates pairing

  fuchsia::bluetooth::PeerId peer_id{peer()->identifier().value()};

  fidlbredr::L2capParameters l2cap_params;
  l2cap_params.set_psm(kPSM);
  l2cap_params.set_parameters(std::move(fidl_params));

  fidlbredr::ConnectParameters connection;
  connection.set_l2cap(std::move(l2cap_params));

  client()->Connect(peer_id, std::move(connection), std::move(sock_cb));
  RunLoopUntilIdle();
}

TEST_F(FIDL_ProfileServerTest_ConnectedPeer,
       AdvertiseChannelParametersReceivedInOnChannelConnectedCallback) {
  fidlbredr::ChannelParameters fidl_chan_params;
  fidl_chan_params.set_channel_mode(fidlbredr::ChannelMode::ENHANCED_RETRANSMISSION);

  constexpr uint16_t kTxMtu = bt::l2cap::kMinACLMTU;

  auto pairing_delegate =
      std::make_unique<bt::gap::FakePairingDelegate>(bt::sm::IOCapability::kDisplayYesNo);
  adapter()->SetPairingDelegate(pairing_delegate->GetWeakPtr());

  using ::testing::StrictMock;
  fidl::InterfaceHandle<fidlbredr::ConnectionReceiver> connect_receiver_handle;
  auto connect_receiver = std::make_unique<StrictMock<MockConnectionReceiver>>(
      connect_receiver_handle.NewRequest(), dispatcher());

  std::vector<fidlbredr::ServiceDefinition> services;
  services.emplace_back(MakeFIDLServiceDefinition());

  client()->Advertise(std::move(services), std::move(fidl_chan_params),
                      std::move(connect_receiver_handle), NopAdvertiseCallback);
  RunLoopUntilIdle();

  EXPECT_CALL(*connect_receiver, Connected(::testing::_, ::testing::_, ::testing::_))
      .WillOnce([expected_id = peer()->identifier(), kTxMtu](fuchsia::bluetooth::PeerId peer_id,
                                                             fidlbredr::Channel channel,
                                                             ::testing::Unused) {
        ASSERT_EQ(expected_id.value(), peer_id.value);
        ASSERT_TRUE(channel.has_socket());
        EXPECT_EQ(channel.channel_mode(), fidlbredr::ChannelMode::ENHANCED_RETRANSMISSION);
        EXPECT_EQ(channel.max_tx_sdu_size(), kTxMtu);
        EXPECT_FALSE(channel.has_ext_direction());
      });

  EXPECT_TRUE(
      l2cap()->TriggerInboundL2capChannel(connection()->link().handle(), kPSM, 0x40, 0x41, kTxMtu));
  RunLoopUntilIdle();
}

class AclPrioritySupportedTest : public FIDL_ProfileServerTest_ConnectedPeer {
 public:
  void SetUp() override {
    set_vendor_features(BT_VENDOR_FEATURES_SET_ACL_PRIORITY_COMMAND);
    FIDL_ProfileServerTest_ConnectedPeer::SetUp();
  }
};

class PriorityTest
    : public AclPrioritySupportedTest,
      public ::testing::WithParamInterface<std::pair<fidlbredr::A2dpDirectionPriority, bool>> {};

TEST_P(PriorityTest, OutboundConnectAndSetPriority) {
  const auto kPriority = GetParam().first;
  const bool kExpectSuccess = GetParam().second;

  auto pairing_delegate =
      std::make_unique<bt::gap::FakePairingDelegate>(bt::sm::IOCapability::kDisplayYesNo);
  adapter()->SetPairingDelegate(pairing_delegate->GetWeakPtr());
  // Approve pairing requests.
  pairing_delegate->SetConfirmPairingCallback(
      [](bt::PeerId, auto confirm_cb) { confirm_cb(true); });
  pairing_delegate->SetCompletePairingCallback(
      [&](bt::PeerId, bt::sm::Status status) { EXPECT_TRUE(status.is_success()); });

  l2cap()->ExpectOutboundL2capChannel(connection()->link().handle(), kPSM, 0x40, 0x41,
                                      bt::l2cap::ChannelParameters());

  fbl::RefPtr<bt::l2cap::testing::FakeChannel> fake_channel;
  l2cap()->set_channel_callback([&](auto chan) { fake_channel = std::move(chan); });

  // Expect a non-empty channel result.
  std::optional<fidlbredr::Channel> channel;
  auto chan_cb = [&channel](auto result) {
    ASSERT_TRUE(result.is_response());
    channel = std::move(result.response().channel);
  };

  fuchsia::bluetooth::PeerId peer_id{peer()->identifier().value()};
  fidlbredr::L2capParameters l2cap_params;
  l2cap_params.set_psm(kPSM);
  fidlbredr::ConnectParameters conn_params;
  conn_params.set_l2cap(std::move(l2cap_params));

  // Initiates pairing
  client()->Connect(peer_id, std::move(conn_params), std::move(chan_cb));
  RunLoopUntilIdle();
  ASSERT_TRUE(fake_channel);
  ASSERT_TRUE(channel.has_value());
  ASSERT_TRUE(channel->has_ext_direction());
  auto client = channel->mutable_ext_direction()->Bind();

  size_t priority_cb_count = 0;
  fake_channel->set_acl_priority_fails(!kExpectSuccess);
  client->SetPriority(kPriority, [&](auto result) {
    EXPECT_EQ(result.is_response(), kExpectSuccess);
    priority_cb_count++;
  });

  RunLoopUntilIdle();
  EXPECT_EQ(priority_cb_count, 1u);
  client = nullptr;
  RunLoopUntilIdle();

  if (kExpectSuccess) {
    switch (kPriority) {
      case fidlbredr::A2dpDirectionPriority::SOURCE:
        EXPECT_EQ(fake_channel->requested_acl_priority(), bt::l2cap::AclPriority::kSource);
        break;
      case fidlbredr::A2dpDirectionPriority::SINK:
        EXPECT_EQ(fake_channel->requested_acl_priority(), bt::l2cap::AclPriority::kSink);
        break;
      default:
        EXPECT_EQ(fake_channel->requested_acl_priority(), bt::l2cap::AclPriority::kNormal);
    }
  } else {
    EXPECT_EQ(fake_channel->requested_acl_priority(), bt::l2cap::AclPriority::kNormal);
  }
}

const std::array<std::pair<fidlbredr::A2dpDirectionPriority, bool>, 4> kPriorityParams = {
    {{fidlbredr::A2dpDirectionPriority::SOURCE, false},
     {fidlbredr::A2dpDirectionPriority::SOURCE, true},
     {fidlbredr::A2dpDirectionPriority::SINK, true},
     {fidlbredr::A2dpDirectionPriority::NORMAL, true}}};
INSTANTIATE_TEST_SUITE_P(FIDL_ProfileServerTest_ConnectedPeer, PriorityTest,
                         ::testing::ValuesIn(kPriorityParams));

TEST_F(AclPrioritySupportedTest, InboundConnectAndSetPriority) {
  fidlbredr::ChannelParameters fidl_chan_params;

  constexpr uint16_t kTxMtu = bt::l2cap::kMinACLMTU;

  auto pairing_delegate =
      std::make_unique<bt::gap::FakePairingDelegate>(bt::sm::IOCapability::kDisplayYesNo);
  adapter()->SetPairingDelegate(pairing_delegate->GetWeakPtr());

  fbl::RefPtr<bt::l2cap::testing::FakeChannel> fake_channel;
  l2cap()->set_channel_callback([&](auto chan) { fake_channel = std::move(chan); });

  using ::testing::StrictMock;
  fidl::InterfaceHandle<fidlbredr::ConnectionReceiver> connect_receiver_handle;
  auto connect_receiver = std::make_unique<StrictMock<MockConnectionReceiver>>(
      connect_receiver_handle.NewRequest(), dispatcher());

  std::vector<fidlbredr::ServiceDefinition> services;
  services.emplace_back(MakeFIDLServiceDefinition());

  client()->Advertise(std::move(services), std::move(fidl_chan_params),
                      std::move(connect_receiver_handle), NopAdvertiseCallback);
  RunLoopUntilIdle();

  std::optional<fidlbredr::Channel> channel;
  EXPECT_CALL(*connect_receiver, Connected(::testing::_, ::testing::_, ::testing::_))
      .WillOnce([&channel](fuchsia::bluetooth::PeerId peer_id, fidlbredr::Channel cb_channel,
                           ::testing::Unused) { channel = std::move(cb_channel); });

  EXPECT_TRUE(
      l2cap()->TriggerInboundL2capChannel(connection()->link().handle(), kPSM, 0x40, 0x41, kTxMtu));
  RunLoopUntilIdle();
  ASSERT_TRUE(channel.has_value());
  ASSERT_TRUE(channel->has_ext_direction());
  auto client = channel->mutable_ext_direction()->Bind();

  size_t priority_cb_count = 0;
  client->SetPriority(fidlbredr::A2dpDirectionPriority::SINK, [&](auto result) {
    EXPECT_TRUE(result.is_response());
    priority_cb_count++;
  });

  RunLoopUntilIdle();
  EXPECT_EQ(priority_cb_count, 1u);
  ASSERT_TRUE(fake_channel);
  EXPECT_EQ(fake_channel->requested_acl_priority(), bt::l2cap::AclPriority::kSink);
}

// Verifies that a socket channel relay is correctly set up such that bytes written to the socket
// are sent to the channel.
TEST_F(FIDL_ProfileServerTest_ConnectedPeer, ConnectReturnsValidSocket) {
  auto pairing_delegate =
      std::make_unique<bt::gap::FakePairingDelegate>(bt::sm::IOCapability::kDisplayYesNo);
  adapter()->SetPairingDelegate(pairing_delegate->GetWeakPtr());
  // Approve pairing requests.
  pairing_delegate->SetConfirmPairingCallback(
      [](bt::PeerId, auto confirm_cb) { confirm_cb(true); });
  pairing_delegate->SetCompletePairingCallback(
      [&](bt::PeerId, bt::sm::Status status) { EXPECT_TRUE(status.is_success()); });

  bt::l2cap::ChannelParameters expected_params;
  l2cap()->ExpectOutboundL2capChannel(connection()->link().handle(), kPSM, 0x40, 0x41,
                                      expected_params);

  fidlbredr::ChannelParameters fidl_params;

  std::optional<fbl::RefPtr<bt::l2cap::testing::FakeChannel>> fake_chan;
  l2cap()->set_channel_callback([&fake_chan](auto chan) { fake_chan = std::move(chan); });

  // Expect a non-empty channel result.
  std::optional<fidlbredr::Channel> channel;
  auto result_cb = [&channel](auto result) {
    EXPECT_TRUE(result.is_response());
    channel = std::move(result.response().channel);
  };

  fuchsia::bluetooth::PeerId peer_id{peer()->identifier().value()};

  fidlbredr::L2capParameters l2cap_params;
  l2cap_params.set_psm(kPSM);
  l2cap_params.set_parameters(std::move(fidl_params));

  fidlbredr::ConnectParameters connection;
  connection.set_l2cap(std::move(l2cap_params));

  // Initiates pairing
  client()->Connect(peer_id, std::move(connection), std::move(result_cb));
  RunLoopUntilIdle();

  ASSERT_TRUE(channel.has_value());
  ASSERT_TRUE(channel->has_socket());
  auto& socket = channel->socket();

  ASSERT_TRUE(fake_chan.has_value());
  auto fake_chan_ptr = fake_chan->get();
  size_t send_count = 0;
  fake_chan_ptr->SetSendCallback([&send_count](auto buffer) { send_count++; },
                                 async_get_default_dispatcher());

  const char write_data[2] = "a";
  size_t bytes_written = 0;
  auto status = socket.write(0, write_data, sizeof(write_data) - 1, &bytes_written);
  EXPECT_EQ(ZX_OK, status);
  EXPECT_EQ(1u, bytes_written);
  RunLoopUntilIdle();
  EXPECT_EQ(1u, send_count);
}

// Verifies that a socket channel relay is correctly set up such that bytes written to the socket
// are sent to the channel.
TEST_F(FIDL_ProfileServerTest_ConnectedPeer, ConnectionReceiverReturnsValidSocket) {
  auto pairing_delegate =
      std::make_unique<bt::gap::FakePairingDelegate>(bt::sm::IOCapability::kDisplayYesNo);
  adapter()->SetPairingDelegate(pairing_delegate->GetWeakPtr());

  using ::testing::StrictMock;
  fidl::InterfaceHandle<fidlbredr::ConnectionReceiver> connect_receiver_handle;
  auto connect_receiver = std::make_unique<StrictMock<MockConnectionReceiver>>(
      connect_receiver_handle.NewRequest(), dispatcher());

  std::optional<fbl::RefPtr<bt::l2cap::testing::FakeChannel>> fake_chan;
  l2cap()->set_channel_callback([&fake_chan](auto chan) { fake_chan = std::move(chan); });

  fidlbredr::ChannelParameters fidl_chan_params;

  std::vector<fidlbredr::ServiceDefinition> services;
  services.emplace_back(MakeFIDLServiceDefinition());

  std::optional<fidlbredr::Channel> channel;

  EXPECT_CALL(*connect_receiver, Connected(::testing::_, ::testing::_, ::testing::_))
      .WillOnce([expected_id = peer()->identifier(), &channel](fuchsia::bluetooth::PeerId peer_id,
                                                               fidlbredr::Channel cb_channel,
                                                               ::testing::Unused) {
        ASSERT_EQ(expected_id.value(), peer_id.value);
        channel = std::move(cb_channel);
      });

  client()->Advertise(std::move(services), std::move(fidl_chan_params),
                      std::move(connect_receiver_handle), NopAdvertiseCallback);
  RunLoopUntilIdle();

  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(connection()->link().handle(), kPSM, 0x40, 0x41));
  RunLoopUntilIdle();

  ASSERT_TRUE(channel.has_value());
  ASSERT_TRUE(channel->has_socket());
  auto& socket = channel->socket();

  ASSERT_TRUE(fake_chan.has_value());
  auto fake_chan_ptr = fake_chan->get();
  size_t send_count = 0;
  fake_chan_ptr->SetSendCallback([&send_count](auto buffer) { send_count++; },
                                 async_get_default_dispatcher());

  const char write_data[2] = "a";
  size_t bytes_written = 0;
  auto status = socket.write(0, write_data, sizeof(write_data) - 1, &bytes_written);
  EXPECT_EQ(ZX_OK, status);
  EXPECT_EQ(1u, bytes_written);
  RunLoopUntilIdle();
  EXPECT_EQ(1u, send_count);
}

fidlbredr::ScoConnectionParameters CreateScoConnectionParameters() {
  fidlbredr::ScoConnectionParameters params;
  params.set_parameter_set(fidlbredr::HfpParameterSet::MSBC_T2);
  params.set_air_coding_format(fidlbredr::CodingFormat::MSBC);
  params.set_air_frame_size(8u);
  params.set_io_bandwidth(32000);
  params.set_io_coding_format(fidlbredr::CodingFormat::LINEAR_PCM);
  params.set_io_frame_size(16u);
  params.set_io_pcm_data_format(fuchsia::hardware::audio::SampleFormat::PCM_SIGNED);
  params.set_io_pcm_sample_payload_msb_position(3u);
  params.set_path(fidlbredr::DataPath::OFFLOAD);
  return params;
}

class FakeScoConnectionReceiver : public fidlbredr::testing::ScoConnectionReceiver_TestBase {
 public:
  FakeScoConnectionReceiver(fidl::InterfaceRequest<ScoConnectionReceiver> request,
                            async_dispatcher_t* dispatcher)
      : binding_(this, std::move(request), dispatcher), connected_count_(0), error_count_(0) {}

  void Connected(::fuchsia::bluetooth::bredr::ScoConnection connection) override {
    connection_ = std::move(connection);
    connected_count_++;
  }
  void Error(::fuchsia::bluetooth::bredr::ScoErrorCode error) override {
    error_ = error;
    error_count_++;
  }

  void Close() { binding_.Close(ZX_ERR_PEER_CLOSED); }

  size_t connected_count() const { return connected_count_; }
  const std::optional<fidlbredr::ScoConnection>& connection() const { return connection_; }

  size_t error_count() const { return error_count_; }
  const std::optional<fidlbredr::ScoErrorCode>& error() const { return error_; }

 private:
  fidl::Binding<ScoConnectionReceiver> binding_;
  size_t connected_count_;
  std::optional<fidlbredr::ScoConnection> connection_;
  size_t error_count_;
  std::optional<fidlbredr::ScoErrorCode> error_;
  virtual void NotImplemented_(const std::string& name) { FAIL() << name << " is not implemented"; }
};

TEST_F(FIDL_ProfileServerTest, ConnectScoWithInvalidParameters) {
  fidlbredr::ScoConnectionParameters bad_sco_params;
  fidl::InterfaceHandle<fidlbredr::ScoConnectionReceiver> receiver_handle;
  FakeScoConnectionReceiver receiver(receiver_handle.NewRequest(), dispatcher());
  client()->ConnectSco(fuchsia::bluetooth::PeerId{1}, true, std::move(bad_sco_params),
                       std::move(receiver_handle));
  RunLoopUntilIdle();
  ASSERT_TRUE(receiver.error().has_value());
  EXPECT_EQ(receiver.error().value(), fidlbredr::ScoErrorCode::INVALID_ARGUMENTS);
  EXPECT_FALSE(receiver.connection().has_value());
}

TEST_F(FIDL_ProfileServerTest, ConnectScoWithUnconnectedPeerReturnsError) {
  fidlbredr::ScoConnectionParameters sco_params = CreateScoConnectionParameters();
  EXPECT_TRUE(fidl_helpers::FidlToScoParameters(sco_params).is_ok());

  fidl::InterfaceHandle<fidlbredr::ScoConnectionReceiver> receiver_handle;
  FakeScoConnectionReceiver receiver(receiver_handle.NewRequest(), dispatcher());
  client()->ConnectSco(fuchsia::bluetooth::PeerId{1}, true, std::move(sco_params),
                       std::move(receiver_handle));
  RunLoopUntilIdle();
  ASSERT_TRUE(receiver.error().has_value());
  EXPECT_EQ(receiver.error().value(), fidlbredr::ScoErrorCode::FAILURE);
  EXPECT_FALSE(receiver.connection().has_value());
}

TEST_F(FIDL_ProfileServerTest_ConnectedPeer, ConnectScoInitiatorSuccess) {
  fidlbredr::ScoConnectionParameters sco_params = CreateScoConnectionParameters();
  EXPECT_TRUE(fidl_helpers::FidlToScoParameters(sco_params).is_ok());

  fidl::InterfaceHandle<fidlbredr::ScoConnectionReceiver> receiver_handle;
  FakeScoConnectionReceiver receiver(receiver_handle.NewRequest(), dispatcher());
  client()->ConnectSco(fuchsia::bluetooth::PeerId{peer()->identifier().value()}, /*initiator=*/true,
                       std::move(sco_params), std::move(receiver_handle));
  RunLoopUntilIdle();
  EXPECT_FALSE(receiver.error().has_value());
  ASSERT_TRUE(receiver.connection().has_value());
  EXPECT_TRUE(receiver.connection()->has_socket());
}

TEST_F(FIDL_ProfileServerTest_ConnectedPeer, ConnectScoInitiatorAndCloseReceiver) {
  fidlbredr::ScoConnectionParameters sco_params = CreateScoConnectionParameters();
  EXPECT_TRUE(fidl_helpers::FidlToScoParameters(sco_params).is_ok());

  fidl::InterfaceHandle<fidlbredr::ScoConnectionReceiver> receiver_handle;
  FakeScoConnectionReceiver receiver(receiver_handle.NewRequest(), dispatcher());
  client()->ConnectSco(fuchsia::bluetooth::PeerId{peer()->identifier().value()}, /*initiator=*/true,
                       std::move(sco_params), std::move(receiver_handle));
  receiver.Close();
  RunLoopUntilIdle();
  EXPECT_FALSE(receiver.error().has_value());
  EXPECT_FALSE(receiver.connection().has_value());
}

// Verifies that the profile server gracefully ignores connection results after the receiver has
// closed.
TEST_F(FIDL_ProfileServerTest_ConnectedPeer,
       ConnectScoInitiatorAndCloseReceiverBeforeCompleteEvent) {
  fidlbredr::ScoConnectionParameters sco_params = CreateScoConnectionParameters();
  EXPECT_TRUE(fidl_helpers::FidlToScoParameters(sco_params).is_ok());

  fidl::InterfaceHandle<fidlbredr::ScoConnectionReceiver> receiver_handle;
  FakeScoConnectionReceiver receiver(receiver_handle.NewRequest(), dispatcher());

  test_device()->SetDefaultCommandStatus(bt::hci::kEnhancedSetupSynchronousConnection,
                                         bt::hci::kSuccess);
  client()->ConnectSco(fuchsia::bluetooth::PeerId{peer()->identifier().value()}, /*initiator=*/true,
                       std::move(sco_params), std::move(receiver_handle));
  receiver.Close();
  RunLoopUntilIdle();
  EXPECT_FALSE(receiver.error().has_value());
  EXPECT_FALSE(receiver.connection().has_value());
  test_device()->SendCommandChannelPacket(bt::testing::SynchronousConnectionCompletePacket(
      0x00, peer()->address(), bt::hci::LinkType::kSCO, bt::hci::StatusCode::kConnectionTimeout));
  RunLoopUntilIdle();
  EXPECT_FALSE(receiver.error().has_value());
  EXPECT_FALSE(receiver.connection().has_value());
}

}  // namespace
}  // namespace bthost
