// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/data/domain.h"

#include <lib/async/cpp/task.h>

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/hci.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/test_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/types.h"
#include "src/connectivity/bluetooth/core/bt-host/sm/status.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_packets.h"

// This test harness provides test cases for interations between L2CAP, RFCOMM,
// and SocketFactory in integration, as they are implemented by the domain
// object. These exercise a production data plane against raw HCI endpoints.

namespace bt {
namespace data {
namespace {

using bt::testing::TestController;
using TestingBase = bt::testing::FakeControllerTest<TestController>;

// Sized intentionally to cause fragmentation for outbound dynamic channel data but not others. May
// need adjustment if Signaling Channel Configuration {Request,Response} default transactions get
// much bigger.
constexpr size_t kMaxDataPacketLength = 64;
constexpr size_t kMaxPacketCount = 10;

// 2x Information Requests: Extended Features, Fixed Channels Supported
constexpr size_t kConnectionCreationPacketCount = 2;

constexpr l2cap::ChannelParameters kChannelParameters{l2cap::ChannelMode::kBasic, l2cap::kMaxMTU};
constexpr l2cap::ExtendedFeatures kExtendedFeatures =
    l2cap::kExtendedFeaturesBitEnhancedRetransmission;

class DATA_DomainTest : public TestingBase {
 public:
  DATA_DomainTest() = default;
  ~DATA_DomainTest() override = default;

 protected:
  void SetUp() override {
    TestingBase::SetUp();
    const auto bredr_buffer_info = hci::DataBufferInfo(kMaxDataPacketLength, kMaxPacketCount);
    InitializeACLDataChannel(bredr_buffer_info);

    domain_ = Domain::CreateWithDispatcher(transport(), dispatcher());
    domain_->Initialize();

    StartTestDevice();

    test_device()->set_data_expectations_enabled(true);

    next_command_id_ = 1;
  }

  void TearDown() override {
    domain_->ShutDown();
    domain_ = nullptr;
    TestingBase::TearDown();
  }

  l2cap::CommandId NextCommandId() { return next_command_id_++; }

  void QueueConfigNegotiation(hci::ConnectionHandle handle, l2cap::ChannelParameters local_params,
                              l2cap::ChannelParameters peer_params, l2cap::ChannelId local_cid,
                              l2cap::ChannelId remote_cid, l2cap::CommandId local_config_req_id,
                              l2cap::CommandId peer_config_req_id) {
    const auto kPeerConfigRsp =
        l2cap::testing::AclConfigRsp(local_config_req_id, handle, local_cid, local_params);
    const auto kPeerConfigReq =
        l2cap::testing::AclConfigReq(peer_config_req_id, handle, local_cid, peer_params);
    EXPECT_ACL_PACKET_OUT(
        test_device(),
        l2cap::testing::AclConfigReq(local_config_req_id, handle, remote_cid, local_params),
        &kPeerConfigRsp, &kPeerConfigReq);
    EXPECT_ACL_PACKET_OUT(test_device(), l2cap::testing::AclConfigRsp(peer_config_req_id, handle,
                                                                      remote_cid, peer_params));
  }

  void QueueInboundL2capConnection(hci::ConnectionHandle handle, l2cap::PSM psm,
                                   l2cap::ChannelId local_cid, l2cap::ChannelId remote_cid,
                                   l2cap::ChannelParameters local_params = kChannelParameters,
                                   l2cap::ChannelParameters peer_params = kChannelParameters) {
    const l2cap::CommandId kPeerConnReqId = 1;
    const l2cap::CommandId kPeerConfigReqId = kPeerConnReqId + 1;
    const auto kConfigReqId = NextCommandId();
    EXPECT_ACL_PACKET_OUT(test_device(), l2cap::testing::AclConnectionRsp(kPeerConnReqId, handle,
                                                                          remote_cid, local_cid));
    QueueConfigNegotiation(handle, local_params, peer_params, local_cid, remote_cid, kConfigReqId,
                           kPeerConfigReqId);

    test_device()->SendACLDataChannelPacket(
        l2cap::testing::AclConnectionReq(kPeerConnReqId, handle, remote_cid, psm));
  }

  template <typename CallbackT>
  void QueueOutboundL2capConnection(hci::ConnectionHandle handle, l2cap::PSM psm,
                                    l2cap::ChannelId local_cid, l2cap::ChannelId remote_cid,
                                    CallbackT open_cb,
                                    l2cap::ChannelParameters local_params = kChannelParameters,
                                    l2cap::ChannelParameters peer_params = kChannelParameters) {
    const l2cap::CommandId kPeerConfigReqId = 1;
    const auto kConnReqId = NextCommandId();
    const auto kConfigReqId = NextCommandId();
    const auto kConnRsp =
        l2cap::testing::AclConnectionRsp(kConnReqId, handle, local_cid, remote_cid);
    EXPECT_ACL_PACKET_OUT(test_device(),
                          l2cap::testing::AclConnectionReq(kConnReqId, handle, local_cid, psm),
                          &kConnRsp);
    QueueConfigNegotiation(handle, local_params, peer_params, local_cid, remote_cid, kConfigReqId,
                           kPeerConfigReqId);

    domain()->OpenL2capChannel(handle, psm, local_params, std::move(open_cb), dispatcher());
  }

  struct QueueAclConnectionRetVal {
    l2cap::CommandId extended_features_id;
    l2cap::CommandId fixed_channels_supported_id;
  };

  QueueAclConnectionRetVal QueueAclConnection(
      hci::ConnectionHandle handle, hci::Connection::Role role = hci::Connection::Role::kMaster) {
    QueueAclConnectionRetVal cmd_ids;
    cmd_ids.extended_features_id = NextCommandId();
    cmd_ids.fixed_channels_supported_id = NextCommandId();

    const auto kExtFeaturesRsp = l2cap::testing::AclExtFeaturesInfoRsp(cmd_ids.extended_features_id,
                                                                       handle, kExtendedFeatures);
    EXPECT_ACL_PACKET_OUT(
        test_device(), l2cap::testing::AclExtFeaturesInfoReq(cmd_ids.extended_features_id, handle),
        &kExtFeaturesRsp);
    EXPECT_ACL_PACKET_OUT(test_device(), l2cap::testing::AclFixedChannelsSupportedInfoReq(
                                             cmd_ids.fixed_channels_supported_id, handle));

    acl_data_channel()->RegisterLink(handle, hci::Connection::LinkType::kACL);
    domain()->AddACLConnection(
        handle, role, /*link_error_callback=*/[]() {},
        /*security_upgrade_callback=*/[](auto, auto, auto) {}, dispatcher());
    return cmd_ids;
  }

  Domain* domain() const { return domain_.get(); }

 private:
  fbl::RefPtr<Domain> domain_;
  l2cap::CommandId next_command_id_;

  DISALLOW_COPY_ASSIGN_AND_MOVE(DATA_DomainTest);
};

TEST_F(DATA_DomainTest, InboundL2capSocket) {
  constexpr l2cap::PSM kPSM = l2cap::kAVDTP;
  constexpr l2cap::ChannelId kLocalId = 0x0040;
  constexpr l2cap::ChannelId kRemoteId = 0x9042;
  constexpr hci::ConnectionHandle kLinkHandle = 0x0001;

  QueueAclConnection(kLinkHandle);

  zx::socket sock;
  ASSERT_FALSE(sock);
  auto sock_cb = [&](auto chan_sock, hci::ConnectionHandle handle) {
    EXPECT_EQ(kLinkHandle, handle);
    sock = std::move(chan_sock.socket);
  };

  domain()->RegisterService(kPSM, kChannelParameters, std::move(sock_cb), dispatcher());
  RunLoopUntilIdle();

  QueueInboundL2capConnection(kLinkHandle, kPSM, kLocalId, kRemoteId);

  RunLoopUntilIdle();
  ASSERT_TRUE(sock);

  // Test basic channel<->socket interaction by verifying that an ACL packet
  // gets routed to the socket.
  test_device()->SendACLDataChannelPacket(CreateStaticByteBuffer(
      // ACL data header (handle: 1, length 8)
      0x01, 0x00, 0x08, 0x00,

      // L2CAP B-frame: (length: 4, channel-id: 0x0040 (kLocalId))
      0x04, 0x00, 0x40, 0x00, 't', 'e', 's', 't'));

  // Run until the packet is written to the socket buffer.
  RunLoopUntilIdle();

  // Allocate a larger buffer than the number of SDU bytes we expect (which is
  // 4).
  StaticByteBuffer<10> socket_bytes;
  size_t bytes_read;
  zx_status_t status = sock.read(0, socket_bytes.mutable_data(), socket_bytes.size(), &bytes_read);
  EXPECT_EQ(ZX_OK, status);
  ASSERT_EQ(4u, bytes_read);
  EXPECT_EQ("test", socket_bytes.view(0, bytes_read).AsString());

  const char write_data[81] =
      u8"🚂🚃🚄🚅🚆🚈🚇🚈🚉🚊🚋🚌🚎🚝🚞🚟🚠🚡🛤🛲";

  // Test outbound data fragments using |kMaxDataPacketLength|.
  constexpr size_t kFirstFragmentPayloadSize = kMaxDataPacketLength - sizeof(l2cap::BasicHeader);
  const auto kFirstFragment =
      StaticByteBuffer<sizeof(hci::ACLDataHeader) + sizeof(l2cap::BasicHeader) +
                       kFirstFragmentPayloadSize>(
          // ACL data header (handle: 1, length 64)
          0x01, 0x00, 0x40, 0x00,

          // L2CAP B-frame: (length: 80, channel-id: 0x9042 (kRemoteId))
          0x50, 0x00, 0x42, 0x90,

          // L2CAP payload (fragmented)
          0xf0, 0x9f, 0x9a, 0x82, 0xf0, 0x9f, 0x9a, 0x83, 0xf0, 0x9f, 0x9a, 0x84, 0xf0, 0x9f, 0x9a,
          0x85, 0xf0, 0x9f, 0x9a, 0x86, 0xf0, 0x9f, 0x9a, 0x88, 0xf0, 0x9f, 0x9a, 0x87, 0xf0, 0x9f,
          0x9a, 0x88, 0xf0, 0x9f, 0x9a, 0x89, 0xf0, 0x9f, 0x9a, 0x8a, 0xf0, 0x9f, 0x9a, 0x8b, 0xf0,
          0x9f, 0x9a, 0x8c, 0xf0, 0x9f, 0x9a, 0x8e, 0xf0, 0x9f, 0x9a, 0x9d, 0xf0, 0x9f, 0x9a, 0x9e);

  constexpr size_t kSecondFragmentPayloadSize = sizeof(write_data) - 1 - kFirstFragmentPayloadSize;
  const auto kSecondFragment =
      StaticByteBuffer<sizeof(hci::ACLDataHeader) + kSecondFragmentPayloadSize>(
          // ACL data header (handle: 1, pbf: continuing fr., length: 20)
          0x01, 0x10, 0x14, 0x00,

          // L2CAP payload (final fragment)
          0xf0, 0x9f, 0x9a, 0x9f, 0xf0, 0x9f, 0x9a, 0xa0, 0xf0, 0x9f, 0x9a, 0xa1, 0xf0, 0x9f, 0x9b,
          0xa4, 0xf0, 0x9f, 0x9b, 0xb2);

  // The 80-byte write should be fragmented over 64- and 20-byte HCI payloads in order to send it
  // to the controller.
  EXPECT_ACL_PACKET_OUT(test_device(), kFirstFragment);
  EXPECT_ACL_PACKET_OUT(test_device(), kSecondFragment);

  size_t bytes_written = 0;
  // Write 80 outbound bytes to the socket buffer.
  status = sock.write(0, write_data, sizeof(write_data) - 1, &bytes_written);
  EXPECT_EQ(ZX_OK, status);
  EXPECT_EQ(80u, bytes_written);

  // Run until the data is flushed out to the TestController.
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());

  domain()->RemoveConnection(kLinkHandle);
  acl_data_channel()->UnregisterLink(kLinkHandle);
  acl_data_channel()->ClearControllerPacketCount(kLinkHandle);

  // try resending data now that connection is closed
  bytes_written = 0;
  status = sock.write(0, write_data, sizeof(write_data) - 1, &bytes_written);

  EXPECT_EQ(ZX_OK, status);
  EXPECT_EQ(80u, bytes_written);

  // no packets should be sent
  RunLoopUntilIdle();
}

TEST_F(DATA_DomainTest, InboundRfcommSocketFails) {
  constexpr l2cap::PSM kPSM = l2cap::kRFCOMM;
  constexpr l2cap::ChannelId kRemoteId = 0x9042;
  constexpr hci::ConnectionHandle kLinkHandle = 0x0001;

  QueueAclConnection(kLinkHandle);

  RunLoopUntilIdle();

  const l2cap::CommandId kPeerConnReqId = 1;

  // Incoming connection refused, RFCOMM is not routed.
  EXPECT_ACL_PACKET_OUT(
      test_device(),
      l2cap::testing::AclConnectionRsp(kPeerConnReqId, kLinkHandle, kRemoteId, 0x0000 /*dest id*/,
                                       l2cap::ConnectionResult::kPSMNotSupported));

  test_device()->SendACLDataChannelPacket(
      l2cap::testing::AclConnectionReq(kPeerConnReqId, kLinkHandle, kRemoteId, kPSM));

  RunLoopUntilIdle();
}

TEST_F(DATA_DomainTest, InboundPacketQueuedAfterChannelOpenIsNotDropped) {
  constexpr l2cap::PSM kPSM = l2cap::kSDP;
  constexpr l2cap::ChannelId kLocalId = 0x0040;
  constexpr l2cap::ChannelId kRemoteId = 0x9042;
  constexpr hci::ConnectionHandle kLinkHandle = 0x0001;

  QueueAclConnection(kLinkHandle);

  zx::socket sock;
  ASSERT_FALSE(sock);
  auto sock_cb = [&](auto chan_sock, hci::ConnectionHandle handle) {
    EXPECT_EQ(kLinkHandle, handle);
    sock = std::move(chan_sock.socket);
  };

  domain()->RegisterService(kPSM, kChannelParameters, std::move(sock_cb), dispatcher());
  RunLoopUntilIdle();

  constexpr l2cap::CommandId kConnectionReqId = 1;
  constexpr l2cap::CommandId kPeerConfigReqId = 6;
  const l2cap::CommandId kConfigReqId = NextCommandId();
  EXPECT_ACL_PACKET_OUT(test_device(), l2cap::testing::AclConnectionRsp(
                                           kConnectionReqId, kLinkHandle, kRemoteId, kLocalId));
  EXPECT_ACL_PACKET_OUT(test_device(), l2cap::testing::AclConfigReq(kConfigReqId, kLinkHandle,
                                                                    kRemoteId, kChannelParameters));
  test_device()->SendACLDataChannelPacket(
      l2cap::testing::AclConnectionReq(kConnectionReqId, kLinkHandle, kRemoteId, kPSM));

  // Config negotiation will not complete yet.
  RunLoopUntilIdle();

  // Remaining config negotiation will be added to dispatch loop.
  EXPECT_ACL_PACKET_OUT(test_device(), l2cap::testing::AclConfigRsp(kPeerConfigReqId, kLinkHandle,
                                                                    kRemoteId, kChannelParameters));
  test_device()->SendACLDataChannelPacket(
      l2cap::testing::AclConfigReq(kPeerConfigReqId, kLinkHandle, kLocalId, kChannelParameters));
  test_device()->SendACLDataChannelPacket(
      l2cap::testing::AclConfigRsp(kConfigReqId, kLinkHandle, kLocalId, kChannelParameters));

  // Queue up a data packet for the new channel before the channel configuration has been
  // processed.
  ASSERT_FALSE(sock);
  test_device()->SendACLDataChannelPacket(CreateStaticByteBuffer(
      // ACL data header (handle: 1, length 8)
      0x01, 0x00, 0x08, 0x00,

      // L2CAP B-frame: (length: 4, channel-id: 0x0040 (kLocalId))
      0x04, 0x00, 0x40, 0x00, 0xf0, 0x9f, 0x94, 0xb0));

  // Run until the socket opens and the packet is written to the socket buffer.
  RunLoopUntilIdle();
  ASSERT_TRUE(sock);

  // Allocate a larger buffer than the number of SDU bytes we expect (which is 4).
  StaticByteBuffer<10> socket_bytes;
  size_t bytes_read;
  zx_status_t status = sock.read(0, socket_bytes.mutable_data(), socket_bytes.size(), &bytes_read);
  EXPECT_EQ(ZX_OK, status);
  ASSERT_EQ(4u, bytes_read);
  EXPECT_EQ(u8"🔰", socket_bytes.view(0, bytes_read).AsString());
}

TEST_F(DATA_DomainTest, OutboundL2capSocket) {
  constexpr l2cap::PSM kPSM = l2cap::kAVCTP;
  constexpr l2cap::ChannelId kLocalId = 0x0040;
  constexpr l2cap::ChannelId kRemoteId = 0x9042;
  constexpr hci::ConnectionHandle kLinkHandle = 0x0001;

  QueueAclConnection(kLinkHandle);
  RunLoopUntilIdle();

  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());

  zx::socket sock;
  ASSERT_FALSE(sock);
  auto sock_cb = [&](auto chan_sock, hci::ConnectionHandle handle) {
    EXPECT_EQ(kLinkHandle, handle);
    sock = std::move(chan_sock.socket);
  };

  QueueOutboundL2capConnection(kLinkHandle, kPSM, kLocalId, kRemoteId, std::move(sock_cb));

  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());
  // We should have opened a channel successfully.
  ASSERT_TRUE(sock);

  // Test basic channel<->socket interaction by verifying that an ACL packet
  // gets routed to the socket.
  test_device()->SendACLDataChannelPacket(CreateStaticByteBuffer(
      // ACL data header (handle: 1, length 8)
      0x01, 0x00, 0x08, 0x00,

      // L2CAP B-frame: (length: 4, channel-id: 0x0040 (kLocalId))
      0x04, 0x00, 0x40, 0x00, 't', 'e', 's', 't'));

  // Run until the packet is written to the socket buffer.
  RunLoopUntilIdle();

  // Allocate a larger buffer than the number of SDU bytes we expect (which is
  // 4).
  StaticByteBuffer<10> socket_bytes;
  size_t bytes_read;
  zx_status_t status = sock.read(0, socket_bytes.mutable_data(), socket_bytes.size(), &bytes_read);
  EXPECT_EQ(ZX_OK, status);
  ASSERT_EQ(4u, bytes_read);
  EXPECT_EQ("test", socket_bytes.view(0, bytes_read).AsString());
}

TEST_F(DATA_DomainTest, OutboundSocketIsInvalidWhenL2capFailsToOpenChannel) {
  constexpr l2cap::PSM kPSM = l2cap::kAVCTP;
  constexpr hci::ConnectionHandle kLinkHandle = 0x0001;

  // Don't register any links. This should cause outbound channels to fail.
  bool sock_cb_called = false;
  auto sock_cb = [&sock_cb_called, kLinkHandle](auto chan_sock, hci::ConnectionHandle handle) {
    sock_cb_called = true;
    EXPECT_FALSE(chan_sock);
    EXPECT_EQ(kLinkHandle, handle);
  };

  domain()->OpenL2capChannel(kLinkHandle, kPSM, kChannelParameters, std::move(sock_cb),
                             dispatcher());

  RunLoopUntilIdle();

  EXPECT_TRUE(sock_cb_called);
}

// Queue dynamic channel packets, then open a new dynamic channel.
// The signaling channel packets should be sent before the queued dynamic channel packets.
TEST_F(DATA_DomainTest, ChannelCreationPrioritizedOverDynamicChannelData) {
  constexpr hci::ConnectionHandle kLinkHandle = 0x0001;

  constexpr l2cap::PSM kPSM0 = l2cap::kAVCTP;
  constexpr l2cap::ChannelId kLocalId0 = 0x0040;
  constexpr l2cap::ChannelId kRemoteId0 = 0x9042;

  constexpr l2cap::PSM kPSM1 = l2cap::kAVDTP;
  constexpr l2cap::ChannelId kLocalId1 = 0x0041;
  constexpr l2cap::ChannelId kRemoteId1 = 0x9043;

  // l2cap connection request (or response), config request, config response
  constexpr size_t kChannelCreationPacketCount = 3;

  QueueAclConnection(kLinkHandle);

  zx::socket sock0;
  ASSERT_FALSE(sock0);
  auto sock_cb0 = [&](auto chan_sock, hci::ConnectionHandle handle) {
    EXPECT_EQ(kLinkHandle, handle);
    sock0 = std::move(chan_sock.socket);
  };
  domain()->RegisterService(kPSM0, kChannelParameters, sock_cb0, dispatcher());

  QueueInboundL2capConnection(kLinkHandle, kPSM0, kLocalId0, kRemoteId0);

  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());
  ASSERT_TRUE(sock0);

  test_device()->SendCommandChannelPacket(testing::NumberOfCompletedPacketsPacket(
      kLinkHandle, kConnectionCreationPacketCount + kChannelCreationPacketCount));

  // Dummy dynamic channel packet
  const auto kPacket0 = CreateStaticByteBuffer(
      // ACL data header (handle: 1, length 5)
      0x01, 0x00, 0x05, 0x00,

      // L2CAP B-frame: (length: 1, channel-id: 0x9042 (kRemoteId))
      0x01, 0x00, 0x42, 0x90,

      // L2CAP payload
      0x01);

  // |kMaxPacketCount| packets should be sent to the controller,
  // and 1 packet should be left in the queue
  const char write_data[] = {0x01};
  for (size_t i = 0; i < kMaxPacketCount + 1; i++) {
    if (i != kMaxPacketCount) {
      EXPECT_ACL_PACKET_OUT(test_device(), kPacket0);
    }
    size_t bytes_written = 0;
    zx_status_t status = sock0.write(0, write_data, sizeof(write_data), &bytes_written);
    EXPECT_EQ(ZX_OK, status);
    EXPECT_EQ(sizeof(write_data), bytes_written);
  }

  EXPECT_FALSE(test_device()->AllExpectedDataPacketsSent());
  // Run until the data is flushed out to the TestController.
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());

  zx::socket sock1;
  ASSERT_FALSE(sock1);
  auto sock_cb1 = [&](auto chan_sock, hci::ConnectionHandle handle) {
    EXPECT_EQ(kLinkHandle, handle);
    sock1 = std::move(chan_sock.socket);
  };

  QueueOutboundL2capConnection(kLinkHandle, kPSM1, kLocalId1, kRemoteId1, std::move(sock_cb1));

  for (size_t i = 0; i < kChannelCreationPacketCount; i++) {
    test_device()->SendCommandChannelPacket(
        testing::NumberOfCompletedPacketsPacket(kLinkHandle, 1));
    // Wait for next connection creation packet to be queued (eg. configuration request/response).
    RunLoopUntilIdle();
  }

  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());
  EXPECT_TRUE(sock1);

  // Make room in buffer for queued dynamic channel packet.
  test_device()->SendCommandChannelPacket(testing::NumberOfCompletedPacketsPacket(kLinkHandle, 1));

  EXPECT_ACL_PACKET_OUT(test_device(), kPacket0);
  RunLoopUntilIdle();
  // 1 Queued dynamic channel data packet should have been sent.
  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());
}

using DATA_DomainLifecycleTest = TestingBase;

TEST_F(DATA_DomainLifecycleTest, ShutdownWithoutInitialize) {
  // Create an isolated domain, and shutdown without initializing
  // We can't use the standard DATA_DomainTest fixture, as it will initialize the domain
  auto data_domain = Domain::CreateWithDispatcher(transport(), dispatcher());
  data_domain->ShutDown();
  data_domain = nullptr;
  SUCCEED();
}

TEST_F(DATA_DomainTest, NegotiateChannelParametersOnOutboundL2capSocket) {
  constexpr l2cap::PSM kPSM = l2cap::kAVDTP;
  constexpr l2cap::ChannelId kLocalId = 0x0040;
  constexpr l2cap::ChannelId kRemoteId = 0x9042;
  constexpr hci::ConnectionHandle kLinkHandle = 0x0001;
  constexpr uint16_t kMtu = l2cap::kMinACLMTU;

  l2cap::ChannelParameters chan_params;
  chan_params.mode = l2cap::ChannelMode::kEnhancedRetransmission;
  chan_params.max_rx_sdu_size = kMtu;

  QueueAclConnection(kLinkHandle);
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());

  fbl::RefPtr<l2cap::Channel> chan;
  auto chan_cb = [&](fbl::RefPtr<l2cap::Channel> cb_chan) { chan = std::move(cb_chan); };

  QueueOutboundL2capConnection(kLinkHandle, kPSM, kLocalId, kRemoteId, chan_cb, chan_params,
                               chan_params);

  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());
  ASSERT_TRUE(chan);
  EXPECT_EQ(kLinkHandle, chan->link_handle());
  EXPECT_EQ(*chan_params.max_rx_sdu_size, chan->max_rx_sdu_size());
  EXPECT_EQ(*chan_params.mode, chan->mode());
}

TEST_F(DATA_DomainTest, NegotiateChannelParametersOnInboundL2capSocket) {
  constexpr l2cap::PSM kPSM = l2cap::kAVDTP;
  constexpr l2cap::ChannelId kLocalId = 0x0040;
  constexpr l2cap::ChannelId kRemoteId = 0x9042;
  constexpr hci::ConnectionHandle kLinkHandle = 0x0001;

  l2cap::ChannelParameters chan_params;
  chan_params.mode = l2cap::ChannelMode::kEnhancedRetransmission;
  chan_params.max_rx_sdu_size = l2cap::kMinACLMTU;

  QueueAclConnection(kLinkHandle);
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());

  std::optional<l2cap::ChannelSocket> chan;
  auto sock_cb = [&](auto cb_chan, auto /*handle*/) { chan.emplace(std::move(cb_chan)); };
  domain()->RegisterService(kPSM, chan_params, sock_cb, dispatcher());

  QueueInboundL2capConnection(kLinkHandle, kPSM, kLocalId, kRemoteId, chan_params, chan_params);

  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());
  ASSERT_TRUE(chan);
  EXPECT_EQ(*chan_params.max_rx_sdu_size, chan->params->max_rx_sdu_size);
  EXPECT_EQ(*chan_params.mode, chan->params->mode);
}

}  // namespace
}  // namespace data
}  // namespace bt
