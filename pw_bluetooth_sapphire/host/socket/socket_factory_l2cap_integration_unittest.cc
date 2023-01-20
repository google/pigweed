// Copyright 2023 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include "socket_factory.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_manager.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/channel_manager_mock_controller_test_fixture.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/test_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/mock_controller.h"

namespace bt::socket {
namespace {

using namespace bt::testing;

using TestingBase = l2cap::ChannelManagerMockControllerTest;

// This test harness provides test cases for interations between SocketFactory and the L2cap layer.
class SocketFactoryL2capIntegrationTest : public TestingBase {
 public:
  SocketFactoryL2capIntegrationTest() = default;
  ~SocketFactoryL2capIntegrationTest() override = default;

 protected:
  void SetUp() override {
    TestingBase::SetUp();
    socket_factory_ = std::make_unique<socket::SocketFactory<l2cap::Channel>>();
  }

  void TearDown() override {
    socket_factory_.reset();
    TestingBase::TearDown();
  }

  zx::socket MakeSocketForChannel(l2cap::Channel::WeakPtr channel) {
    return socket_factory_->MakeSocketForChannel(std::move(channel));
  }

 private:
  std::unique_ptr<socket::SocketFactory<l2cap::Channel>> socket_factory_;

  BT_DISALLOW_COPY_ASSIGN_AND_MOVE(SocketFactoryL2capIntegrationTest);
};

TEST_F(SocketFactoryL2capIntegrationTest, InboundL2capSocket) {
  constexpr l2cap::PSM kPSM = l2cap::kAVDTP;
  constexpr l2cap::ChannelId kLocalId = 0x0040;
  constexpr l2cap::ChannelId kRemoteId = 0x9042;
  constexpr hci_spec::ConnectionHandle kLinkHandle = 0x0001;

  QueueAclConnection(kLinkHandle);

  l2cap::Channel::WeakPtr chan;
  auto chan_cb = [&](auto cb_chan) {
    EXPECT_EQ(kLinkHandle, cb_chan->link_handle());
    chan = std::move(cb_chan);
  };

  chanmgr()->RegisterService(kPSM, kChannelParameters, std::move(chan_cb));
  RunLoopUntilIdle();

  QueueInboundL2capConnection(kLinkHandle, kPSM, kLocalId, kRemoteId);

  RunLoopUntilIdle();
  ASSERT_TRUE(chan.is_alive());
  zx::socket sock = MakeSocketForChannel(chan);

  // Test basic channel<->socket interaction by verifying that an ACL packet
  // gets routed to the socket.
  test_device()->SendACLDataChannelPacket(StaticByteBuffer(
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

  const char write_data[81] = "🚂🚃🚄🚅🚆🚈🚇🚈🚉🚊🚋🚌🚎🚝🚞🚟🚠🚡🛤🛲";

  // Test outbound data fragments using |kMaxDataPacketLength|.
  constexpr size_t kFirstFragmentPayloadSize = kMaxDataPacketLength - sizeof(l2cap::BasicHeader);
  const StaticByteBuffer<sizeof(hci_spec::ACLDataHeader) + sizeof(l2cap::BasicHeader) +
                         kFirstFragmentPayloadSize>
      kFirstFragment(
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
  const StaticByteBuffer<sizeof(hci_spec::ACLDataHeader) + kSecondFragmentPayloadSize>
      kSecondFragment(
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

  // Run until the data is flushed out to the MockController.
  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());

  // Synchronously closes channels & sockets.
  chanmgr()->RemoveConnection(kLinkHandle);
  acl_data_channel()->UnregisterLink(kLinkHandle);
  acl_data_channel()->ClearControllerPacketCount(kLinkHandle);

  // try resending data now that connection is closed
  bytes_written = 0;
  status = sock.write(0, write_data, sizeof(write_data) - 1, &bytes_written);

  EXPECT_EQ(ZX_ERR_PEER_CLOSED, status);
  EXPECT_EQ(0u, bytes_written);

  // no packets should be sent
  RunLoopUntilIdle();
}

TEST_F(SocketFactoryL2capIntegrationTest, OutboundL2capSocket) {
  constexpr l2cap::PSM kPSM = l2cap::kAVCTP;
  constexpr l2cap::ChannelId kLocalId = 0x0040;
  constexpr l2cap::ChannelId kRemoteId = 0x9042;
  constexpr hci_spec::ConnectionHandle kLinkHandle = 0x0001;

  QueueAclConnection(kLinkHandle);
  RunLoopUntilIdle();

  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());

  l2cap::Channel::WeakPtr chan;
  auto chan_cb = [&](auto cb_chan) {
    EXPECT_EQ(kLinkHandle, cb_chan->link_handle());
    chan = std::move(cb_chan);
  };

  QueueOutboundL2capConnection(kLinkHandle, kPSM, kLocalId, kRemoteId, std::move(chan_cb));

  RunLoopUntilIdle();
  EXPECT_TRUE(test_device()->AllExpectedDataPacketsSent());
  // We should have opened a channel successfully.
  ASSERT_TRUE(chan.is_alive());
  zx::socket sock = MakeSocketForChannel(chan);

  // Test basic channel<->socket interaction by verifying that an ACL packet
  // gets routed to the socket.
  test_device()->SendACLDataChannelPacket(StaticByteBuffer(
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

  EXPECT_ACL_PACKET_OUT(test_device(), l2cap::testing::AclDisconnectionReq(
                                           NextCommandId(), kLinkHandle, kLocalId, kRemoteId));
}

}  // namespace
}  // namespace bt::socket
