// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "profile_server.h"

#include <gtest/gtest.h>

#include "src/connectivity/bluetooth/core/bt-host/data/fake_domain.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/adapter_test_fixture.h"
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
  def.service_class_uuids.emplace_back(bt::sdp::profile::kAudioSink.ToString());

  fidlbredr::ProtocolDescriptor l2cap_proto;
  l2cap_proto.protocol = fidlbredr::ProtocolIdentifier::L2CAP;
  fidlbredr::DataElement l2cap_data_el;
  l2cap_data_el.type = fidlbredr::DataElementType::UNSIGNED_INTEGER;
  l2cap_data_el.size = 2;
  l2cap_data_el.data.set_integer(fidlbredr::kPSM_AVDTP);
  l2cap_proto.params.emplace_back(std::move(l2cap_data_el));
  def.protocol_descriptors.emplace_back(std::move(l2cap_proto));

  fidlbredr::ProtocolDescriptor avdtp_proto;
  avdtp_proto.protocol = fidlbredr::ProtocolIdentifier::AVDTP;
  fidlbredr::DataElement avdtp_data_el;
  avdtp_data_el.type = fidlbredr::DataElementType::UNSIGNED_INTEGER;
  avdtp_data_el.size = 2;
  avdtp_data_el.data.set_integer(0x0103);  // Version 1.3
  avdtp_proto.params.emplace_back(std::move(avdtp_data_el));
  def.protocol_descriptors.emplace_back(std::move(avdtp_proto));

  fidlbredr::ProfileDescriptor prof_desc;
  prof_desc.profile_id = fidlbredr::ServiceClassProfileIdentifier::AdvancedAudioDistribution;
  prof_desc.major_version = 1;
  prof_desc.minor_version = 3;
  def.profile_descriptors.emplace_back(prof_desc);

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

TEST_F(FIDL_ProfileServerTest, ErrorOnInvalidUuid) {
  bool called = false;
  fuchsia::bluetooth::Status status;
  uint64_t service_id;
  auto cb = [&](auto s, uint64_t id) {
    called = true;
    status = std::move(s);
    service_id = id;
  };

  fidlbredr::ServiceDefinition def;
  def.service_class_uuids.emplace_back("bogus_uuid");

  client()->AddService(std::move(def), fidlbredr::SecurityLevel::ENCRYPTION_OPTIONAL,
                       fidlbredr::ChannelParameters(), std::move(cb));

  RunLoopUntilIdle();

  EXPECT_TRUE(called);
  EXPECT_TRUE(status.error);
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
  auto sock_cb = [](auto /*status*/, auto /*channel*/) {};
  // Initiates pairing
  client()->ConnectL2cap(peer()->identifier().ToString(), kPSM, std::move(fidl_params),
                         std::move(sock_cb));
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

  size_t sock_cb_count = 0;
  auto sock_cb = [&](std::string /*device_id*/, uint64_t /*service_id*/, fidlbredr::Channel channel,
                     fidlbredr::ProtocolDescriptor /*protocol*/) {
    sock_cb_count++;
    EXPECT_EQ(fidlbredr::ChannelMode::ENHANCED_RETRANSMISSION, channel.channel_mode());
    EXPECT_EQ(kTxMtu, channel.max_tx_sdu_size());
  };
  client().events().OnConnected = sock_cb;

  bool service_cb_called = false;
  auto cb = [&](fuchsia::bluetooth::Status status, uint64_t id) {
    service_cb_called = true;
    EXPECT_FALSE(status.error);
  };

  client()->AddService(MakeFIDLServiceDefinition(), fidlbredr::SecurityLevel::ENCRYPTION_OPTIONAL,
                       std::move(fidl_chan_params), std::move(cb));
  RunLoopUntilIdle();
  EXPECT_TRUE(service_cb_called);

  EXPECT_TRUE(data_domain()->TriggerInboundL2capChannel(connection()->link().handle(), kPSM, 0x40,
                                                        0x41, kTxMtu));

  RunLoopUntilIdle();
  EXPECT_EQ(1u, sock_cb_count);
}

}  // namespace
}  // namespace bthost
