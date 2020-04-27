// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "profile_server.h"

#include <fuchsia/bluetooth/bredr/cpp/fidl_test_base.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/data/fake_domain.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/adapter_test_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/fake_pairing_delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_peer.h"

namespace bthost {
namespace {

namespace fidlbredr = fuchsia::bluetooth::bredr;

namespace {

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

  bt::gap::BrEdrConnectionManager* conn_mgr() const {
    return adapter()->bredr_connection_manager();
  }

 private:
  std::unique_ptr<ProfileServer> server_;
  fidlbredr::ProfilePtr client_;

  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(FIDL_ProfileServerTest);
};

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
  // Empty service definition is not allowed - it must contain at least a serivce UUID.

  services.emplace_back(std::move(def));

  client()->Advertise(std::move(services), fidlbredr::SecurityRequirements(),
                      fidlbredr::ChannelParameters(), std::move(receiver_handle));

  RunLoopUntilIdle();

  // Server should close because it's not a good definition.
  zx_signals_t signals;
  request.channel().wait_one(ZX_CHANNEL_PEER_CLOSED, zx::time(0), &signals);
  EXPECT_TRUE(signals & ZX_CHANNEL_PEER_CLOSED);
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

    EXPECT_TRUE(conn_mgr()->Connect(peer_->identifier(), connect_cb));
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
  conn_mgr()->SetPairingDelegate(pairing_delegate->GetWeakPtr());
  // Approve pairing requests.
  pairing_delegate->SetConfirmPairingCallback(
      [](bt::PeerId, auto confirm_cb) { confirm_cb(true); });
  pairing_delegate->SetCompletePairingCallback(
      [&](bt::PeerId, bt::sm::Status status) { EXPECT_TRUE(status.is_success()); });

  bt::l2cap::ChannelParameters expected_params;
  expected_params.mode = bt::l2cap::ChannelMode::kEnhancedRetransmission;
  expected_params.max_rx_sdu_size = bt::l2cap::kMinACLMTU;
  data_domain()->ExpectOutboundL2capChannel(connection()->link().handle(), kPSM, 0x40, 0x41,
                                            expected_params);

  fidlbredr::ChannelParameters fidl_params;
  fidl_params.set_channel_mode(fidlbredr::ChannelMode::ENHANCED_RETRANSMISSION);
  fidl_params.set_max_rx_sdu_size(bt::l2cap::kMinACLMTU);

  // Expect a non-empty channel result.
  auto sock_cb = [](auto result) {
    EXPECT_TRUE(result.is_response());
    EXPECT_TRUE(!result.response().ResultValue_().IsEmpty());
  };
  // Initiates pairing

  fuchsia::bluetooth::PeerId peer_id{peer()->identifier().value()};

  client()->Connect(peer_id, kPSM, std::move(fidl_params), std::move(sock_cb));
  RunLoopUntilIdle();
}

// Tests receiving an empty Channel results in an error propagated through the callback.
TEST_F(FIDL_ProfileServerTest_ConnectedPeer, ConnectEmptyChannelResponse) {
  auto pairing_delegate =
      std::make_unique<bt::gap::FakePairingDelegate>(bt::sm::IOCapability::kDisplayYesNo);
  conn_mgr()->SetPairingDelegate(pairing_delegate->GetWeakPtr());
  // Approve pairing requests.
  pairing_delegate->SetConfirmPairingCallback(
      [](bt::PeerId, auto confirm_cb) { confirm_cb(true); });
  pairing_delegate->SetCompletePairingCallback(
      [&](bt::PeerId, bt::sm::Status status) { EXPECT_TRUE(status.is_success()); });

  // Make the l2cap channel creation fail.
  data_domain()->set_simulate_open_channel_failure(true);

  bt::l2cap::ChannelParameters expected_params;
  expected_params.mode = bt::l2cap::ChannelMode::kEnhancedRetransmission;
  expected_params.max_rx_sdu_size = bt::l2cap::kMinACLMTU;
  data_domain()->ExpectOutboundL2capChannel(connection()->link().handle(), kPSM, 0x40, 0x41,
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

  client()->Connect(peer_id, kPSM, std::move(fidl_params), std::move(sock_cb));
  RunLoopUntilIdle();
}

TEST_F(FIDL_ProfileServerTest_ConnectedPeer,
       AddServiceChannelParametersReceivedInOnChannelConnectedCallback) {
  fidlbredr::ChannelParameters fidl_chan_params;
  fidl_chan_params.set_channel_mode(fidlbredr::ChannelMode::ENHANCED_RETRANSMISSION);

  constexpr uint16_t kTxMtu = bt::l2cap::kMinACLMTU;

  auto pairing_delegate =
      std::make_unique<bt::gap::FakePairingDelegate>(bt::sm::IOCapability::kDisplayYesNo);
  conn_mgr()->SetPairingDelegate(pairing_delegate->GetWeakPtr());

  using ::testing::StrictMock;
  fidl::InterfaceHandle<fidlbredr::ConnectionReceiver> connect_receiver_handle;
  auto connect_receiver = std::make_unique<StrictMock<MockConnectionReceiver>>(
      connect_receiver_handle.NewRequest(), dispatcher());

  std::vector<fidlbredr::ServiceDefinition> services;
  services.emplace_back(MakeFIDLServiceDefinition());

  client()->Advertise(std::move(services), fidlbredr::SecurityRequirements(),
                      std::move(fidl_chan_params), std::move(connect_receiver_handle));
  RunLoopUntilIdle();

  EXPECT_CALL(*connect_receiver, Connected(::testing::_, ::testing::_, ::testing::_))
      .WillOnce([expected_id = peer()->identifier()](fuchsia::bluetooth::PeerId peer_id,
                                                     fidlbredr::Channel channel,
                                                     ::testing::Unused) {
        ASSERT_EQ(expected_id.value(), peer_id.value);
        ASSERT_TRUE(channel.has_socket());
      });

  EXPECT_TRUE(data_domain()->TriggerInboundL2capChannel(connection()->link().handle(), kPSM, 0x40,
                                                        0x41, kTxMtu));
  RunLoopUntilIdle();
}

}  // namespace
}  // namespace bthost
