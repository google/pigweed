// Copyright 2019 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "profile_server.h"

#include <fuchsia/bluetooth/bredr/cpp/fidl_test_base.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "fuchsia/bluetooth/bredr/cpp/fidl.h"
#include "lib/fidl/cpp/vector.h"
#include "lib/zx/socket.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/adapter_test_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/fidl/helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/fake_adapter_test_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/gap/fake_pairing_delegate.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/fake_l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/data_element.h"
#include "src/connectivity/bluetooth/core/bt-host/sdp/sdp.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_peer.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_packets.h"

namespace bthost {
namespace {

namespace fidlbredr = fuchsia::bluetooth::bredr;
namespace hci_android = bt::hci_spec::vendor::android;
using pw::bluetooth::AclPriority;
using FeaturesBits = pw::bluetooth::Controller::FeaturesBits;

namespace {

using FakeChannel = bt::l2cap::testing::FakeChannel;

void NopAdvertiseCallback(fidlbredr::Profile_Advertise_Result) {}

const bt::DeviceAddress kTestDevAddr(bt::DeviceAddress::Type::kBREDR, {1});
constexpr bt::l2cap::PSM kPSM = bt::l2cap::kAVDTP;

constexpr uint16_t kSynchronousDataPacketLength = 64;
constexpr uint8_t kTotalNumSynchronousDataPackets = 1;

fidlbredr::ScoConnectionParameters CreateScoConnectionParameters(
    fidlbredr::HfpParameterSet param_set = fidlbredr::HfpParameterSet::MSBC_T2) {
  fidlbredr::ScoConnectionParameters params;
  params.set_parameter_set(param_set);
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

  // Additional attributes are also OK.
  fidlbredr::Attribute addl_attr;
  addl_attr.id = 0x000A;  // Documentation URL ID
  fidlbredr::DataElement doc_url_el;
  doc_url_el.set_url("fuchsia.dev");
  addl_attr.element = std::move(doc_url_el);
  def.mutable_additional_attributes()->emplace_back(std::move(addl_attr));

  return def;
}

// Returns a basic protocol list element with a protocol descriptor list that only contains an L2CAP
// descriptor.
bt::sdp::DataElement MakeL2capProtocolListElement() {
  bt::sdp::DataElement l2cap_uuid_el;
  l2cap_uuid_el.Set(bt::UUID(bt::sdp::protocol::kL2CAP));
  std::vector<bt::sdp::DataElement> l2cap_descriptor_list;
  l2cap_descriptor_list.emplace_back(std::move(l2cap_uuid_el));
  std::vector<bt::sdp::DataElement> protocols;
  protocols.emplace_back(std::move(l2cap_descriptor_list));
  bt::sdp::DataElement protocol_list_el;
  protocol_list_el.Set(std::move(protocols));
  return protocol_list_el;
}

class FakeScoConnectionReceiver : public fidlbredr::testing::ScoConnectionReceiver_TestBase {
 public:
  FakeScoConnectionReceiver(fidl::InterfaceRequest<ScoConnectionReceiver> request,
                            async_dispatcher_t* dispatcher)
      : binding_(this, std::move(request), dispatcher), connected_count_(0), error_count_(0) {}

  void Connected(fidl::InterfaceHandle<::fuchsia::bluetooth::bredr::ScoConnection> connection,
                 fidlbredr::ScoConnectionParameters params) override {
    connection_.Bind(std::move(connection));
    parameters_ = std::move(params);
    connected_count_++;
  }
  void Error(fidlbredr::ScoErrorCode error) override {
    error_ = error;
    error_count_++;
  }

  void Close() { binding_.Close(ZX_ERR_PEER_CLOSED); }

  size_t connected_count() const { return connected_count_; }
  const fidl::InterfacePtr<fidlbredr::ScoConnection>& connection() const { return connection_; }
  fidl::InterfacePtr<fidlbredr::ScoConnection> take_connection() { return std::move(connection_); }
  const std::optional<fidlbredr::ScoConnectionParameters>& parameters() const {
    return parameters_;
  }

  size_t error_count() const { return error_count_; }
  const std::optional<fidlbredr::ScoErrorCode>& error() const { return error_; }

 private:
  fidl::Binding<ScoConnectionReceiver> binding_;
  size_t connected_count_;
  fidl::InterfacePtr<fidlbredr::ScoConnection> connection_;
  std::optional<fidlbredr::ScoConnectionParameters> parameters_;
  std::optional<uint16_t> max_tx_data_size_;
  size_t error_count_;
  std::optional<fidlbredr::ScoErrorCode> error_;
  void NotImplemented_(const std::string& name) override {
    FAIL() << name << " is not implemented";
  }
};

}  // namespace

using TestingBase = bthost::testing::AdapterTestFixture;
class ProfileServerTest : public TestingBase {
 public:
  ProfileServerTest() = default;
  ~ProfileServerTest() override = default;

 protected:
  void SetUp(FeaturesBits features) {
    bt::testing::FakeController::Settings settings;
    settings.ApplyDualModeDefaults();
    settings.synchronous_data_packet_length = kSynchronousDataPacketLength;
    settings.total_num_synchronous_data_packets = kTotalNumSynchronousDataPackets;
    TestingBase::SetUp(settings, features);

    fidl::InterfaceHandle<fidlbredr::Profile> profile_handle;
    client_.Bind(std::move(profile_handle));
    server_ =
        std::make_unique<ProfileServer>(adapter()->AsWeakPtr(), client_.NewRequest(dispatcher()));
  }
  void SetUp() override { SetUp(FeaturesBits{0}); }

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

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ProfileServerTest);
};

class FakeConnectionReceiver : public fidlbredr::testing::ConnectionReceiver_TestBase {
 public:
  FakeConnectionReceiver(fidl::InterfaceRequest<ConnectionReceiver> request,
                         async_dispatcher_t* dispatcher)
      : binding_(this, std::move(request), dispatcher), connected_count_(0) {}

  void Connected(fuchsia::bluetooth::PeerId peer_id, fidlbredr::Channel channel,
                 std::vector<fidlbredr::ProtocolDescriptor> protocol) override {
    peer_id_ = peer_id;
    channel_ = std::move(channel);
    protocol_ = std::move(protocol);
    connected_count_++;
  }

  size_t connected_count() const { return connected_count_; }
  const std::optional<fuchsia::bluetooth::PeerId>& peer_id() const { return peer_id_; }
  const std::optional<fidlbredr::Channel>& channel() const { return channel_; }
  const std::optional<std::vector<fidlbredr::ProtocolDescriptor>>& protocol() const {
    return protocol_;
  }

  std::optional<fidl::InterfacePtr<fidlbredr::AudioDirectionExt>> bind_ext_direction() {
    if (!channel().has_value()) {
      return std::nullopt;
    }
    auto client = channel_.value().mutable_ext_direction()->Bind();
    return client;
  }

  fidlbredr::Channel take_channel() {
    auto channel = std::move(channel_.value());
    channel_.reset();
    return channel;
  }

 private:
  fidl::Binding<ConnectionReceiver> binding_;
  size_t connected_count_;
  std::optional<fuchsia::bluetooth::PeerId> peer_id_;
  std::optional<fidlbredr::Channel> channel_;
  std::optional<std::vector<fidlbredr::ProtocolDescriptor>> protocol_;

  void NotImplemented_(const std::string& name) override {
    FAIL() << name << " is not implemented";
  }
};

class FakeSearchResults : public fidlbredr::testing::SearchResults_TestBase {
 public:
  FakeSearchResults(fidl::InterfaceRequest<SearchResults> request, async_dispatcher_t* dispatcher)
      : binding_(this, std::move(request), dispatcher), service_found_count_(0) {}

  void ServiceFound(fuchsia::bluetooth::PeerId peer_id,
                    fidl::VectorPtr<fidlbredr::ProtocolDescriptor> protocol,
                    std::vector<fidlbredr::Attribute> attributes,
                    ServiceFoundCallback callback) override {
    peer_id_ = peer_id;
    attributes_ = std::move(attributes);
    callback();
    service_found_count_++;
  }

  size_t service_found_count() const { return service_found_count_; }
  const std::optional<fuchsia::bluetooth::PeerId>& peer_id() const { return peer_id_; }
  const std::optional<std::vector<fidlbredr::Attribute>>& attributes() const { return attributes_; }

 private:
  fidl::Binding<SearchResults> binding_;
  std::optional<fuchsia::bluetooth::PeerId> peer_id_;
  std::optional<std::vector<fidlbredr::Attribute>> attributes_;
  size_t service_found_count_;

  void NotImplemented_(const std::string& name) override {
    FAIL() << name << " is not implemented";
  }
};

TEST_F(ProfileServerTest, ErrorOnInvalidDefinition) {
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

TEST_F(ProfileServerTest, ErrorOnMultipleAdvertiseRequests) {
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

TEST_F(ProfileServerTest, ErrorOnInvalidConnectParametersNoPSM) {
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

TEST_F(ProfileServerTest, ErrorOnInvalidConnectParametersRfcomm) {
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

TEST_F(ProfileServerTest, UnregisterAdvertisementTriggersCallback) {
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

class ProfileServerTestConnectedPeer : public ProfileServerTest {
 public:
  ProfileServerTestConnectedPeer() = default;
  ~ProfileServerTestConnectedPeer() override = default;

 protected:
  void SetUp(FeaturesBits features) {
    ProfileServerTest::SetUp(features);
    peer_ = peer_cache()->NewPeer(kTestDevAddr, /*connectable=*/true);
    auto fake_peer = std::make_unique<bt::testing::FakePeer>(kTestDevAddr);
    test_device()->AddPeer(std::move(fake_peer));

    bt::hci::Result<> status = ToResult(bt::HostError::kFailed);
    auto connect_cb = [this, &status](auto cb_status, auto cb_conn_ref) {
      ASSERT_TRUE(cb_conn_ref);
      status = cb_status;
      connection_ = std::move(cb_conn_ref);
    };

    EXPECT_TRUE(adapter()->bredr()->Connect(peer_->identifier(), connect_cb));
    EXPECT_EQ(bt::gap::Peer::ConnectionState::kInitializing, peer_->bredr()->connection_state());

    RunLoopUntilIdle();
    EXPECT_EQ(fit::ok(), status);
    ASSERT_TRUE(connection_);
    EXPECT_EQ(peer_->identifier(), connection_->peer_id());
    EXPECT_NE(bt::gap::Peer::ConnectionState::kNotConnected, peer_->bredr()->connection_state());
  }
  void SetUp() override { SetUp(FeaturesBits::kHciSco); }

  void TearDown() override {
    connection_ = nullptr;
    peer_ = nullptr;
    ProfileServerTest::TearDown();
  }

  bt::gap::BrEdrConnection* connection() const { return connection_; }

  bt::gap::Peer* peer() const { return peer_; }

 private:
  bt::gap::BrEdrConnection* connection_;
  bt::gap::Peer* peer_;

  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ProfileServerTestConnectedPeer);
};

class ProfileServerTestScoConnected : public ProfileServerTestConnectedPeer {
 public:
  void SetUp() override {
    fidlbredr::ScoConnectionParameters params =
        CreateScoConnectionParameters(fidlbredr::HfpParameterSet::CVSD_D0);
    params.set_path(fidlbredr::DataPath::HOST);
    SetUp(std::move(params));
  }

  void SetUp(fidlbredr::ScoConnectionParameters conn_params) {
    ProfileServerTestConnectedPeer::SetUp(FeaturesBits::kHciSco);

    test_device()->set_configure_sco_cb([](auto, auto, auto, auto cb) { cb(PW_STATUS_OK); });
    test_device()->set_reset_sco_cb([](auto cb) { cb(PW_STATUS_OK); });

    std::vector<fidlbredr::ScoConnectionParameters> sco_params_list;
    sco_params_list.emplace_back(std::move(conn_params));

    fidl::InterfaceHandle<fidlbredr::ScoConnectionReceiver> receiver_handle;
    FakeScoConnectionReceiver receiver(receiver_handle.NewRequest(), dispatcher());
    client()->ConnectSco(fuchsia::bluetooth::PeerId{peer()->identifier().value()},
                         /*initiator=*/false, std::move(sco_params_list),
                         std::move(receiver_handle));
    RunLoopUntilIdle();
    test_device()->SendConnectionRequest(peer()->address(), bt::hci_spec::LinkType::kSCO);
    RunLoopUntilIdle();
    ASSERT_TRUE(receiver.connection().is_bound());
    sco_connection_ = receiver.take_connection();
    sco_connection_.set_error_handler([this](zx_status_t status) {
      sco_connection_ = nullptr;
      sco_conn_error_ = status;
    });

    // Find the link handle used for the SCO connection.
    bt::testing::FakePeer* fake_peer = test_device()->FindPeer(peer()->address());
    ASSERT_TRUE(fake_peer);
    // There are 2 connections: BR/EDR, SCO
    ASSERT_EQ(fake_peer->logical_links().size(), 2u);
    bt::testing::FakePeer::HandleSet links = fake_peer->logical_links();
    // The link that is not the BR/EDR connection link must be the SCO link.
    links.erase(connection()->link().handle());
    sco_conn_handle_ = *links.begin();
  }

  void TearDown() override { ProfileServerTestConnectedPeer::TearDown(); }

  fidl::InterfacePtr<fidlbredr::ScoConnection>& sco_connection() { return sco_connection_; }

  std::optional<zx_status_t> sco_conn_error() const { return sco_conn_error_; }

  bt::hci_spec::ConnectionHandle sco_handle() const { return sco_conn_handle_; }

 private:
  fidl::InterfacePtr<fidlbredr::ScoConnection> sco_connection_;
  bt::hci_spec::ConnectionHandle sco_conn_handle_;
  std::optional<zx_status_t> sco_conn_error_;
};

class ProfileServerTestOffloadedScoConnected : public ProfileServerTestScoConnected {
 public:
  void SetUp() override {
    fidlbredr::ScoConnectionParameters params =
        CreateScoConnectionParameters(fidlbredr::HfpParameterSet::CVSD_D0);
    params.set_path(fidlbredr::DataPath::OFFLOAD);
    ProfileServerTestScoConnected::SetUp(std::move(params));
  }
};

TEST_F(ProfileServerTestConnectedPeer, ConnectL2capChannelParameters) {
  auto pairing_delegate =
      std::make_unique<bt::gap::FakePairingDelegate>(bt::sm::IOCapability::kDisplayYesNo);
  adapter()->SetPairingDelegate(pairing_delegate->GetWeakPtr());
  // Approve pairing requests.
  pairing_delegate->SetConfirmPairingCallback(
      [](bt::PeerId, auto confirm_cb) { confirm_cb(true); });
  pairing_delegate->SetCompletePairingCallback(
      [&](bt::PeerId, bt::sm::Result<> status) { EXPECT_EQ(fit::ok(), status); });

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
  EXPECT_FALSE(channel->has_flush_timeout());
}

TEST_F(ProfileServerTestConnectedPeer,
       ConnectWithAuthenticationRequiredButLinkKeyNotAuthenticatedFails) {
  auto pairing_delegate =
      std::make_unique<bt::gap::FakePairingDelegate>(bt::sm::IOCapability::kNoInputNoOutput);
  adapter()->SetPairingDelegate(pairing_delegate->GetWeakPtr());
  pairing_delegate->SetCompletePairingCallback(
      [&](bt::PeerId, bt::sm::Result<> status) { EXPECT_EQ(fit::ok(), status); });

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
TEST_F(ProfileServerTestConnectedPeer, ConnectEmptyChannelResponse) {
  auto pairing_delegate =
      std::make_unique<bt::gap::FakePairingDelegate>(bt::sm::IOCapability::kDisplayYesNo);
  adapter()->SetPairingDelegate(pairing_delegate->GetWeakPtr());
  // Approve pairing requests.
  pairing_delegate->SetConfirmPairingCallback(
      [](bt::PeerId, auto confirm_cb) { confirm_cb(true); });
  pairing_delegate->SetCompletePairingCallback(
      [&](bt::PeerId, bt::sm::Result<> status) { EXPECT_EQ(fit::ok(), status); });

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

TEST_F(ProfileServerTestConnectedPeer,
       AdvertiseChannelParametersReceivedInOnChannelConnectedCallback) {
  fidlbredr::ChannelParameters fidl_chan_params;
  fidl_chan_params.set_channel_mode(fidlbredr::ChannelMode::ENHANCED_RETRANSMISSION);

  constexpr uint16_t kTxMtu = bt::l2cap::kMinACLMTU;

  auto pairing_delegate =
      std::make_unique<bt::gap::FakePairingDelegate>(bt::sm::IOCapability::kDisplayYesNo);
  adapter()->SetPairingDelegate(pairing_delegate->GetWeakPtr());

  fidl::InterfaceHandle<fidlbredr::ConnectionReceiver> connect_receiver_handle;
  FakeConnectionReceiver connect_receiver(connect_receiver_handle.NewRequest(), dispatcher());

  std::vector<fidlbredr::ServiceDefinition> services;
  services.emplace_back(MakeFIDLServiceDefinition());

  client()->Advertise(std::move(services), std::move(fidl_chan_params),
                      std::move(connect_receiver_handle), NopAdvertiseCallback);
  RunLoopUntilIdle();

  ASSERT_EQ(connect_receiver.connected_count(), 0u);
  EXPECT_TRUE(
      l2cap()->TriggerInboundL2capChannel(connection()->link().handle(), kPSM, 0x40, 0x41, kTxMtu));
  RunLoopUntilIdle();

  ASSERT_EQ(connect_receiver.connected_count(), 1u);
  ASSERT_EQ(connect_receiver.peer_id().value().value, peer()->identifier().value());
  ASSERT_TRUE(connect_receiver.channel().value().has_socket());
  EXPECT_EQ(connect_receiver.channel().value().channel_mode(),
            fidlbredr::ChannelMode::ENHANCED_RETRANSMISSION);
  EXPECT_EQ(connect_receiver.channel().value().max_tx_sdu_size(), kTxMtu);
  EXPECT_FALSE(connect_receiver.channel().value().has_ext_direction());
  EXPECT_FALSE(connect_receiver.channel().value().has_flush_timeout());
}

class AclPrioritySupportedTest : public ProfileServerTestConnectedPeer {
 public:
  void SetUp() override {
    ProfileServerTestConnectedPeer::SetUp(FeaturesBits::kSetAclPriorityCommand);
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
      [&](bt::PeerId, bt::sm::Result<> status) { EXPECT_EQ(fit::ok(), status); });

  l2cap()->ExpectOutboundL2capChannel(connection()->link().handle(), kPSM, 0x40, 0x41,
                                      bt::l2cap::ChannelParameters());

  bt::l2cap::testing::FakeChannel::WeakPtr fake_channel;
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
  ASSERT_TRUE(fake_channel.is_alive());
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
        EXPECT_EQ(fake_channel->requested_acl_priority(), AclPriority::kSource);
        break;
      case fidlbredr::A2dpDirectionPriority::SINK:
        EXPECT_EQ(fake_channel->requested_acl_priority(), AclPriority::kSink);
        break;
      default:
        EXPECT_EQ(fake_channel->requested_acl_priority(), AclPriority::kNormal);
    }
  } else {
    EXPECT_EQ(fake_channel->requested_acl_priority(), AclPriority::kNormal);
  }
}

const std::array<std::pair<fidlbredr::A2dpDirectionPriority, bool>, 4> kPriorityParams = {
    {{fidlbredr::A2dpDirectionPriority::SOURCE, false},
     {fidlbredr::A2dpDirectionPriority::SOURCE, true},
     {fidlbredr::A2dpDirectionPriority::SINK, true},
     {fidlbredr::A2dpDirectionPriority::NORMAL, true}}};
INSTANTIATE_TEST_SUITE_P(ProfileServerTestConnectedPeer, PriorityTest,
                         ::testing::ValuesIn(kPriorityParams));

TEST_F(AclPrioritySupportedTest, InboundConnectAndSetPriority) {
  fidlbredr::ChannelParameters fidl_chan_params;

  constexpr uint16_t kTxMtu = bt::l2cap::kMinACLMTU;

  auto pairing_delegate =
      std::make_unique<bt::gap::FakePairingDelegate>(bt::sm::IOCapability::kDisplayYesNo);
  adapter()->SetPairingDelegate(pairing_delegate->GetWeakPtr());

  bt::l2cap::testing::FakeChannel::WeakPtr fake_channel;
  l2cap()->set_channel_callback([&](auto chan) { fake_channel = std::move(chan); });

  fidl::InterfaceHandle<fidlbredr::ConnectionReceiver> connect_receiver_handle;
  FakeConnectionReceiver connect_receiver(connect_receiver_handle.NewRequest(), dispatcher());

  std::vector<fidlbredr::ServiceDefinition> services;
  services.emplace_back(MakeFIDLServiceDefinition());

  client()->Advertise(std::move(services), std::move(fidl_chan_params),
                      std::move(connect_receiver_handle), NopAdvertiseCallback);
  RunLoopUntilIdle();

  ASSERT_EQ(connect_receiver.connected_count(), 0u);
  EXPECT_TRUE(
      l2cap()->TriggerInboundL2capChannel(connection()->link().handle(), kPSM, 0x40, 0x41, kTxMtu));

  RunLoopUntilIdle();
  ASSERT_EQ(connect_receiver.connected_count(), 1u);
  ASSERT_TRUE(connect_receiver.channel().has_value());
  ASSERT_TRUE(connect_receiver.channel().value().has_ext_direction());
  // Taking value() is safe because of the has_ext_direction() check.
  auto client = connect_receiver.bind_ext_direction().value();

  size_t priority_cb_count = 0;
  client->SetPriority(fidlbredr::A2dpDirectionPriority::SINK, [&](auto result) {
    EXPECT_TRUE(result.is_response());
    priority_cb_count++;
  });

  RunLoopUntilIdle();
  EXPECT_EQ(priority_cb_count, 1u);
  ASSERT_TRUE(fake_channel.is_alive());
  EXPECT_EQ(fake_channel->requested_acl_priority(), AclPriority::kSink);
}

// Verifies that a socket channel relay is correctly set up such that bytes written to the socket
// are sent to the channel.
TEST_F(ProfileServerTestConnectedPeer, ConnectReturnsValidSocket) {
  auto pairing_delegate =
      std::make_unique<bt::gap::FakePairingDelegate>(bt::sm::IOCapability::kDisplayYesNo);
  adapter()->SetPairingDelegate(pairing_delegate->GetWeakPtr());
  // Approve pairing requests.
  pairing_delegate->SetConfirmPairingCallback(
      [](bt::PeerId, auto confirm_cb) { confirm_cb(true); });
  pairing_delegate->SetCompletePairingCallback(
      [&](bt::PeerId, bt::sm::Result<> status) { EXPECT_EQ(fit::ok(), status); });

  bt::l2cap::ChannelParameters expected_params;
  l2cap()->ExpectOutboundL2capChannel(connection()->link().handle(), kPSM, 0x40, 0x41,
                                      expected_params);

  fidlbredr::ChannelParameters fidl_params;

  std::optional<bt::l2cap::testing::FakeChannel::WeakPtr> fake_chan;
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
  auto fake_chan_ptr = fake_chan.value();
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
TEST_F(ProfileServerTestConnectedPeer, ConnectionReceiverReturnsValidSocket) {
  auto pairing_delegate =
      std::make_unique<bt::gap::FakePairingDelegate>(bt::sm::IOCapability::kDisplayYesNo);
  adapter()->SetPairingDelegate(pairing_delegate->GetWeakPtr());

  fidl::InterfaceHandle<fidlbredr::ConnectionReceiver> connect_receiver_handle;
  FakeConnectionReceiver connect_receiver(connect_receiver_handle.NewRequest(), dispatcher());

  std::optional<bt::l2cap::testing::FakeChannel::WeakPtr> fake_chan;
  l2cap()->set_channel_callback([&fake_chan](auto chan) { fake_chan = std::move(chan); });

  fidlbredr::ChannelParameters fidl_chan_params;

  std::vector<fidlbredr::ServiceDefinition> services;
  services.emplace_back(MakeFIDLServiceDefinition());

  client()->Advertise(std::move(services), std::move(fidl_chan_params),
                      std::move(connect_receiver_handle), NopAdvertiseCallback);
  RunLoopUntilIdle();

  ASSERT_EQ(connect_receiver.connected_count(), 0u);
  EXPECT_TRUE(l2cap()->TriggerInboundL2capChannel(connection()->link().handle(), kPSM, 0x40, 0x41));
  RunLoopUntilIdle();

  ASSERT_EQ(connect_receiver.connected_count(), 1u);
  ASSERT_EQ(connect_receiver.peer_id().value().value, peer()->identifier().value());
  ASSERT_TRUE(connect_receiver.channel().has_value());
  ASSERT_TRUE(connect_receiver.channel().value().has_socket());
  // Taking channel is safe because of the previous checks.
  auto channel = connect_receiver.take_channel();

  ASSERT_TRUE(fake_chan.has_value());
  auto fake_chan_ptr = fake_chan.value();
  size_t send_count = 0;
  fake_chan_ptr->SetSendCallback([&send_count](auto buffer) { send_count++; },
                                 async_get_default_dispatcher());

  const char write_data[2] = "a";
  size_t bytes_written = 0;
  auto status = channel.socket().write(0, write_data, sizeof(write_data) - 1, &bytes_written);
  EXPECT_EQ(ZX_OK, status);
  EXPECT_EQ(1u, bytes_written);
  RunLoopUntilIdle();
  EXPECT_EQ(1u, send_count);
}

TEST_F(ProfileServerTest, ConnectScoWithInvalidParameters) {
  std::vector<fidlbredr::ScoConnectionParameters> bad_sco_params;
  bad_sco_params.emplace_back(fidlbredr::ScoConnectionParameters());
  fidl::InterfaceHandle<fidlbredr::ScoConnectionReceiver> receiver_handle;
  FakeScoConnectionReceiver receiver(receiver_handle.NewRequest(), dispatcher());
  client()->ConnectSco(fuchsia::bluetooth::PeerId{1}, /*initiator=*/true, std::move(bad_sco_params),
                       std::move(receiver_handle));
  RunLoopUntilIdle();
  ASSERT_TRUE(receiver.error().has_value());
  EXPECT_EQ(receiver.error().value(), fidlbredr::ScoErrorCode::INVALID_ARGUMENTS);
  EXPECT_FALSE(receiver.connection().is_bound());
}

TEST_F(ProfileServerTest, ConnectScoWithEmptyParameters) {
  fidl::InterfaceHandle<fidlbredr::ScoConnectionReceiver> receiver_handle;
  FakeScoConnectionReceiver receiver(receiver_handle.NewRequest(), dispatcher());
  client()->ConnectSco(fuchsia::bluetooth::PeerId{1}, /*initiator=*/true, /*params=*/{},
                       std::move(receiver_handle));
  RunLoopUntilIdle();
  ASSERT_TRUE(receiver.error().has_value());
  EXPECT_EQ(receiver.error().value(), fidlbredr::ScoErrorCode::INVALID_ARGUMENTS);
  EXPECT_FALSE(receiver.connection().is_bound());
}

TEST_F(ProfileServerTest, ConnectScoInitiatorWithTooManyParameters) {
  std::vector<fidlbredr::ScoConnectionParameters> sco_params_list;
  sco_params_list.emplace_back(CreateScoConnectionParameters());
  sco_params_list.emplace_back(CreateScoConnectionParameters());

  fidl::InterfaceHandle<fidlbredr::ScoConnectionReceiver> receiver_handle;
  FakeScoConnectionReceiver receiver(receiver_handle.NewRequest(), dispatcher());
  client()->ConnectSco(fuchsia::bluetooth::PeerId{1}, /*initiator=*/true,
                       /*params=*/std::move(sco_params_list), std::move(receiver_handle));
  RunLoopUntilIdle();
  ASSERT_TRUE(receiver.error().has_value());
  EXPECT_EQ(receiver.error().value(), fidlbredr::ScoErrorCode::INVALID_ARGUMENTS);
  EXPECT_FALSE(receiver.connection().is_bound());
}

TEST_F(ProfileServerTest, ConnectScoWithUnconnectedPeerReturnsError) {
  fidlbredr::ScoConnectionParameters sco_params = CreateScoConnectionParameters();
  EXPECT_TRUE(fidl_helpers::FidlToScoParameters(sco_params).is_ok());
  std::vector<fidlbredr::ScoConnectionParameters> sco_params_list;
  sco_params_list.emplace_back(std::move(sco_params));

  fidl::InterfaceHandle<fidlbredr::ScoConnectionReceiver> receiver_handle;
  FakeScoConnectionReceiver receiver(receiver_handle.NewRequest(), dispatcher());
  client()->ConnectSco(fuchsia::bluetooth::PeerId{1}, /*initiator=*/true,
                       std::move(sco_params_list), std::move(receiver_handle));
  RunLoopUntilIdle();
  ASSERT_TRUE(receiver.error().has_value());
  EXPECT_EQ(receiver.error().value(), fidlbredr::ScoErrorCode::FAILURE);
  EXPECT_FALSE(receiver.connection().is_bound());
}

TEST_F(ProfileServerTestConnectedPeer, ConnectScoInitiatorSuccess) {
  fidlbredr::ScoConnectionParameters sco_params =
      CreateScoConnectionParameters(fidlbredr::HfpParameterSet::MSBC_T1);
  EXPECT_TRUE(fidl_helpers::FidlToScoParameters(sco_params).is_ok());
  std::vector<fidlbredr::ScoConnectionParameters> sco_params_list;
  sco_params_list.emplace_back(std::move(sco_params));

  fidl::InterfaceHandle<fidlbredr::ScoConnectionReceiver> receiver_handle;
  FakeScoConnectionReceiver receiver(receiver_handle.NewRequest(), dispatcher());
  client()->ConnectSco(fuchsia::bluetooth::PeerId{peer()->identifier().value()}, /*initiator=*/true,
                       std::move(sco_params_list), std::move(receiver_handle));
  RunLoopUntilIdle();
  EXPECT_FALSE(receiver.error().has_value());
  ASSERT_TRUE(receiver.connection().is_bound());
  ASSERT_TRUE(receiver.parameters().has_value());
  ASSERT_TRUE(receiver.parameters()->has_parameter_set());
  EXPECT_EQ(receiver.parameters()->parameter_set(), fidlbredr::HfpParameterSet::MSBC_T1);
  ASSERT_TRUE(receiver.parameters()->has_max_tx_data_size());
  EXPECT_EQ(receiver.parameters()->max_tx_data_size(), kSynchronousDataPacketLength);
}

TEST_F(ProfileServerTestConnectedPeer, ConnectScoResponderSuccess) {
  // Use 2 parameter sets to test that the profile server returns the second set when a SCO
  // connection request is received (MSBC_T2 is ESCO only and CVSD_D0 is SCO only, so CVSD_D0 will
  // be used to accept the connection).
  std::vector<fidlbredr::ScoConnectionParameters> sco_params_list;
  sco_params_list.emplace_back(CreateScoConnectionParameters(fidlbredr::HfpParameterSet::MSBC_T2));
  sco_params_list.emplace_back(CreateScoConnectionParameters(fidlbredr::HfpParameterSet::CVSD_D0));

  fidl::InterfaceHandle<fidlbredr::ScoConnectionReceiver> receiver_handle;
  FakeScoConnectionReceiver receiver(receiver_handle.NewRequest(), dispatcher());
  client()->ConnectSco(fuchsia::bluetooth::PeerId{peer()->identifier().value()},
                       /*initiator=*/false, std::move(sco_params_list), std::move(receiver_handle));
  RunLoopUntilIdle();
  // Receive a SCO connection request. The CVSD_D0 parameters will be used to accept the request.
  test_device()->SendConnectionRequest(peer()->address(), bt::hci_spec::LinkType::kSCO);
  RunLoopUntilIdle();
  EXPECT_FALSE(receiver.error().has_value());
  ASSERT_TRUE(receiver.connection().is_bound());
  ASSERT_TRUE(receiver.parameters().has_value());
  ASSERT_TRUE(receiver.parameters()->has_parameter_set());
  EXPECT_EQ(receiver.parameters()->parameter_set(), fidlbredr::HfpParameterSet::CVSD_D0);
}

TEST_F(ProfileServerTestConnectedPeer, ConnectScoResponderUnconnectedPeerReturnsError) {
  std::vector<fidlbredr::ScoConnectionParameters> sco_params_list;
  sco_params_list.emplace_back(CreateScoConnectionParameters());

  fidl::InterfaceHandle<fidlbredr::ScoConnectionReceiver> receiver_handle;
  FakeScoConnectionReceiver receiver(receiver_handle.NewRequest(), dispatcher());
  client()->ConnectSco(fuchsia::bluetooth::PeerId{1}, /*initiator=*/false,
                       std::move(sco_params_list), std::move(receiver_handle));
  RunLoopUntilIdle();
  ASSERT_TRUE(receiver.error().has_value());
  EXPECT_EQ(receiver.error().value(), fidlbredr::ScoErrorCode::FAILURE);
  EXPECT_FALSE(receiver.connection().is_bound());
}

TEST_F(ProfileServerTestConnectedPeer, ConnectScoInitiatorAndCloseReceiver) {
  fidlbredr::ScoConnectionParameters sco_params = CreateScoConnectionParameters();
  EXPECT_TRUE(fidl_helpers::FidlToScoParameters(sco_params).is_ok());
  std::vector<fidlbredr::ScoConnectionParameters> sco_params_list;
  sco_params_list.emplace_back(std::move(sco_params));

  fidl::InterfaceHandle<fidlbredr::ScoConnectionReceiver> receiver_handle;
  FakeScoConnectionReceiver receiver(receiver_handle.NewRequest(), dispatcher());
  client()->ConnectSco(fuchsia::bluetooth::PeerId{peer()->identifier().value()}, /*initiator=*/true,
                       std::move(sco_params_list), std::move(receiver_handle));
  receiver.Close();
  RunLoopUntilIdle();
  EXPECT_FALSE(receiver.error().has_value());
  EXPECT_FALSE(receiver.connection().is_bound());
}

// Verifies that the profile server gracefully ignores connection results after the receiver has
// closed.
TEST_F(ProfileServerTestConnectedPeer, ConnectScoInitiatorAndCloseReceiverBeforeCompleteEvent) {
  fidlbredr::ScoConnectionParameters sco_params = CreateScoConnectionParameters();
  EXPECT_TRUE(fidl_helpers::FidlToScoParameters(sco_params).is_ok());
  std::vector<fidlbredr::ScoConnectionParameters> sco_params_list;
  sco_params_list.emplace_back(std::move(sco_params));

  fidl::InterfaceHandle<fidlbredr::ScoConnectionReceiver> receiver_handle;
  FakeScoConnectionReceiver receiver(receiver_handle.NewRequest(), dispatcher());

  test_device()->SetDefaultCommandStatus(bt::hci_spec::kEnhancedSetupSynchronousConnection,
                                         pw::bluetooth::emboss::StatusCode::SUCCESS);
  client()->ConnectSco(fuchsia::bluetooth::PeerId{peer()->identifier().value()}, /*initiator=*/true,
                       std::move(sco_params_list), std::move(receiver_handle));
  receiver.Close();
  RunLoopUntilIdle();
  EXPECT_FALSE(receiver.error().has_value());
  EXPECT_FALSE(receiver.connection().is_bound());
  test_device()->SendCommandChannelPacket(bt::testing::SynchronousConnectionCompletePacket(
      0x00, peer()->address(), bt::hci_spec::LinkType::kSCO,
      pw::bluetooth::emboss::StatusCode::CONNECTION_TIMEOUT));
  RunLoopUntilIdle();
  EXPECT_FALSE(receiver.error().has_value());
  EXPECT_FALSE(receiver.connection().is_bound());
}

class ProfileServerTestFakeAdapter : public bt::gap::testing::FakeAdapterTestFixture {
 public:
  ProfileServerTestFakeAdapter() = default;
  ~ProfileServerTestFakeAdapter() override = default;

  void SetUp() override {
    FakeAdapterTestFixture::SetUp();

    fidl::InterfaceHandle<fidlbredr::Profile> profile_handle;
    client_.Bind(std::move(profile_handle));
    server_ =
        std::make_unique<ProfileServer>(adapter()->AsWeakPtr(), client_.NewRequest(dispatcher()));
  }

  void TearDown() override { FakeAdapterTestFixture::TearDown(); }

  fidlbredr::ProfilePtr& client() { return client_; }

 private:
  std::unique_ptr<ProfileServer> server_;
  fidlbredr::ProfilePtr client_;
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ProfileServerTestFakeAdapter);
};

TEST_F(ProfileServerTestFakeAdapter, ConnectChannelParametersContainsFlushTimeout) {
  const bt::PeerId kPeerId;
  const fuchsia::bluetooth::PeerId kFidlPeerId{kPeerId.value()};
  const zx::duration kFlushTimeout(zx::msec(100));

  FakeChannel::WeakPtr last_channel;
  adapter()->fake_bredr()->set_l2cap_channel_callback(
      [&](auto chan) { last_channel = std::move(chan); });

  fidlbredr::ChannelParameters chan_params;
  chan_params.set_flush_timeout(kFlushTimeout.get());
  fidlbredr::L2capParameters l2cap_params;
  l2cap_params.set_psm(fidlbredr::PSM_AVDTP);
  l2cap_params.set_parameters(std::move(chan_params));
  fidlbredr::ConnectParameters conn_params;
  conn_params.set_l2cap(std::move(l2cap_params));

  std::optional<fidlbredr::Channel> response_channel;
  client()->Connect(kFidlPeerId, std::move(conn_params),
                    [&](fidlbredr::Profile_Connect_Result result) {
                      ASSERT_TRUE(result.is_response());
                      response_channel = std::move(result.response().channel);
                    });
  RunLoopUntilIdle();
  ASSERT_TRUE(last_channel.is_alive());
  EXPECT_EQ(last_channel->info().flush_timeout, std::optional(kFlushTimeout));
  ASSERT_TRUE(response_channel.has_value());
  ASSERT_TRUE(response_channel->has_flush_timeout());
  ASSERT_EQ(response_channel->flush_timeout(), kFlushTimeout.get());
}

TEST_F(ProfileServerTestFakeAdapter, AdvertiseChannelParametersContainsFlushTimeout) {
  const zx::duration kFlushTimeout(zx::msec(100));
  const bt::hci_spec::ConnectionHandle kHandle(1);

  std::vector<fidlbredr::ServiceDefinition> services;
  services.emplace_back(MakeFIDLServiceDefinition());
  fidlbredr::ChannelParameters chan_params;
  chan_params.set_flush_timeout(kFlushTimeout.get());

  fidl::InterfaceHandle<fidlbredr::ConnectionReceiver> connect_receiver_handle;
  FakeConnectionReceiver connect_receiver(connect_receiver_handle.NewRequest(), dispatcher());

  client()->Advertise(std::move(services), std::move(chan_params),
                      std::move(connect_receiver_handle), NopAdvertiseCallback);
  RunLoopUntilIdle();

  ASSERT_EQ(adapter()->fake_bredr()->registered_services().size(), 1u);
  auto service_iter = adapter()->fake_bredr()->registered_services().begin();
  EXPECT_EQ(service_iter->second.channel_params.flush_timeout, std::optional(kFlushTimeout));

  bt::l2cap::ChannelInfo chan_info = bt::l2cap::ChannelInfo::MakeBasicMode(
      bt::l2cap::kDefaultMTU, bt::l2cap::kDefaultMTU, bt::l2cap::kAVDTP, kFlushTimeout);
  auto channel = std::make_unique<FakeChannel>(bt::l2cap::kFirstDynamicChannelId,
                                               bt::l2cap::kFirstDynamicChannelId, kHandle,
                                               bt::LinkType::kACL, chan_info);
  service_iter->second.connect_callback(channel->GetWeakPtr(), MakeL2capProtocolListElement());
  RunLoopUntilIdle();
  ASSERT_TRUE(connect_receiver.channel().has_value());
  fidlbredr::Channel fidl_channel = connect_receiver.take_channel();
  ASSERT_TRUE(fidl_channel.has_flush_timeout());
  EXPECT_EQ(fidl_channel.flush_timeout(), kFlushTimeout.get());

  channel->Close();
  RunLoopUntilIdle();
}

TEST_F(ProfileServerTestFakeAdapter, L2capParametersExtRequestParametersSucceeds) {
  const bt::PeerId kPeerId;
  const fuchsia::bluetooth::PeerId kFidlPeerId{kPeerId.value()};
  const zx::duration kFlushTimeout(zx::msec(100));
  const uint16_t kMaxRxSduSize(200);

  FakeChannel::WeakPtr last_channel;
  adapter()->fake_bredr()->set_l2cap_channel_callback([&](auto chan) { last_channel = chan; });

  fidlbredr::ChannelParameters chan_params;
  chan_params.set_channel_mode(fidlbredr::ChannelMode::BASIC);
  chan_params.set_max_rx_sdu_size(kMaxRxSduSize);
  fidlbredr::L2capParameters l2cap_params;
  l2cap_params.set_psm(fidlbredr::PSM_AVDTP);
  l2cap_params.set_parameters(std::move(chan_params));
  fidlbredr::ConnectParameters conn_params;
  conn_params.set_l2cap(std::move(l2cap_params));

  std::optional<fidlbredr::Channel> response_channel;
  client()->Connect(kFidlPeerId, std::move(conn_params),
                    [&](fidlbredr::Profile_Connect_Result result) {
                      ASSERT_TRUE(result.is_response());
                      response_channel = std::move(result.response().channel);
                    });
  RunLoopUntilIdle();
  ASSERT_TRUE(last_channel.is_alive());
  EXPECT_FALSE(last_channel->info().flush_timeout.has_value());
  ASSERT_TRUE(response_channel.has_value());
  ASSERT_FALSE(response_channel->has_flush_timeout());
  ASSERT_TRUE(response_channel->has_ext_l2cap());

  fidlbredr::ChannelParameters request_chan_params;
  request_chan_params.set_flush_timeout(kFlushTimeout.get());

  std::optional<fidlbredr::ChannelParameters> result_chan_params;
  auto l2cap_client = response_channel->mutable_ext_l2cap()->Bind();
  l2cap_client->RequestParameters(
      std::move(request_chan_params),
      [&](fidlbredr::ChannelParameters new_params) { result_chan_params = std::move(new_params); });
  RunLoopUntilIdle();
  ASSERT_TRUE(result_chan_params.has_value());
  ASSERT_TRUE(result_chan_params->has_channel_mode());
  ASSERT_TRUE(result_chan_params->has_max_rx_sdu_size());
  // TODO(fxb/73039): set current security requirements in returned channel parameters
  ASSERT_FALSE(result_chan_params->has_security_requirements());
  ASSERT_TRUE(result_chan_params->has_flush_timeout());
  EXPECT_EQ(result_chan_params->channel_mode(), fidlbredr::ChannelMode::BASIC);
  EXPECT_EQ(result_chan_params->max_rx_sdu_size(), kMaxRxSduSize);
  EXPECT_EQ(result_chan_params->flush_timeout(), kFlushTimeout.get());
  l2cap_client.Unbind();
  RunLoopUntilIdle();
}

TEST_F(ProfileServerTestFakeAdapter, L2capParametersExtRequestParametersFails) {
  const bt::PeerId kPeerId;
  const fuchsia::bluetooth::PeerId kFidlPeerId{kPeerId.value()};
  const zx::duration kFlushTimeout(zx::msec(100));

  FakeChannel::WeakPtr last_channel;
  adapter()->fake_bredr()->set_l2cap_channel_callback([&](auto chan) { last_channel = chan; });

  fidlbredr::L2capParameters l2cap_params;
  l2cap_params.set_psm(fidlbredr::PSM_AVDTP);
  fidlbredr::ConnectParameters conn_params;
  conn_params.set_l2cap(std::move(l2cap_params));

  std::optional<fidlbredr::Channel> response_channel;
  client()->Connect(kFidlPeerId, std::move(conn_params),
                    [&](fidlbredr::Profile_Connect_Result result) {
                      ASSERT_TRUE(result.is_response());
                      response_channel = std::move(result.response().channel);
                    });
  RunLoopUntilIdle();
  ASSERT_TRUE(last_channel.is_alive());
  EXPECT_FALSE(last_channel->info().flush_timeout.has_value());
  ASSERT_TRUE(response_channel.has_value());
  ASSERT_FALSE(response_channel->has_flush_timeout());
  ASSERT_TRUE(response_channel->has_ext_l2cap());

  last_channel->set_flush_timeout_succeeds(false);

  fidlbredr::ChannelParameters request_chan_params;
  request_chan_params.set_flush_timeout(kFlushTimeout.get());
  std::optional<fidlbredr::ChannelParameters> result_chan_params;
  auto l2cap_client = response_channel->mutable_ext_l2cap()->Bind();
  l2cap_client->RequestParameters(
      std::move(request_chan_params),
      [&](fidlbredr::ChannelParameters new_params) { result_chan_params = std::move(new_params); });
  RunLoopUntilIdle();
  ASSERT_TRUE(result_chan_params.has_value());
  EXPECT_FALSE(result_chan_params->has_flush_timeout());
  l2cap_client.Unbind();
  RunLoopUntilIdle();
}

TEST_F(ProfileServerTestFakeAdapter, L2capParametersExtRequestParametersClosedOnChannelClosed) {
  const bt::PeerId kPeerId;
  const fuchsia::bluetooth::PeerId kFidlPeerId{kPeerId.value()};

  FakeChannel::WeakPtr last_channel;
  adapter()->fake_bredr()->set_l2cap_channel_callback([&](auto chan) { last_channel = chan; });

  fidlbredr::L2capParameters l2cap_params;
  l2cap_params.set_psm(fidlbredr::PSM_AVDTP);
  fidlbredr::ConnectParameters conn_params;
  conn_params.set_l2cap(std::move(l2cap_params));

  std::optional<fidlbredr::Channel> response_channel;
  client()->Connect(kFidlPeerId, std::move(conn_params),
                    [&](fidlbredr::Profile_Connect_Result result) {
                      ASSERT_TRUE(result.is_response());
                      response_channel = std::move(result.response().channel);
                    });
  RunLoopUntilIdle();
  ASSERT_TRUE(last_channel.is_alive());
  ASSERT_TRUE(response_channel.has_value());

  fidlbredr::L2capParametersExtPtr l2cap_client = response_channel->mutable_ext_l2cap()->Bind();
  bool l2cap_client_closed = false;
  l2cap_client.set_error_handler([&](zx_status_t /*status*/) { l2cap_client_closed = true; });

  // Closing the channel should close l2cap_client (after running the loop).
  last_channel->Close();
  // Destroy the channel (like the real LogicalLink would) to verify that ProfileServer doesn't try
  // to use channel pointers.
  EXPECT_TRUE(adapter()->fake_bredr()->DestroyChannel(last_channel->id()));

  // Any request for the closed channel should be ignored.
  fidlbredr::ChannelParameters request_chan_params;
  std::optional<fidlbredr::ChannelParameters> result_chan_params;
  l2cap_client->RequestParameters(
      std::move(request_chan_params),
      [&](fidlbredr::ChannelParameters new_params) { result_chan_params = std::move(new_params); });
  RunLoopUntilIdle();
  EXPECT_TRUE(l2cap_client_closed);
  EXPECT_FALSE(result_chan_params.has_value());
  l2cap_client.Unbind();
  RunLoopUntilIdle();
}

TEST_F(ProfileServerTestFakeAdapter, AudioDirectionExtRequestParametersClosedOnChannelClosed) {
  const bt::PeerId kPeerId;
  const fuchsia::bluetooth::PeerId kFidlPeerId{kPeerId.value()};

  FakeChannel::WeakPtr last_channel;
  adapter()->fake_bredr()->set_l2cap_channel_callback([&](auto chan) { last_channel = chan; });

  fidlbredr::L2capParameters l2cap_params;
  l2cap_params.set_psm(fidlbredr::PSM_AVDTP);
  fidlbredr::ConnectParameters conn_params;
  conn_params.set_l2cap(std::move(l2cap_params));

  std::optional<fidlbredr::Channel> response_channel;
  client()->Connect(kFidlPeerId, std::move(conn_params),
                    [&](fidlbredr::Profile_Connect_Result result) {
                      ASSERT_TRUE(result.is_response());
                      response_channel = std::move(result.response().channel);
                    });
  RunLoopUntilIdle();
  ASSERT_TRUE(last_channel.is_alive());
  ASSERT_TRUE(response_channel.has_value());

  fidlbredr::AudioDirectionExtPtr audio_client = response_channel->mutable_ext_direction()->Bind();
  bool audio_client_closed = false;
  audio_client.set_error_handler([&](zx_status_t /*status*/) { audio_client_closed = true; });

  // Closing the channel should close audio_client (after running the loop).
  last_channel->Close();
  // Destroy the channel (like the real LogicalLink would) to verify that ProfileServer doesn't try
  // to use channel pointers.
  EXPECT_TRUE(adapter()->fake_bredr()->DestroyChannel(last_channel->id()));

  // Any request for the closed channel should be ignored.
  fidlbredr::ChannelParameters request_chan_params;
  size_t priority_cb_count = 0;
  audio_client->SetPriority(fidlbredr::A2dpDirectionPriority::NORMAL,
                            [&](auto result) { priority_cb_count++; });

  RunLoopUntilIdle();
  EXPECT_TRUE(audio_client_closed);
  EXPECT_EQ(priority_cb_count, 0u);
  audio_client.Unbind();
  RunLoopUntilIdle();
}

class AndroidSupportedFeaturesTest
    : public ProfileServerTestFakeAdapter,
      public ::testing::WithParamInterface<
          std::pair<bool /*android_vendor_support*/, uint32_t /*android_vendor_capabilities*/>> {};

TEST_P(AndroidSupportedFeaturesTest, AudioOffloadExtGetSupportedFeatures) {
  const bool android_vendor_ext_support = GetParam().first;
  const uint32_t a2dp_offload_capabilities = GetParam().second;

  if (android_vendor_ext_support) {
    adapter()->mutable_state().controller_features |= FeaturesBits::kAndroidVendorExtensions;

    hci_android::LEGetVendorCapabilitiesReturnParams params;
    memset(&params, 0, sizeof(params));
    params.a2dp_source_offload_capability_mask = htole32(a2dp_offload_capabilities);
    params.status = pw::bluetooth::emboss::StatusCode::SUCCESS;
    adapter()->mutable_state().android_vendor_capabilities.Initialize(params);
  }

  const bt::PeerId peer_id(1);
  const fuchsia::bluetooth::PeerId fidl_peer_id{peer_id.value()};

  fidlbredr::L2capParameters l2cap_params;
  l2cap_params.set_psm(fidlbredr::PSM_AVDTP);

  fidlbredr::ChannelParameters chan_params;
  l2cap_params.set_parameters(std::move(chan_params));

  fidlbredr::ConnectParameters conn_params;
  conn_params.set_l2cap(std::move(l2cap_params));

  std::optional<fidlbredr::Channel> response_channel;
  client()->Connect(fidl_peer_id, std::move(conn_params),
                    [&response_channel](fidlbredr::Profile_Connect_Result result) {
                      ASSERT_TRUE(result.is_response());
                      response_channel = std::move(result.response().channel);
                    });
  RunLoopUntilIdle();
  ASSERT_TRUE(response_channel.has_value());
  if (!android_vendor_ext_support || !a2dp_offload_capabilities) {
    EXPECT_FALSE(response_channel->has_ext_audio_offload());
    return;
  }
  ASSERT_TRUE(response_channel->has_ext_audio_offload());

  std::optional<fidlbredr::AudioOffloadExtGetSupportedFeaturesResponse> result_features;
  fidl::InterfacePtr<fuchsia::bluetooth::bredr::AudioOffloadExt> audio_offload_client =
      response_channel->mutable_ext_audio_offload()->Bind();
  audio_offload_client->GetSupportedFeatures(
      [&result_features](fidlbredr::AudioOffloadExtGetSupportedFeaturesResponse features) {
        result_features = std::move(features);
      });
  RunLoopUntilIdle();

  EXPECT_TRUE(result_features->has_audio_offload_features());
  const std::vector<fidlbredr::AudioOffloadFeatures>& audio_offload_features =
      result_features->audio_offload_features();
  const uint32_t audio_offload_features_size =
      std::bitset<std::numeric_limits<uint32_t>::digits>(a2dp_offload_capabilities).count();
  EXPECT_EQ(audio_offload_features_size, audio_offload_features.size());

  uint32_t capabilities = 0;
  const uint32_t sbc_capability = static_cast<uint32_t>(hci_android::A2dpCodecType::kSbc);
  const uint32_t aac_capability = static_cast<uint32_t>(hci_android::A2dpCodecType::kAac);
  for (const fidlbredr::AudioOffloadFeatures& feature : audio_offload_features) {
    if (feature.is_sbc()) {
      capabilities |= sbc_capability;
    }
    if (feature.is_aac()) {
      capabilities |= aac_capability;
    }
  }
  EXPECT_EQ(capabilities, a2dp_offload_capabilities);
}

const std::vector<std::pair<bool, uint32_t>> kVendorCapabilitiesParams = {
    {{true, static_cast<uint32_t>(hci_android::A2dpCodecType::kSbc)},
     {true, static_cast<uint32_t>(hci_android::A2dpCodecType::kAac)},
     {true, static_cast<uint32_t>(hci_android::A2dpCodecType::kSbc) |
                static_cast<uint32_t>(hci_android::A2dpCodecType::kAac)},
     {true, 0},
     {false, 0}}};
INSTANTIATE_TEST_SUITE_P(ProfileServerTestFakeAdapter, AndroidSupportedFeaturesTest,
                         ::testing::ValuesIn(kVendorCapabilitiesParams));

TEST_F(ProfileServerTestFakeAdapter, ServiceFoundRelayedToFidlClient) {
  fidl::InterfaceHandle<fidlbredr::SearchResults> search_results_handle;
  FakeSearchResults search_results(search_results_handle.NewRequest(), dispatcher());

  auto search_uuid = fidlbredr::ServiceClassProfileIdentifier::AUDIO_SINK;
  auto attr_ids = std::vector<uint16_t>();

  EXPECT_EQ(adapter()->fake_bredr()->registered_searches().size(), 0u);
  EXPECT_EQ(search_results.service_found_count(), 0u);

  // FIDL client registers a service search.
  client()->Search(search_uuid, std::move(attr_ids), std::move(search_results_handle));
  RunLoopUntilIdle();

  EXPECT_EQ(adapter()->fake_bredr()->registered_searches().size(), 1u);

  // Trigger a match on the service search with some data. Should be received by the FIDL
  // client.
  auto peer_id = bt::PeerId{10};
  bt::UUID uuid(static_cast<uint32_t>(search_uuid));

  bt::sdp::AttributeId attr_id = 50;  // Random Attribute ID
  auto elem = bt::sdp::DataElement();
  elem.SetUrl("https://foobar.dev");  // Random URL
  auto attributes = std::map<bt::sdp::AttributeId, bt::sdp::DataElement>();
  attributes.emplace(attr_id, std::move(elem));
  adapter()->fake_bredr()->TriggerServiceFound(peer_id, uuid, std::move(attributes));

  RunLoopUntilIdle();

  EXPECT_EQ(search_results.service_found_count(), 1u);
  EXPECT_EQ(search_results.peer_id().value().value, peer_id.value());
  EXPECT_EQ(search_results.attributes().value().size(), 1u);
  EXPECT_EQ(search_results.attributes().value()[0].id, attr_id);
  EXPECT_EQ(search_results.attributes().value()[0].element.url(),
            std::string("https://foobar.dev"));
}

TEST_F(ProfileServerTestScoConnected, ScoConnectionRead2Packets) {
  // Queue a read request before the packet is received.
  std::optional<fuchsia::bluetooth::bredr::RxPacketStatus> packet_status;
  std::optional<std::vector<uint8_t>> packet;
  sco_connection()->Read([&](fidlbredr::RxPacketStatus status, std::vector<uint8_t> cb_packet) {
    packet_status = status;
    packet = std::move(cb_packet);
  });
  RunLoopUntilIdle();
  EXPECT_FALSE(packet_status);
  EXPECT_FALSE(packet);

  bt::StaticByteBuffer packet_buffer_0(
      bt::LowerBits(sco_handle()),
      bt::UpperBits(sco_handle()) | 0x30,  // handle + packet status flag: kDataPartiallyLost
      0x01,                                // payload length
      0x00                                 // payload
  );
  bt::BufferView packet_buffer_0_payload =
      packet_buffer_0.view(sizeof(bt::hci_spec::SynchronousDataHeader));
  test_device()->SendScoDataChannelPacket(packet_buffer_0);
  RunLoopUntilIdle();
  ASSERT_TRUE(packet_status);
  EXPECT_EQ(packet_status.value(), fidlbredr::RxPacketStatus::DATA_PARTIALLY_LOST);
  ASSERT_TRUE(packet);
  EXPECT_THAT(packet.value(), ::testing::ElementsAreArray(packet_buffer_0_payload));
  packet_status.reset();
  packet.reset();

  // Receive a second packet. This time, receive the packet before Read() is called.
  bt::StaticByteBuffer packet_buffer_1(
      bt::LowerBits(sco_handle()),
      bt::UpperBits(sco_handle()),  // handle + packet status flag: kCorrectlyReceived
      0x01,                         // payload length
      0x01                          // payload
  );
  bt::BufferView packet_buffer_1_payload =
      packet_buffer_1.view(sizeof(bt::hci_spec::SynchronousDataHeader));
  test_device()->SendScoDataChannelPacket(packet_buffer_1);
  RunLoopUntilIdle();

  sco_connection()->Read([&](fidlbredr::RxPacketStatus status, std::vector<uint8_t> cb_packet) {
    packet_status = status;
    packet = std::move(cb_packet);
  });
  RunLoopUntilIdle();
  ASSERT_TRUE(packet_status);
  EXPECT_EQ(packet_status.value(), fidlbredr::RxPacketStatus::CORRECTLY_RECEIVED_DATA);
  ASSERT_TRUE(packet);
  EXPECT_THAT(packet.value(), ::testing::ElementsAreArray(packet_buffer_1_payload));
}

TEST_F(ProfileServerTestScoConnected, ScoConnectionReadWhileReadPendingClosesConnection) {
  std::optional<fuchsia::bluetooth::bredr::RxPacketStatus> packet_status_0;
  std::optional<std::vector<uint8_t>> packet_0;
  sco_connection()->Read([&](fidlbredr::RxPacketStatus status, std::vector<uint8_t> cb_packet) {
    packet_status_0 = status;
    packet_0 = std::move(cb_packet);
  });

  RunLoopUntilIdle();
  EXPECT_FALSE(packet_status_0);
  EXPECT_FALSE(packet_0);

  std::optional<fuchsia::bluetooth::bredr::RxPacketStatus> packet_status_1;
  std::optional<std::vector<uint8_t>> packet_1;
  sco_connection()->Read([&](fidlbredr::RxPacketStatus status, std::vector<uint8_t> cb_packet) {
    packet_status_1 = status;
    packet_1 = std::move(cb_packet);
  });

  RunLoopUntilIdle();
  EXPECT_FALSE(packet_status_0);
  EXPECT_FALSE(packet_0);
  EXPECT_FALSE(packet_status_1);
  EXPECT_FALSE(packet_1);
  EXPECT_FALSE(sco_connection());
  ASSERT_TRUE(sco_conn_error());
  EXPECT_EQ(sco_conn_error().value(), ZX_ERR_BAD_STATE);
}

TEST_F(ProfileServerTestOffloadedScoConnected, ScoConnectionReadFails) {
  std::optional<fuchsia::bluetooth::bredr::RxPacketStatus> packet_status;
  std::optional<std::vector<uint8_t>> packet;
  sco_connection()->Read([&](fidlbredr::RxPacketStatus status, std::vector<uint8_t> cb_packet) {
    packet_status = status;
    packet = std::move(cb_packet);
  });

  RunLoopUntilIdle();
  EXPECT_FALSE(packet_status);
  EXPECT_FALSE(packet);
  EXPECT_FALSE(sco_connection());
  ASSERT_TRUE(sco_conn_error());
  EXPECT_EQ(sco_conn_error().value(), ZX_ERR_IO_NOT_PRESENT);
}

TEST_F(ProfileServerTestScoConnected, ScoConnectionWriteTwice) {
  bt::StaticByteBuffer payload_0(0x00);
  bt::DynamicByteBuffer packet_buffer_0 = bt::testing::ScoDataPacket(
      sco_handle(), bt::hci_spec::SynchronousDataPacketStatusFlag::kCorrectlyReceived,
      payload_0.view());

  bt::StaticByteBuffer payload_1(0x01);
  bt::DynamicByteBuffer packet_buffer_1 = bt::testing::ScoDataPacket(
      sco_handle(), bt::hci_spec::SynchronousDataPacketStatusFlag::kCorrectlyReceived,
      payload_1.view());

  int sco_cb_count = 0;
  test_device()->SetScoDataCallback([&](const bt::ByteBuffer& buffer) {
    if (sco_cb_count == 0) {
      EXPECT_THAT(buffer, ::testing::ElementsAreArray(packet_buffer_0));
    } else if (sco_cb_count == 1) {
      EXPECT_THAT(buffer, ::testing::ElementsAreArray(packet_buffer_1));
    } else {
      ADD_FAILURE() << "Unexpected packet sent";
    }
    sco_cb_count++;
  });
  int write_cb_0_count = 0;
  sco_connection()->Write(payload_0.ToVector(), [&] { write_cb_0_count++; });
  RunLoopUntilIdle();
  EXPECT_EQ(sco_cb_count, 1);
  EXPECT_EQ(write_cb_0_count, 1);

  int write_cb_1_count = 0;
  sco_connection()->Write(payload_1.ToVector(), [&] { write_cb_1_count++; });
  RunLoopUntilIdle();
  EXPECT_EQ(sco_cb_count, 2);
  EXPECT_EQ(write_cb_1_count, 1);

  test_device()->ClearScoDataCallback();
}

TEST_F(ProfileServerTestScoConnected, ScoConnectionWriteTooLarge) {
  bt::StaticByteBuffer<kSynchronousDataPacketLength + 1> payload_buffer;
  payload_buffer.Fill(0x00);

  int sco_cb_count = 0;
  test_device()->SetScoDataCallback([&](const bt::ByteBuffer& buffer) { sco_cb_count++; });
  int write_cb_count = 0;
  sco_connection()->Write(payload_buffer.ToVector(), [&] { write_cb_count++; });
  RunLoopUntilIdle();
  EXPECT_EQ(sco_cb_count, 0);
  EXPECT_EQ(write_cb_count, 0);
  EXPECT_FALSE(sco_connection());
  ASSERT_TRUE(sco_conn_error());
  EXPECT_EQ(sco_conn_error().value(), ZX_ERR_IO);

  test_device()->ClearScoDataCallback();
}

TEST_F(ProfileServerTestOffloadedScoConnected, ScoConnectionWriteFails) {
  int write_cb_count = 0;
  sco_connection()->Write(/*data=*/{0x00}, [&] { write_cb_count++; });
  RunLoopUntilIdle();
  EXPECT_EQ(write_cb_count, 0);
  EXPECT_FALSE(sco_connection());
  ASSERT_TRUE(sco_conn_error());
  EXPECT_EQ(sco_conn_error().value(), ZX_ERR_IO_NOT_PRESENT);
}

}  // namespace
}  // namespace bthost
