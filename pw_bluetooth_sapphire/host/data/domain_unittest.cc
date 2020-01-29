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

constexpr size_t kACLSigPacketTotalHeaderSize =
    sizeof(hci::ACLDataHeader) + sizeof(l2cap::BasicHeader) + sizeof(l2cap::CommandHeader);

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

    test_device()->SetDataCallback(
        [this](const ByteBuffer& packet) {
          data_cb_count_++;
          if (data_cb_)
            data_cb_(packet);
        },
        dispatcher());
  }

  void TearDown() override {
    domain_->ShutDown();
    domain_ = nullptr;
    data_cb_ = nullptr;
    TestingBase::TearDown();
  }

  // TODO(armansito): Move this to the testing library. This should set up
  // expectations on the TestController and not just transmit.
  auto ChannelCreationDataCallback(hci::ConnectionHandle link_handle, l2cap::ChannelId remote_id,
                                   l2cap::ChannelId expected_local_cid, l2cap::PSM psm,
                                   l2cap::ChannelMode peer_mode = l2cap::ChannelMode::kBasic) {
    auto response_cb = [=](const ByteBuffer& bytes) {
      ASSERT_LE(kACLSigPacketTotalHeaderSize, bytes.size());
      EXPECT_EQ(LowerBits(link_handle), bytes[0]);
      EXPECT_EQ(UpperBits(link_handle), bytes[1]);
      l2cap::CommandCode code = bytes[sizeof(hci::ACLDataHeader) + sizeof(l2cap::BasicHeader)];
      auto id = bytes[sizeof(hci::ACLDataHeader) + sizeof(l2cap::BasicHeader) +
                      sizeof(l2cap::CommandCode)];
      const auto payload = bytes.view(kACLSigPacketTotalHeaderSize);
      switch (code) {
        case l2cap::kInformationRequest: {
          const auto info_type = static_cast<l2cap::InformationType>(
              letoh16(payload.view(0, sizeof(uint16_t)).As<uint16_t>()));
          switch (info_type) {
            case l2cap::InformationType::kExtendedFeaturesSupported: {
              test_device()->SendACLDataChannelPacket(
                  l2cap::testing::AclExtFeaturesInfoRsp(id, link_handle, kExtendedFeatures));
              return;
            }
            case l2cap::InformationType::kFixedChannelsSupported: {
              test_device()->SendACLDataChannelPacket(
                  l2cap::testing::AclFixedChannelsSupportedInfoRsp(
                      id, link_handle, l2cap::InformationResult::kSuccess,
                      l2cap::kFixedChannelsSupportedBitSignaling));
              return;
            }
            default:
              ASSERT_TRUE(false);
          }
        }
        case l2cap::kConnectionRequest: {
          ASSERT_EQ(16u, bytes.size());
          EXPECT_EQ(LowerBits(psm), bytes[12]);
          EXPECT_EQ(UpperBits(psm), bytes[13]);
          EXPECT_EQ(LowerBits(expected_local_cid), bytes[14]);
          EXPECT_EQ(UpperBits(expected_local_cid), bytes[15]);
          test_device()->SendACLDataChannelPacket(
              l2cap::testing::AclConnectionRsp(id, link_handle, expected_local_cid, remote_id));
          return;
        }
        case l2cap::kConfigurationRequest: {
          ASSERT_LE(16u, bytes.size());
          // Just blindly accept any configuration request as long as it for our
          // cid
          EXPECT_EQ(LowerBits(remote_id), bytes[12]);
          EXPECT_EQ(UpperBits(remote_id), bytes[13]);

          // Respond to the given request, and make your own request.
          test_device()->SendACLDataChannelPacket(
              l2cap::testing::AclConfigRsp(id, link_handle, expected_local_cid));

          constexpr l2cap::CommandId kConfigReqId = 6;
          test_device()->SendACLDataChannelPacket(l2cap::testing::AclConfigReq(
              kConfigReqId, link_handle, expected_local_cid, l2cap::kDefaultMTU, peer_mode));
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
          return;
      };
    };

    return response_cb;
  }

  Domain* domain() const { return domain_.get(); }

  void set_data_cb(TestController::DataCallback cb) {
    data_cb_ = std::move(cb);
    data_cb_count_ = 0;
  }
  void clear_data_cb() {
    data_cb_ = nullptr;
    data_cb_count_ = 0;
  }

  size_t data_cb_count() const { return data_cb_count_; }

 private:
  fbl::RefPtr<Domain> domain_;

  TestController::DataCallback data_cb_;
  size_t data_cb_count_;

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
  auto sock_cb = [&](auto chan_sock, hci::ConnectionHandle handle) {
    EXPECT_EQ(kLinkHandle, handle);
    sock = std::move(chan_sock.socket);
  };

  domain()->RegisterService(kPSM, kChannelParameters, std::move(sock_cb), dispatcher());
  RunLoopUntilIdle();

  set_data_cb(ChannelCreationDataCallback(kLinkHandle, kRemoteId, kLocalId, kPSM));
  test_device()->SendACLDataChannelPacket(
      l2cap::testing::AclConnectionReq(1, kLinkHandle, kRemoteId, kPSM));

  RunLoopUntilIdle();
  ASSERT_TRUE(sock);
  clear_data_cb();

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

  auto rx_cb = [this, &kFirstFragment, &kSecondFragment](const ByteBuffer& packet) {
    if (data_cb_count() == 1) {
      EXPECT_TRUE(ContainersEqual(kFirstFragment, packet));
    } else if (data_cb_count() == 2) {
      EXPECT_TRUE(ContainersEqual(kSecondFragment, packet));
    }
  };
  set_data_cb(rx_cb);

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

  // The 80-byte write should be fragmented over 64- and 20-byte HCI payloads in order to send it
  // to the controller.
  EXPECT_EQ(2u, data_cb_count());
  clear_data_cb();

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
  EXPECT_EQ(0u, data_cb_count());
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

  set_data_cb(response_cb);

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
  auto sock_cb = [&](auto chan_sock, hci::ConnectionHandle handle) {
    EXPECT_EQ(kLinkHandle, handle);
    sock = std::move(chan_sock.socket);
  };

  domain()->RegisterService(kPSM, kChannelParameters, std::move(sock_cb), dispatcher());
  RunLoopUntilIdle();

  constexpr l2cap::CommandId kConnectionReqId = 1;
  constexpr l2cap::CommandId kConfigReqId = 6;
  constexpr l2cap::CommandId kConfigRspId = 3;
  test_device()->SendACLDataChannelPacket(
      l2cap::testing::AclConnectionReq(kConnectionReqId, kLinkHandle, kRemoteId, kPSM));
  RunLoopUntilIdle();
  test_device()->SendACLDataChannelPacket(l2cap::testing::AclConfigReq(
      kConfigReqId, kLinkHandle, kLocalId, l2cap::kDefaultMTU, l2cap::ChannelMode::kBasic));
  test_device()->SendACLDataChannelPacket(
      l2cap::testing::AclConfigRsp(kConfigRspId, kLinkHandle, kLocalId));

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

  // Register a fake link.
  acl_data_channel()->RegisterLink(kLinkHandle, hci::Connection::LinkType::kACL);
  domain()->AddACLConnection(kLinkHandle, hci::Connection::Role::kMaster, DoNothing,
                             NopSecurityCallback, dispatcher());
  RunLoopUntilIdle();

  zx::socket sock;
  ASSERT_FALSE(sock);
  auto sock_cb = [&](auto chan_sock, hci::ConnectionHandle handle) {
    EXPECT_EQ(kLinkHandle, handle);
    sock = std::move(chan_sock.socket);
  };

  domain()->OpenL2capChannel(kLinkHandle, kPSM, kChannelParameters, std::move(sock_cb),
                             dispatcher());

  // Expect the CHANNEL opening stuff here
  set_data_cb(ChannelCreationDataCallback(kLinkHandle, kRemoteId, kLocalId, kPSM));

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

  // info req x2, connection request/response, config request, config response
  constexpr size_t kChannelCreationPacketCount = 3;

  // Register a fake link.
  acl_data_channel()->RegisterLink(kLinkHandle, hci::Connection::LinkType::kACL);
  domain()->AddACLConnection(kLinkHandle, hci::Connection::Role::kMaster, DoNothing,
                             NopSecurityCallback, dispatcher());

  zx::socket sock0;
  ASSERT_FALSE(sock0);
  auto sock_cb0 = [&](auto chan_sock, hci::ConnectionHandle handle) {
    EXPECT_EQ(kLinkHandle, handle);
    sock0 = std::move(chan_sock.socket);
  };
  domain()->RegisterService(kPSM0, kChannelParameters, sock_cb0, dispatcher());

  set_data_cb(ChannelCreationDataCallback(kLinkHandle, kRemoteId0, kLocalId0, kPSM0));
  test_device()->SendACLDataChannelPacket(
      l2cap::testing::AclConnectionReq(1, kLinkHandle, kRemoteId0, kPSM0));

  RunLoopUntilIdle();
  ASSERT_TRUE(sock0);

  // Channel creation packet count
  EXPECT_EQ(kConnectionCreationPacketCount + kChannelCreationPacketCount, data_cb_count());
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

  set_data_cb(
      [&kPacket0](const ByteBuffer& packet) { EXPECT_TRUE(ContainersEqual(kPacket0, packet)); });

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
  EXPECT_EQ(kMaxPacketCount, data_cb_count());

  zx::socket sock1;
  ASSERT_FALSE(sock1);
  auto sock_cb1 = [&](auto chan_sock, hci::ConnectionHandle handle) {
    EXPECT_EQ(kLinkHandle, handle);
    sock1 = std::move(chan_sock.socket);
  };

  domain()->OpenL2capChannel(kLinkHandle, kPSM1, kChannelParameters, std::move(sock_cb1),
                             dispatcher());
  set_data_cb(ChannelCreationDataCallback(kLinkHandle, kRemoteId1, kLocalId1, kPSM1));

  for (size_t i = 0; i < kChannelCreationPacketCount; i++) {
    test_device()->SendCommandChannelPacket(
        testing::NumberOfCompletedPacketsPacket(kLinkHandle, 1));
    // Wait for next connection creation packet to be queued (eg. configuration request/response).
    RunLoopUntilIdle();
  }

  EXPECT_TRUE(sock1);
  EXPECT_EQ(kChannelCreationPacketCount, data_cb_count());

  set_data_cb(
      [&kPacket0](const ByteBuffer& packet) { EXPECT_TRUE(ContainersEqual(kPacket0, packet)); });

  // Make room in buffer for queued dynamic channel packet.
  test_device()->SendCommandChannelPacket(testing::NumberOfCompletedPacketsPacket(kLinkHandle, 1));

  RunLoopUntilIdle();
  // 1 Queued dynamic channel data packet should have been sent.
  EXPECT_EQ(1u, data_cb_count());
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

  set_data_cb(
      ChannelCreationDataCallback(kLinkHandle, kRemoteId, kLocalId, kPSM, *chan_params.mode));

  // Register a fake link.
  acl_data_channel()->RegisterLink(kLinkHandle, hci::Connection::LinkType::kACL);
  domain()->AddACLConnection(kLinkHandle, hci::Connection::Role::kMaster, DoNothing,
                             NopSecurityCallback, dispatcher());

  RunLoopUntilIdle();
  EXPECT_EQ(kConnectionCreationPacketCount, data_cb_count());

  fbl::RefPtr<l2cap::Channel> chan;
  auto chan_cb = [&](fbl::RefPtr<l2cap::Channel> cb_chan) { chan = std::move(cb_chan); };

  domain()->OpenL2capChannel(kLinkHandle, kPSM, chan_params, chan_cb, dispatcher());

  RunLoopUntilIdle();
  EXPECT_TRUE(chan);
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

  set_data_cb(
      ChannelCreationDataCallback(kLinkHandle, kRemoteId, kLocalId, kPSM, *chan_params.mode));

  // Register a fake link.
  acl_data_channel()->RegisterLink(kLinkHandle, hci::Connection::LinkType::kACL);
  domain()->AddACLConnection(kLinkHandle, hci::Connection::Role::kMaster, DoNothing,
                             NopSecurityCallback, dispatcher());

  RunLoopUntilIdle();
  EXPECT_EQ(kConnectionCreationPacketCount, data_cb_count());

  std::optional<l2cap::ChannelInfo> chan_info;
  auto sock_cb = [&](auto chan_sock, auto /*handle*/) {
    EXPECT_TRUE(chan_sock);
    chan_info = chan_sock.params;
  };
  domain()->RegisterService(kPSM, chan_params, sock_cb, dispatcher());

  constexpr l2cap::CommandId kConnReqId = 1;
  test_device()->SendACLDataChannelPacket(
      l2cap::testing::AclConnectionReq(kConnReqId, kLinkHandle, kRemoteId, kPSM));

  RunLoopUntilIdle();
  ASSERT_TRUE(chan_info);
  EXPECT_EQ(*chan_params.max_rx_sdu_size, chan_info->max_rx_sdu_size);
  EXPECT_EQ(*chan_params.mode, chan_info->mode);
}

}  // namespace
}  // namespace data
}  // namespace bt
