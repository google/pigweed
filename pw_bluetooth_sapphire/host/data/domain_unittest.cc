// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/data/domain.h"

#include <fbl/macros.h>
#include <lib/async/cpp/task.h>

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

class DATA_DomainTest : public TestingBase {
 public:
  DATA_DomainTest() = default;
  ~DATA_DomainTest() override = default;

 protected:
  void SetUp() override {
    TestingBase::SetUp();
    InitializeACLDataChannel();

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

        // Configuration Response (ID: 1, length: 6, src cid: |dst_id|, flags: 0,
        // result: success)
        0x05, 0x01, 0x06, 0x00,
        LowerBits(dst_id), UpperBits(dst_id), 0x00, 0x00,
        0x00, 0x00));
    // clang-format on

    RunLoopUntilIdle();
  }

  void ExpectOutgoingChannelCreation(hci::ConnectionHandle link_handle, l2cap::ChannelId remote_id,
                                     l2cap::ChannelId expected_local_cid, l2cap::PSM psm) {
    auto response_cb = [=](const auto& bytes) {
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
    test_device()->SetDataCallback(response_cb, dispatcher());
  }

  Domain* domain() const { return domain_.get(); }

 private:
  fbl::RefPtr<Domain> domain_;

  DISALLOW_COPY_ASSIGN_AND_MOVE(DATA_DomainTest);
};

TEST_F(DATA_DomainTest, InboundL2capSocket) {
  constexpr l2cap::PSM kPSM = l2cap::kAVDTP;
  constexpr l2cap::ChannelId kLocalId = 0x0040;
  constexpr l2cap::ChannelId kRemoteId = 0x9042;
  constexpr hci::ConnectionHandle kLinkHandle = 0x0001;

  // Register a fake link.
  domain()->AddACLConnection(
      kLinkHandle, hci::Connection::Role::kMaster, [] {}, [](auto, auto, auto) {}, dispatcher());

  zx::socket sock;
  ASSERT_FALSE(sock);
  auto sock_cb = [&](zx::socket cb_sock, hci::ConnectionHandle handle) {
    EXPECT_EQ(kLinkHandle, handle);
    sock = std::move(cb_sock);
  };

  domain()->RegisterService(kPSM, std::move(sock_cb), dispatcher());
  RunLoopUntilIdle();

  EmulateIncomingChannelCreation(kLinkHandle, kRemoteId, kLocalId, kPSM);
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

TEST_F(DATA_DomainTest, OutboundL2apSocket) {
  constexpr l2cap::PSM kPSM = l2cap::kAVCTP;
  constexpr l2cap::ChannelId kLocalId = 0x0040;
  constexpr l2cap::ChannelId kRemoteId = 0x9042;
  constexpr hci::ConnectionHandle kLinkHandle = 0x0001;

  // Register a fake link.
  domain()->AddACLConnection(
      kLinkHandle, hci::Connection::Role::kMaster, [] {}, [](auto, auto, auto) {}, dispatcher());

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

// TODO(armansito): Add unit tests for RFCOMM sockets when the Domain class
// has a public API for it.

}  // namespace
}  // namespace data
}  // namespace bt
