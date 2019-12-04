// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/data/domain.h"

#include <lib/async/cpp/task.h>

#include <fbl/macros.h>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_controller.h"

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

auto MakeNumCompletedPacketsEvent(hci::ConnectionHandle handle, uint16_t num_packets) {
  return CreateStaticByteBuffer(
      0x13, 0x05,  // Number Of Completed Packet HCI event header, parameters length
      0x01,        // Number of handles
      LowerBits(handle), UpperBits(handle), LowerBits(num_packets), UpperBits(num_packets));
}

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
  }

  void TearDown() override {
    domain_->ShutDown();
    domain_ = nullptr;
    TestingBase::TearDown();
  }

  // TODO(armansito): Move this to the testing library. This should set up
  // expectations on the TestController and not just transmit.
  void EmulateIncomingChannelCreation(hci::ConnectionHandle link_handle, l2cap::ChannelId src_id,
                                      l2cap::ChannelId dst_id, l2cap::PSM psm) {
    // clang-format off
    test_device()->SendACLDataChannelPacket(CreateStaticByteBuffer(
        // ACL data header (handle: |link_handle|, length: 12 bytes)
        LowerBits(link_handle), UpperBits(link_handle), 0x0c, 0x00,

        // L2CAP B-frame header (length: 8 bytes, channel-id: 0x0001 (ACL sig))
        0x08, 0x00, 0x01, 0x00,

        // Connection Request (ID: 1, length: 4, |psm|, |src_id|)
        0x02, 0x01, 0x04, 0x00,
        LowerBits(psm), UpperBits(psm), LowerBits(src_id), UpperBits(src_id)));

    RunLoopUntilIdle();

    test_device()->SendACLDataChannelPacket(CreateStaticByteBuffer(
        // ACL data header (handle: |link_handle|, length: 16 bytes)
        LowerBits(link_handle), UpperBits(link_handle), 0x10, 0x00,

        // L2CAP B-frame header (length: 12 bytes, channel-id: 0x0001 (ACL sig))
        0x0c, 0x00, 0x01, 0x00,

        // Configuration Request (ID: 6, length: 8, |dst_id|, flags: 0,
        // options: [type: MTU, length: 2, MTU: 1024])
        0x04, 0x06, 0x08, 0x00,
        LowerBits(dst_id), UpperBits(dst_id), 0x00, 0x00,
        0x01, 0x02, 0x00, 0x04));

    test_device()->SendACLDataChannelPacket(CreateStaticByteBuffer(
        // ACL data header (handle: |link_handle|, length: 14 bytes)
        LowerBits(link_handle), UpperBits(link_handle), 0x0e, 0x00,

        // L2CAP B-frame header (length: 10 bytes, channel-id: 0x0001 (ACL sig))
        0x0a, 0x00, 0x01, 0x00,

        // Configuration Response (ID: 2, length: 6, src cid: |dst_id|, flags: 0,
        // result: success)
        0x05, 0x02, 0x06, 0x00,
        LowerBits(dst_id), UpperBits(dst_id), 0x00, 0x00,
        0x00, 0x00));
    // clang-format on
  }

  auto OutgoingChannelCreationDataCallback(hci::ConnectionHandle link_handle,
                                           l2cap::ChannelId remote_id,
                                           l2cap::ChannelId expected_local_cid, l2cap::PSM psm) {
    auto response_cb = [=](const ByteBuffer& bytes) {
      ASSERT_LE(10u, bytes.size());
      EXPECT_EQ(LowerBits(link_handle), bytes[0]);
      EXPECT_EQ(UpperBits(link_handle), bytes[1]);
      l2cap::CommandCode code = bytes[8];
      auto id = bytes[9];
      switch (code) {
        case l2cap::kConnectionRequest: {
          ASSERT_EQ(16u, bytes.size());
          EXPECT_EQ(LowerBits(psm), bytes[12]);
          EXPECT_EQ(UpperBits(psm), bytes[13]);
          EXPECT_EQ(LowerBits(expected_local_cid), bytes[14]);
          EXPECT_EQ(UpperBits(expected_local_cid), bytes[15]);
          test_device()->SendACLDataChannelPacket(CreateStaticByteBuffer(
              // ACL data header (handle: |link handle|, length: 16 bytes)
              LowerBits(link_handle), UpperBits(link_handle), 0x10, 0x00,
              // L2CAP B-frame header: length 12, channel-id 1 (signaling)
              0x0c, 0x00, 0x01, 0x00,
              // Connection Response (0x03) id: (matches request), length 8
              l2cap::kConnectionResponse, id, 0x08, 0x00,
              // destination cid |remote_id|
              LowerBits(remote_id), UpperBits(remote_id),
              // source cid (same as request)
              bytes[14], bytes[15],
              // Result (success), status
              0x00, 0x00, 0x00, 0x00));
          return;
        }
        case l2cap::kConfigurationRequest: {
          ASSERT_LE(16u, bytes.size());
          // Just blindly accept any configuration request as long as it for our
          // cid
          EXPECT_EQ(LowerBits(remote_id), bytes[12]);
          EXPECT_EQ(UpperBits(remote_id), bytes[13]);
          // Respond to the given request, and make your own request.
          test_device()->SendACLDataChannelPacket(CreateStaticByteBuffer(
              // ACL header: handle |link_handle|, length: 18
              LowerBits(link_handle), UpperBits(link_handle), 0x12, 0x00,
              // L2CAP B-frame header: length 14, channel-id 1 (signaling)
              0x0e, 0x00, 0x01, 0x00,
              // Configuration Response (0x04), id: (match request) length 10
              l2cap::kConfigurationResponse, id, 0x0a, 0x00,
              // Source CID, Flags (same as req)
              LowerBits(expected_local_cid), UpperBits(expected_local_cid), bytes[14], bytes[15],
              // Result (success)
              0x00, 0x00,
              // Config option: MTU, length 2, MTU 1024
              0x01, 0x02, 0x00, 0x04));
          test_device()->SendACLDataChannelPacket(CreateStaticByteBuffer(
              // ACL data header (handle: |link_handle|, length: 16 bytes)
              LowerBits(link_handle), UpperBits(link_handle), 0x10, 0x00,

              // L2CAP B-frame header (length: 12 bytes, channel-id: 0x0001 (ACL
              // sig))
              0x0c, 0x00, 0x01, 0x00,

              // Configuration Request (ID: 6, length: 8, |dst_id|, flags: 0,
              // options: [type: MTU, length: 2, MTU: 1024])
              l2cap::kConfigurationRequest, 0x06, 0x08, 0x00, LowerBits(expected_local_cid),
              UpperBits(expected_local_cid), 0x00, 0x00, 0x01, 0x02, 0x00, 0x04));
          return;
        }
        case l2cap::kConfigurationResponse: {
          ASSERT_LE(16u, bytes.size());
          // Should indicate our cid
          EXPECT_EQ(LowerBits(remote_id), bytes[12]);
          EXPECT_EQ(UpperBits(remote_id), bytes[13]);
          return;
        }
        default:
          ASSERT_TRUE(false);
          return;
      };
    };

    return response_cb;
  }

  void ExpectOutgoingChannelCreation(hci::ConnectionHandle link_handle, l2cap::ChannelId remote_id,
                                     l2cap::ChannelId expected_local_cid, l2cap::PSM psm) {
    auto response_cb =
        OutgoingChannelCreationDataCallback(link_handle, remote_id, expected_local_cid, psm);
    test_device()->SetDataCallback(response_cb, dispatcher());
  }

  Domain* domain() const { return domain_.get(); }

 private:
  fbl::RefPtr<Domain> domain_;

  DISALLOW_COPY_ASSIGN_AND_MOVE(DATA_DomainTest);
};

void DoNothing() {}
void NopSecurityCallback(hci::ConnectionHandle, sm::SecurityLevel, sm::StatusCallback) {}

TEST_F(DATA_DomainTest, InboundL2capSocket) {
  constexpr l2cap::PSM kPSM = l2cap::kAVDTP;
  constexpr l2cap::ChannelId kLocalId = 0x0040;
  constexpr l2cap::ChannelId kRemoteId = 0x9042;
  constexpr hci::ConnectionHandle kLinkHandle = 0x0001;

  // Register a fake link.
  acl_data_channel()->RegisterLink(kLinkHandle, hci::Connection::LinkType::kACL);
  domain()->AddACLConnection(kLinkHandle, hci::Connection::Role::kMaster, DoNothing,
                             NopSecurityCallback, dispatcher());

  zx::socket sock;
  ASSERT_FALSE(sock);
  auto sock_cb = [&](zx::socket cb_sock, hci::ConnectionHandle handle) {
    EXPECT_EQ(kLinkHandle, handle);
    sock = std::move(cb_sock);
  };

  domain()->RegisterService(kPSM, std::move(sock_cb), dispatcher());
  RunLoopUntilIdle();

  EmulateIncomingChannelCreation(kLinkHandle, kRemoteId, kLocalId, kPSM);
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

  // Test outbound data fragments using |kMaxDataPacketLength|.
  const auto kFirstFragment = CreateStaticByteBuffer(
      // ACL data header (handle: 1, length 64)
      0x01, 0x00, 0x40, 0x00,

      // L2CAP B-frame: (length: 80, channel-id: 0x9042 (kRemoteId))
      0x50, 0x00, 0x42, 0x90,

      // L2CAP payload (fragmented)
      0xf0, 0x9f, 0x9a, 0x82, 0xf0, 0x9f, 0x9a, 0x83, 0xf0, 0x9f, 0x9a, 0x84, 0xf0, 0x9f, 0x9a,
      0x85, 0xf0, 0x9f, 0x9a, 0x86, 0xf0, 0x9f, 0x9a, 0x88, 0xf0, 0x9f, 0x9a, 0x87, 0xf0, 0x9f,
      0x9a, 0x88, 0xf0, 0x9f, 0x9a, 0x89, 0xf0, 0x9f, 0x9a, 0x8a, 0xf0, 0x9f, 0x9a, 0x8b, 0xf0,
      0x9f, 0x9a, 0x8c, 0xf0, 0x9f, 0x9a, 0x8e, 0xf0, 0x9f, 0x9a, 0x9d, 0xf0, 0x9f, 0x9a, 0x9e);

  const auto kSecondFragment = CreateStaticByteBuffer(
      // ACL data header (handle: 1, pbf: continuing fr., length: 20)
      0x01, 0x10, 0x14, 0x00,

      // L2CAP payload (final fragment)
      0xf0, 0x9f, 0x9a, 0x9f, 0xf0, 0x9f, 0x9a, 0xa0, 0xf0, 0x9f, 0x9a, 0xa1, 0xf0, 0x9f, 0x9b,
      0xa4, 0xf0, 0x9f, 0x9b, 0xb2);

  int rx_count = 0;
  auto rx_cb = [&rx_count, &kFirstFragment, &kSecondFragment](const ByteBuffer& packet) {
    rx_count++;
    if (rx_count == 1) {
      EXPECT_TRUE(ContainersEqual(kFirstFragment, packet));
    } else if (rx_count == 2) {
      EXPECT_TRUE(ContainersEqual(kSecondFragment, packet));
    }
  };
  test_device()->SetDataCallback(rx_cb, dispatcher());

  // Write some outbound bytes to the socket buffer.
  // clang-format off
  const char write_data[] = u8"🚂🚃🚄🚅🚆🚈🚇🚈🚉🚊🚋🚌🚎🚝🚞🚟🚠🚡🛤🛲";
  // clang-format on
  size_t bytes_written = 0;
  status = sock.write(0, write_data, sizeof(write_data) - 1, &bytes_written);
  EXPECT_EQ(ZX_OK, status);
  EXPECT_EQ(80u, bytes_written);

  // Run until the data is flushed out to the TestController.
  RunLoopUntilIdle();

  // The 80-byte write should be fragmented over 64- and 20-byte HCI payloads in order to send it to
  // the controller.
  EXPECT_EQ(2, rx_count);
  rx_count = 0;

  domain()->RemoveConnection(kLinkHandle);
  acl_data_channel()->UnregisterLink(kLinkHandle);
  acl_data_channel()->ClearControllerPacketCount(kLinkHandle);

  // try resending data now that connection is closed
  bytes_written = 0;
  status = sock.write(0, write_data, sizeof(write_data) - 1, &bytes_written);

  EXPECT_EQ(ZX_OK, status);
  EXPECT_EQ(80u, bytes_written);

  RunLoopUntilIdle();

  // no packets should have been received
  EXPECT_EQ(0, rx_count);
}

TEST_F(DATA_DomainTest, InboundRfcommSocketFails) {
  constexpr l2cap::PSM kPSM = l2cap::kRFCOMM;
  // constexpr l2cap::ChannelId kLocalId = 0x0040;
  constexpr l2cap::ChannelId kRemoteId = 0x9042;
  constexpr hci::ConnectionHandle kLinkHandle = 0x0001;

  // Register a fake link.
  acl_data_channel()->RegisterLink(kLinkHandle, hci::Connection::LinkType::kACL);
  domain()->AddACLConnection(kLinkHandle, hci::Connection::Role::kMaster, DoNothing,
                             NopSecurityCallback, dispatcher());

  RunLoopUntilIdle();

  bool responded = false;
  auto response_cb = [=, &responded](const auto& bytes) {
    responded = true;
    auto kConnectionRejectedResponse = CreateStaticByteBuffer(
        // ACL Data header (handle, length = 16)
        LowerBits(kLinkHandle), UpperBits(kLinkHandle), 0x10, 0x00,
        // L2CAP b-frame header (length = 12, cid)
        0x0c, 0x00, 0x01, 0x00,
        // Connnection Response, ID: 1, length: 8,
        0x03, 0x01, 0x08, 0x00,
        // |dest cid|, |source cid|
        0x00, 0x00, LowerBits(kRemoteId), UpperBits(kRemoteId),
        // Result: refused|, |status|
        0x02, 0x00, 0x00, 0x00);
    ASSERT_TRUE(ContainersEqual(kConnectionRejectedResponse, bytes));
  };

  test_device()->SetDataCallback(response_cb, dispatcher());

  // clang-format off
  test_device()->SendACLDataChannelPacket(CreateStaticByteBuffer(
      // ACL data header (handle: |link_handle|, length: 12 bytes)
      LowerBits(kLinkHandle), UpperBits(kLinkHandle), 0x0c, 0x00,

      // L2CAP B-frame header (length: 8 bytes, channel-id: 0x0001 (ACL sig))
      0x08, 0x00, 0x01, 0x00,

      // Connection Request (ID: 1, length: 4, |psm|, |src_id|)
      0x02, 0x01, 0x04, 0x00,
      LowerBits(kPSM), UpperBits(kPSM), LowerBits(kRemoteId), UpperBits(kRemoteId)));
  // clang-format on

  RunLoopUntilIdle();
  // Incoming connection refused, RFCOMM is not routed.
  ASSERT_TRUE(responded);
}

TEST_F(DATA_DomainTest, InboundPacketQueuedAfterChannelOpenIsNotDropped) {
  constexpr l2cap::PSM kPSM = l2cap::kSDP;
  constexpr l2cap::ChannelId kLocalId = 0x0040;
  constexpr l2cap::ChannelId kRemoteId = 0x9042;
  constexpr hci::ConnectionHandle kLinkHandle = 0x0001;

  // Register a fake link.
  acl_data_channel()->RegisterLink(kLinkHandle, hci::Connection::LinkType::kACL);
  domain()->AddACLConnection(kLinkHandle, hci::Connection::Role::kMaster, DoNothing,
                             NopSecurityCallback, dispatcher());

  zx::socket sock;
  ASSERT_FALSE(sock);
  auto sock_cb = [&](zx::socket cb_sock, hci::ConnectionHandle handle) {
    EXPECT_EQ(kLinkHandle, handle);
    sock = std::move(cb_sock);
  };

  domain()->RegisterService(kPSM, std::move(sock_cb), dispatcher());
  RunLoopUntilIdle();

  EmulateIncomingChannelCreation(kLinkHandle, kRemoteId, kLocalId, kPSM);

  // Queue up a data packet for the new channel before the channel configuration has been processed.
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

  // Register a fake link.
  acl_data_channel()->RegisterLink(kLinkHandle, hci::Connection::LinkType::kACL);
  domain()->AddACLConnection(kLinkHandle, hci::Connection::Role::kMaster, DoNothing,
                             NopSecurityCallback, dispatcher());
  RunLoopUntilIdle();

  zx::socket sock;
  ASSERT_FALSE(sock);
  auto sock_cb = [&](zx::socket cb_sock, hci::ConnectionHandle handle) {
    EXPECT_EQ(kLinkHandle, handle);
    sock = std::move(cb_sock);
  };

  domain()->OpenL2capChannel(kLinkHandle, kPSM, std::move(sock_cb), dispatcher());

  // Expect the CHANNEL opening stuff here
  ExpectOutgoingChannelCreation(kLinkHandle, kRemoteId, kLocalId, kPSM);

  RunLoopUntilIdle();
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
  auto sock_cb = [&sock_cb_called, kLinkHandle](zx::socket cb_sock, hci::ConnectionHandle handle) {
    sock_cb_called = true;
    EXPECT_FALSE(cb_sock);
    EXPECT_EQ(kLinkHandle, handle);
  };

  domain()->OpenL2capChannel(kLinkHandle, kPSM, std::move(sock_cb), dispatcher());

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

  // info req, connection request/response, config request, config response
  constexpr size_t kConnectionCreationPacketCount = 1;
  constexpr size_t kChannelCreationPacketCount = 3;

  TestController::DataCallback data_cb = [](const ByteBuffer& packet) {};
  size_t data_cb_count = 0;
  auto data_cb_wrapper = [&data_cb, &data_cb_count](const ByteBuffer& packet) {
    data_cb_count++;
    data_cb(packet);
  };
  test_device()->SetDataCallback(data_cb_wrapper, dispatcher());

  // Register a fake link.
  acl_data_channel()->RegisterLink(kLinkHandle, hci::Connection::LinkType::kACL);
  domain()->AddACLConnection(kLinkHandle, hci::Connection::Role::kMaster, DoNothing,
                             NopSecurityCallback, dispatcher());

  zx::socket sock0;
  ASSERT_FALSE(sock0);
  auto sock_cb0 = [&](zx::socket cb_sock, hci::ConnectionHandle handle) {
    EXPECT_EQ(kLinkHandle, handle);
    sock0 = std::move(cb_sock);
  };
  domain()->RegisterService(kPSM0, sock_cb0, dispatcher());

  EmulateIncomingChannelCreation(kLinkHandle, kRemoteId0, kLocalId0, kPSM0);
  RunLoopUntilIdle();
  ASSERT_TRUE(sock0);

  // Channel creation packet count
  EXPECT_EQ(kConnectionCreationPacketCount + kChannelCreationPacketCount, data_cb_count);
  test_device()->SendCommandChannelPacket(MakeNumCompletedPacketsEvent(
      kLinkHandle, kConnectionCreationPacketCount + kChannelCreationPacketCount));

  // Dummy dynamic channel packet
  const auto kPacket0 = CreateStaticByteBuffer(
      // ACL data header (handle: 1, length 5)
      0x01, 0x00, 0x05, 0x00,

      // L2CAP B-frame: (length: 1, channel-id: 0x9042 (kRemoteId))
      0x01, 0x00, 0x42, 0x90,

      // L2CAP payload
      0x01);

  data_cb_count = 0;
  data_cb = [&kPacket0](const ByteBuffer& packet) {
    EXPECT_TRUE(ContainersEqual(kPacket0, packet));
  };

  // |kMaxPacketCount| packets should be sent to the controller,
  // and 1 packet should be left in the queue
  const char write_data[] = {0x01};
  for (size_t i = 0; i < kMaxPacketCount + 1; i++) {
    size_t bytes_written = 0;
    zx_status_t status = sock0.write(0, write_data, sizeof(write_data), &bytes_written);
    EXPECT_EQ(ZX_OK, status);
    EXPECT_EQ(sizeof(write_data), bytes_written);
  }

  // Run until the data is flushed out to the TestController.
  RunLoopUntilIdle();
  EXPECT_EQ(kMaxPacketCount, data_cb_count);

  zx::socket sock1;
  ASSERT_FALSE(sock1);
  auto sock_cb1 = [&](zx::socket cb_sock, hci::ConnectionHandle handle) {
    EXPECT_EQ(kLinkHandle, handle);
    sock1 = std::move(cb_sock);
  };

  domain()->OpenL2capChannel(kLinkHandle, kPSM1, std::move(sock_cb1), dispatcher());
  data_cb_count = 0;
  data_cb = OutgoingChannelCreationDataCallback(kLinkHandle, kRemoteId1, kLocalId1, kPSM1);

  for (size_t i = 0; i < kChannelCreationPacketCount; i++) {
    test_device()->SendCommandChannelPacket(MakeNumCompletedPacketsEvent(kLinkHandle, 1));
    // Wait for next connection creation packet to be queued (eg. configuration request/response).
    RunLoopUntilIdle();
  }

  EXPECT_TRUE(sock1);
  EXPECT_EQ(kChannelCreationPacketCount, data_cb_count);

  data_cb_count = 0;
  data_cb = [&kPacket0](const ByteBuffer& packet) {
    EXPECT_TRUE(ContainersEqual(kPacket0, packet));
  };

  // Make room in buffer for queued dynamic channel packet.
  test_device()->SendCommandChannelPacket(MakeNumCompletedPacketsEvent(kLinkHandle, 1));

  RunLoopUntilIdle();
  // 1 Queued dynamic channel data packet should have been sent.
  EXPECT_EQ(1u, data_cb_count);
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

}  // namespace
}  // namespace data
}  // namespace bt
