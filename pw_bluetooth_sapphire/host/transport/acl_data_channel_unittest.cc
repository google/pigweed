// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/transport/acl_data_channel.h"

#include <lib/async/cpp/task.h>

#include <unordered_map>

#include <gmock/gmock.h>

#include "src/connectivity/bluetooth/core/bt-host/common/assert.h"
#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/hci-spec/defaults.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap_defs.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/inspect.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/inspect_util.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/mock_controller.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_packets.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/link_type.h"
#include "src/connectivity/bluetooth/core/bt-host/transport/transport.h"

namespace bt::hci {
namespace {

constexpr hci_spec::ConnectionHandle kLinkHandle = 0x0001;

using TestingBase = bt::testing::ControllerTest<bt::testing::MockController>;

class ACLDataChannelTest : public TestingBase {
 public:
  ACLDataChannelTest() = default;
  ~ACLDataChannelTest() override = default;

 protected:
  // TestBase overrides:
  void SetUp() override {
    TestingBase::SetUp();
    StartTestDevice();
  }

 private:
  BT_DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ACLDataChannelTest);
};

using HCI_ACLDataChannelTest = ACLDataChannelTest;

TEST_F(ACLDataChannelTest, VerifyMTUs) {
  const DataBufferInfo kBREDRBufferInfo(1024, 50);
  const DataBufferInfo kLEBufferInfo(64, 16);

  // BR/EDR buffer only.
  InitializeACLDataChannel(kBREDRBufferInfo, DataBufferInfo());
  EXPECT_EQ(kBREDRBufferInfo, acl_data_channel()->GetBufferInfo());
  EXPECT_EQ(kBREDRBufferInfo, acl_data_channel()->GetLeBufferInfo());

  TearDown();
  SetUp();

  // LE buffer only.
  InitializeACLDataChannel(DataBufferInfo(), kLEBufferInfo);
  EXPECT_EQ(DataBufferInfo(), acl_data_channel()->GetBufferInfo());
  EXPECT_EQ(kLEBufferInfo, acl_data_channel()->GetLeBufferInfo());

  TearDown();
  SetUp();

  // Both buffers available.
  InitializeACLDataChannel(kBREDRBufferInfo, kLEBufferInfo);
  EXPECT_EQ(kBREDRBufferInfo, acl_data_channel()->GetBufferInfo());
  EXPECT_EQ(kLEBufferInfo, acl_data_channel()->GetLeBufferInfo());
}

// Test that SendPacket works using only the BR/EDR buffer.
TEST_F(ACLDataChannelTest, SendPacketBREDRBuffer) {
  constexpr size_t kMaxMTU = 5;
  constexpr size_t kMaxNumPackets = 5;
  constexpr hci_spec::ConnectionHandle kHandle0 = 0x0001;
  constexpr hci_spec::ConnectionHandle kHandle1 = 0x0002;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle0, bt::LinkType::kLE);
  acl_data_channel()->RegisterLink(kHandle1, bt::LinkType::kACL);

  int handle0_packet_count = 0;
  int handle1_packet_count = 0;

  // Callback invoked by TestDevice when it receive a data packet from us.
  auto data_callback = [&](const ByteBuffer& bytes) {
    BT_DEBUG_ASSERT(bytes.size() >= sizeof(hci_spec::ACLDataHeader));

    PacketView<hci_spec::ACLDataHeader> packet(&bytes,
                                               bytes.size() - sizeof(hci_spec::ACLDataHeader));
    hci_spec::ConnectionHandle connection_handle =
        le16toh(packet.header().handle_and_flags) & 0xFFF;

    if (connection_handle == kHandle0) {
      handle0_packet_count++;
    } else {
      ASSERT_EQ(kHandle1, connection_handle);
      handle1_packet_count++;
    }
  };
  test_device()->SetDataCallback(data_callback, dispatcher());

  // Queue up 11 packets in total, distributed among the two connection handles.
  // The first 5 packets should be sent immediately, and the next 6 should be queued.
  async::PostTask(dispatcher(), [this] {
    for (int i = 0; i < 11; ++i) {
      hci_spec::ConnectionHandle handle = (i % 2) ? kHandle1 : kHandle0;
      auto packet = ACLDataPacket::New(handle, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                       hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMTU);
      EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kInvalidChannelId,
                                                 AclDataChannel::PacketPriority::kLow));
    }
  });

  RunLoopUntilIdle();

  // kMaxNumPackets is 5. The controller should have received 3 packets on
  // kHandle0 and 2 on kHandle1
  EXPECT_EQ(3, handle0_packet_count);
  EXPECT_EQ(2, handle1_packet_count);

  // Notify the processed packets with a Number Of Completed Packet HCI event.
  // This should cause 5 (not 6!) packets to be sent.
  StaticByteBuffer nocp_event_buffer_0(0x13, 0x09,              // Event header
                                       0x02,                    // Number of handles
                                       0x01, 0x00, 0x03, 0x00,  // 3 packets on handle 0x0001
                                       0x02, 0x00, 0x02, 0x00   // 2 packets on handle 0x0002
  );
  test_device()->SendCommandChannelPacket(nocp_event_buffer_0);
  RunLoopUntilIdle();
  // The controller should have received 2 packets on kHandle0 and 3 on kHandle1.
  EXPECT_EQ(5, handle0_packet_count);
  EXPECT_EQ(5, handle1_packet_count);

  // Notify the processed packets with a Number Of Completed Packet HCI event.
  // This should cause 1 packet to be sent.
  StaticByteBuffer nocp_event_buffer_1(0x13, 0x09,              // Event header
                                       0x02,                    // Number of handles
                                       0x01, 0x00, 0x02, 0x00,  // 3 packets on handle 0x0001
                                       0x02, 0x00, 0x03, 0x00   // 2 packets on handle 0x0002
  );
  test_device()->SendCommandChannelPacket(nocp_event_buffer_1);
  RunLoopUntilIdle();
  // The controller should have received 1 packets on kHandle0 and 0 on kHandle1.
  EXPECT_EQ(6, handle0_packet_count);
  EXPECT_EQ(5, handle1_packet_count);
}

// Test that SendPacket works using the LE buffer when no BR/EDR buffer is
// available.
TEST_F(ACLDataChannelTest, SendPacketLEBuffer) {
  constexpr size_t kMaxMTU = 5;
  constexpr size_t kTotalAttempts = 12;
  constexpr size_t kTotalExpected = 6;
  constexpr size_t kBufferNumPackets = 3;
  constexpr hci_spec::ConnectionHandle kHandle0 = 0x0001;
  constexpr hci_spec::ConnectionHandle kHandle1 = 0x0002;

  InitializeACLDataChannel(DataBufferInfo(), DataBufferInfo(kMaxMTU, kBufferNumPackets));

  acl_data_channel()->RegisterLink(kHandle0, bt::LinkType::kLE);
  acl_data_channel()->RegisterLink(kHandle1, bt::LinkType::kACL);

  // This should fail because the payload exceeds the MTU.
  auto packet = ACLDataPacket::New(kHandle1, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                   hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMTU + 1);
  EXPECT_FALSE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kInvalidChannelId,
                                              AclDataChannel::PacketPriority::kLow));

  size_t handle0_packet_count = 0;
  size_t handle1_packet_count = 0;
  auto data_callback = [&](const ByteBuffer& bytes) {
    BT_DEBUG_ASSERT(bytes.size() >= sizeof(hci_spec::ACLDataHeader));

    PacketView<hci_spec::ACLDataHeader> packet(&bytes,
                                               bytes.size() - sizeof(hci_spec::ACLDataHeader));
    hci_spec::ConnectionHandle connection_handle =
        le16toh(packet.header().handle_and_flags) & 0xFFF;

    if (connection_handle == kHandle0) {
      handle0_packet_count++;
    } else {
      ASSERT_EQ(kHandle1, connection_handle);
      handle1_packet_count++;
    }
  };
  test_device()->SetDataCallback(data_callback, dispatcher());

  // Queue up 12 packets in total, distributed among the two connection handles
  // and link types. Since the BR/EDR MTU is zero, we expect to only see LE
  // packets transmitted.
  async::PostTask(dispatcher(), [this] {
    for (size_t i = 0; i < kTotalAttempts; ++i) {
      hci_spec::ConnectionHandle handle = (i % 2) ? kHandle1 : kHandle0;
      auto packet = ACLDataPacket::New(handle, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                       hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMTU);
      if (i % 2) {
        // ACL-U packets should fail due to 0 MTU size.
        EXPECT_FALSE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kInvalidChannelId,
                                                    AclDataChannel::PacketPriority::kLow));
      } else {
        EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kInvalidChannelId,
                                                   AclDataChannel::PacketPriority::kLow));
      }
    }
  });

  RunLoopUntilIdle();

  // The controller can buffer 3 packets. Since BR/EDR packets should have
  // failed to go out, the controller should have received 3 packets on handle 0
  // and none on handle 1.
  EXPECT_EQ(kTotalExpected / 2u, handle0_packet_count);
  EXPECT_EQ(0u, handle1_packet_count);

  // Notify the processed packets with a Number Of Completed Packet HCI event.
  StaticByteBuffer event_buffer(0x13, 0x05,  // Event header
                                0x01,        // Number of handles
                                0x01, 0x00, 0x03,
                                0x00  // 3 packets on handle 0x0001
  );
  test_device()->SendCommandChannelPacket(event_buffer);

  RunLoopUntilIdle();

  EXPECT_EQ(kTotalExpected, handle0_packet_count);
  EXPECT_EQ(0u, handle1_packet_count);
}

// Test that SendPacket works for LE packets when both buffer types are
// available.
TEST_F(ACLDataChannelTest, SendLEPacketBothBuffers) {
  constexpr size_t kMaxMTU = 200;
  constexpr size_t kMaxNumPackets = 50;
  constexpr size_t kLEMaxMTU = 5;
  constexpr size_t kLEMaxNumPackets = 5;
  constexpr hci_spec::ConnectionHandle kHandle0 = 0x0001;
  constexpr hci_spec::ConnectionHandle kHandle1 = 0x0002;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets),
                           DataBufferInfo(kLEMaxMTU, kLEMaxNumPackets));

  acl_data_channel()->RegisterLink(kHandle0, bt::LinkType::kLE);
  acl_data_channel()->RegisterLink(kHandle1, bt::LinkType::kLE);

  // This should fail because the payload exceeds the LE MTU.
  auto packet = ACLDataPacket::New(kHandle1, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                   hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMTU);
  EXPECT_FALSE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kInvalidChannelId,
                                              AclDataChannel::PacketPriority::kLow));

  int handle0_packet_count = 0;
  int handle1_packet_count = 0;
  auto data_callback = [&](const ByteBuffer& bytes) {
    BT_DEBUG_ASSERT(bytes.size() >= sizeof(hci_spec::ACLDataHeader));

    PacketView<hci_spec::ACLDataHeader> packet(&bytes,
                                               bytes.size() - sizeof(hci_spec::ACLDataHeader));
    hci_spec::ConnectionHandle connection_handle =
        le16toh(packet.header().handle_and_flags) & 0xFFF;

    if (connection_handle == kHandle0) {
      handle0_packet_count++;
    } else {
      ASSERT_EQ(kHandle1, connection_handle);
      handle1_packet_count++;
    }
  };
  test_device()->SetDataCallback(data_callback, dispatcher());

  // Queue up 10 packets in total, distributed among the two connection handles.
  async::PostTask(dispatcher(), [this] {
    for (int i = 0; i < 10; ++i) {
      hci_spec::ConnectionHandle handle = (i % 2) ? kHandle1 : kHandle0;
      auto packet = ACLDataPacket::New(handle, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                       hci_spec::ACLBroadcastFlag::kPointToPoint, kLEMaxMTU);
      EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kInvalidChannelId,
                                                 AclDataChannel::PacketPriority::kLow));
    }
  });

  RunLoopUntilIdle();

  // ACLDataChannel should be looking at kLEMaxNumPackets, which is 5. The
  // controller should have received 3 packets on kHandle0 and 2 on kHandle1
  EXPECT_EQ(3, handle0_packet_count);
  EXPECT_EQ(2, handle1_packet_count);

  // Notify the processed packets with a Number Of Completed Packet HCI event.
  StaticByteBuffer event_buffer(0x13, 0x09,              // Event header
                                0x02,                    // Number of handles
                                0x01, 0x00, 0x03, 0x00,  // 3 packets on handle 0x0001
                                0x02, 0x00, 0x02, 0x00   // 2 packets on handle 0x0002
  );
  test_device()->SendCommandChannelPacket(event_buffer);

  RunLoopUntilIdle();

  EXPECT_EQ(5, handle0_packet_count);
  EXPECT_EQ(5, handle1_packet_count);
}

// Test that SendPacket works for BR/EDR packets when both buffer types are
// available.
TEST_F(ACLDataChannelTest, SendBREDRPacketBothBuffers) {
  constexpr size_t kLEMaxMTU = 200;
  constexpr size_t kLEMaxNumPackets = 50;
  constexpr size_t kMaxMTU = 5;
  constexpr size_t kMaxNumPackets = 5;
  constexpr hci_spec::ConnectionHandle kHandle0 = 0x0001;
  constexpr hci_spec::ConnectionHandle kHandle1 = 0x0002;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets),
                           DataBufferInfo(kLEMaxMTU, kLEMaxNumPackets));

  acl_data_channel()->RegisterLink(kHandle0, bt::LinkType::kACL);
  acl_data_channel()->RegisterLink(kHandle1, bt::LinkType::kACL);

  // This should fail because the payload exceeds the ACL MTU.
  auto packet = ACLDataPacket::New(kHandle1, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                   hci_spec::ACLBroadcastFlag::kPointToPoint, kLEMaxMTU);
  EXPECT_FALSE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kInvalidChannelId,
                                              AclDataChannel::PacketPriority::kLow));

  int handle0_packet_count = 0;
  int handle1_packet_count = 0;
  auto data_callback = [&](const ByteBuffer& bytes) {
    BT_DEBUG_ASSERT(bytes.size() >= sizeof(hci_spec::ACLDataHeader));

    PacketView<hci_spec::ACLDataHeader> packet(&bytes,
                                               bytes.size() - sizeof(hci_spec::ACLDataHeader));
    hci_spec::ConnectionHandle connection_handle =
        le16toh(packet.header().handle_and_flags) & 0xFFF;

    if (connection_handle == kHandle0) {
      handle0_packet_count++;
    } else {
      ASSERT_EQ(kHandle1, connection_handle);
      handle1_packet_count++;
    }
  };
  test_device()->SetDataCallback(data_callback, dispatcher());

  // Queue up 10 packets in total, distributed among the two connection handles.
  async::PostTask(dispatcher(), [this] {
    for (int i = 0; i < 10; ++i) {
      hci_spec::ConnectionHandle handle = (i % 2) ? kHandle1 : kHandle0;
      auto packet = ACLDataPacket::New(handle, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                       hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMTU);
      EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kInvalidChannelId,
                                                 AclDataChannel::PacketPriority::kLow));
    }
  });

  RunLoopUntilIdle();

  // ACLDataChannel should be looking at kLEMaxNumPackets, which is 5. The
  // controller should have received 3 packets on kHandle0 and 2 on kHandle1
  EXPECT_EQ(3, handle0_packet_count);
  EXPECT_EQ(2, handle1_packet_count);

  // Notify the processed packets with a Number Of Completed Packet HCI event.
  StaticByteBuffer event_buffer(0x13, 0x09,              // Event header
                                0x02,                    // Number of handles
                                0x01, 0x00, 0x03, 0x00,  // 3 packets on handle 0x0001
                                0x02, 0x00, 0x02, 0x00   // 2 packets on handle 0x0002
  );
  test_device()->SendCommandChannelPacket(event_buffer);

  RunLoopUntilIdle();

  EXPECT_EQ(5, handle0_packet_count);
  EXPECT_EQ(5, handle1_packet_count);
}

TEST_F(ACLDataChannelTest, SendPacketsFailure) {
  constexpr size_t kMaxMTU = 5;
  constexpr hci_spec::ConnectionHandle kHandle = 0x0001;
  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, 100), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle, bt::LinkType::kACL);

  // Empty packet list.
  EXPECT_FALSE(acl_data_channel()->SendPackets(std::list<ACLDataPacketPtr>(),
                                               l2cap::kInvalidChannelId,
                                               AclDataChannel::PacketPriority::kLow));

  // Packet exceeds MTU
  std::list<ACLDataPacketPtr> packets;
  packets.push_back(ACLDataPacket::New(kHandle, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                       hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMTU + 1));
  EXPECT_FALSE(acl_data_channel()->SendPackets(std::move(packets), l2cap::kInvalidChannelId,
                                               AclDataChannel::PacketPriority::kLow));
}

// Suffix DeathTest has GoogleTest-specific behavior
using ACLDataChannelDeathTest = HCI_ACLDataChannelTest;

TEST_F(ACLDataChannelDeathTest, SendPacketsCrashesWithContinuingFragments) {
  constexpr size_t kMaxMTU = 5;
  constexpr hci_spec::ConnectionHandle kHandle = 0x0001;
  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, 100), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle, bt::LinkType::kACL);

  std::list<ACLDataPacketPtr> packets;
  packets.push_back(ACLDataPacket::New(kHandle,
                                       hci_spec::ACLPacketBoundaryFlag::kContinuingFragment,
                                       hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMTU));
  ASSERT_DEATH_IF_SUPPORTED(
      acl_data_channel()->SendPackets(std::move(packets), l2cap::kInvalidChannelId,
                                      AclDataChannel::PacketPriority::kLow),
      "expected full PDU");
}

TEST_F(ACLDataChannelDeathTest, SendPacketsCrashesWithPacketsForMoreThanOneConnection) {
  constexpr size_t kMaxMTU = 5;
  constexpr hci_spec::ConnectionHandle kHandle0 = 0x0001;
  constexpr hci_spec::ConnectionHandle kHandle1 = 0x0002;
  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, 100), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle0, bt::LinkType::kACL);
  acl_data_channel()->RegisterLink(kHandle1, bt::LinkType::kACL);

  // Packet exceeds MTU
  std::list<ACLDataPacketPtr> packets;
  packets.push_back(ACLDataPacket::New(kHandle0,
                                       hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                       hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMTU));
  packets.push_back(ACLDataPacket::New(kHandle1,
                                       hci_spec::ACLPacketBoundaryFlag::kContinuingFragment,
                                       hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMTU));
  ASSERT_DEATH_IF_SUPPORTED(
      acl_data_channel()->SendPackets(std::move(packets), l2cap::kInvalidChannelId,
                                      AclDataChannel::PacketPriority::kLow),
      "expected only fragments for one connection");
}

// Tests sending multiple packets in a single call.
TEST_F(ACLDataChannelTest, SendPackets) {
  constexpr int kExpectedPacketCount = 5;
  constexpr hci_spec::ConnectionHandle kHandle = 0x0001;

  InitializeACLDataChannel(DataBufferInfo(1024, 100), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle, bt::LinkType::kLE);

  bool pass = true;
  int seq_no = 0;
  auto data_cb = [&pass, &seq_no](const ByteBuffer& bytes) {
    BT_DEBUG_ASSERT(bytes.size() >= sizeof(hci_spec::ACLDataHeader));
    PacketView<hci_spec::ACLDataHeader> packet(&bytes,
                                               bytes.size() - sizeof(hci_spec::ACLDataHeader));
    EXPECT_EQ(1u, packet.payload_size());

    int cur_no = packet.payload_data().data()[0];
    if (cur_no != seq_no + 1) {
      pass = false;
      return;
    }

    seq_no = cur_no;
  };
  test_device()->SetDataCallback(data_cb, dispatcher());

  std::list<ACLDataPacketPtr> packets;
  for (int i = 1; i <= kExpectedPacketCount; ++i) {
    auto packet = ACLDataPacket::New(kHandle, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     hci_spec::ACLBroadcastFlag::kPointToPoint, 1);
    packet->mutable_view()->mutable_payload_bytes()[0] = i;
    packets.push_back(std::move(packet));
  }

  EXPECT_TRUE(acl_data_channel()->SendPackets(std::move(packets), l2cap::kInvalidChannelId,
                                              AclDataChannel::PacketPriority::kLow));

  RunLoopUntilIdle();

  EXPECT_TRUE(pass);
  EXPECT_EQ(kExpectedPacketCount, seq_no);
}

TEST_F(ACLDataChannelTest,
       UnregisterLinkDoesNotClearNumSentPacketsAndClearControllerPacketCountDoes) {
  constexpr size_t kMaxMTU = 1024;
  constexpr size_t kMaxNumPackets = 2;
  constexpr hci_spec::ConnectionHandle kHandle1 = 1;
  constexpr hci_spec::ConnectionHandle kHandle2 = 2;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle1, bt::LinkType::kLE);
  acl_data_channel()->RegisterLink(kHandle2, bt::LinkType::kLE);

  int packet_count = 0;
  test_device()->SetDataCallback([&](const auto&) { packet_count++; }, dispatcher());

  // Send 3 packets on two links. This is enough to fill up the data buffers.
  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle1, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                         hci_spec::ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId, AclDataChannel::PacketPriority::kLow));
  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle2, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                         hci_spec::ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId, AclDataChannel::PacketPriority::kLow));
  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle1, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                         hci_spec::ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId, AclDataChannel::PacketPriority::kLow));

  RunLoopUntilIdle();

  // The third packet should have been queued.
  ASSERT_EQ(2, packet_count);

  // UnregisterLink should not decrement sent packet count,
  // so next packet should not be sent.
  acl_data_channel()->UnregisterLink(kHandle2);
  RunLoopUntilIdle();
  ASSERT_EQ(2, packet_count);

  // Clear the controller packet count for |kHandle2|. The next packet should go out.
  acl_data_channel()->ClearControllerPacketCount(kHandle2);
  RunLoopUntilIdle();
  ASSERT_EQ(3, packet_count);
}

TEST_F(ACLDataChannelTest, SendingPacketsOnUnregisteredLinkDropsPackets) {
  constexpr size_t kMaxMTU = 1024;
  constexpr size_t kMaxNumPackets = 2;
  constexpr hci_spec::ConnectionHandle kHandle = 1;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets), DataBufferInfo());

  int packet_count = 0;
  test_device()->SetDataCallback([&](const auto&) { packet_count++; }, dispatcher());

  // Send packet with unregistered handle.
  ASSERT_FALSE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                         hci_spec::ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId, AclDataChannel::PacketPriority::kLow));

  RunLoopUntilIdle();

  // Packet should not have been sent.
  ASSERT_EQ(0, packet_count);

  // Now register link. Packet should have been dropped and should still
  // not be sent.
  acl_data_channel()->RegisterLink(kHandle, bt::LinkType::kLE);

  RunLoopUntilIdle();

  // Packet should not have been sent.
  ASSERT_EQ(0, packet_count);

  // Unregister a link that has not been registered
  acl_data_channel()->UnregisterLink(kHandle);
  RunLoopUntilIdle();
  ASSERT_EQ(0, packet_count);
}

TEST_F(ACLDataChannelTest, UnregisterLinkClearsPendingPackets) {
  constexpr size_t kMaxMTU = 1024;
  constexpr size_t kMaxNumPackets = 1;
  constexpr hci_spec::ConnectionHandle kHandle1 = 1;
  constexpr hci_spec::ConnectionHandle kHandle2 = 2;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle1, bt::LinkType::kLE);
  acl_data_channel()->RegisterLink(kHandle2, bt::LinkType::kLE);

  int handle1_packet_count = 0;
  int handle2_packet_count = 0;
  auto data_callback = [&](const ByteBuffer& bytes) {
    BT_DEBUG_ASSERT(bytes.size() >= sizeof(hci_spec::ACLDataHeader));

    PacketView<hci_spec::ACLDataHeader> packet(&bytes,
                                               bytes.size() - sizeof(hci_spec::ACLDataHeader));
    hci_spec::ConnectionHandle connection_handle =
        le16toh(packet.header().handle_and_flags) & 0xFFF;

    if (connection_handle == kHandle1) {
      handle1_packet_count++;
    } else {
      ASSERT_EQ(kHandle2, connection_handle);
      handle2_packet_count++;
    }
  };
  test_device()->SetDataCallback(data_callback, dispatcher());

  // Send 3 packets on two links. This is enough to fill up the data buffers.
  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle1, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                         hci_spec::ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId, AclDataChannel::PacketPriority::kLow));
  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle2, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                         hci_spec::ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId, AclDataChannel::PacketPriority::kLow));
  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle1, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                         hci_spec::ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId, AclDataChannel::PacketPriority::kLow));

  RunLoopUntilIdle();

  // Only kHandle1 packet should have been sent
  ASSERT_EQ(1, handle1_packet_count);
  ASSERT_EQ(0, handle2_packet_count);

  // Clear pending packet for |kHandle2|
  acl_data_channel()->UnregisterLink(kHandle2);

  // Notify the processed packet with a Number Of Completed Packet HCI event.
  StaticByteBuffer event_buffer(0x13, 0x05,             // Event header
                                0x01,                   // Number of handles
                                0x01, 0x00, 0x01, 0x00  // 1 packet on handle 0x0001
  );
  test_device()->SendCommandChannelPacket(event_buffer);

  RunLoopUntilIdle();

  // second |kHandle1| packet should have been sent
  ASSERT_EQ(2, handle1_packet_count);
  ASSERT_EQ(0, handle2_packet_count);
}

TEST_F(ACLDataChannelTest, PacketsQueuedByFlowControlAreNotSentAfterUnregisterLink) {
  constexpr size_t kMaxMTU = 1024;
  constexpr size_t kMaxNumPackets = 1;
  constexpr hci_spec::ConnectionHandle kHandle1 = 1;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle1, bt::LinkType::kLE);

  int packet_count = 0;
  test_device()->SetDataCallback([&](const auto&) { packet_count++; }, dispatcher());

  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle1, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                         hci_spec::ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId, AclDataChannel::PacketPriority::kLow));
  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle1, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                         hci_spec::ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId, AclDataChannel::PacketPriority::kLow));

  RunLoopUntilIdle();

  // The second packet should have been queued.
  ASSERT_EQ(1, packet_count);

  // Clear the packet count for |kHandle2|. The second packet should NOT go out.
  acl_data_channel()->UnregisterLink(kHandle1);

  // Notify the processed packet with a Number Of Completed Packet HCI event.
  StaticByteBuffer event_buffer(0x13, 0x05,             // Event header
                                0x01,                   // Number of handles
                                0x01, 0x00, 0x01, 0x00  // 1 packet on handle 0x0001
  );
  test_device()->SendCommandChannelPacket(event_buffer);

  RunLoopUntilIdle();

  // The second packet should not have been sent
  ASSERT_EQ(1, packet_count);
}

TEST_F(ACLDataChannelTest,
       StalePacketsBufferedBeforeFirstUnregisterAndBeforeSecondRegisterAreNotSent) {
  constexpr size_t kMaxMTU = 1024;
  constexpr size_t kMaxNumPackets = 1;
  constexpr hci_spec::ConnectionHandle kHandle = 1;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle, bt::LinkType::kLE);

  // Unique packet to send to re-registered link with same handle.
  const StaticByteBuffer kPacket(
      // ACL data header (handle: 1, length 1)
      0x01, 0x00, 0x01, 0x00,

      // Unique payload to distinguish this packet from stale packet
      0x01);

  int data_cb_count = 0;
  auto data_cb = [&](const ByteBuffer& packet) {
    data_cb_count++;
    if (data_cb_count == 2) {
      EXPECT_TRUE(ContainersEqual(kPacket, packet));
    }
  };
  test_device()->SetDataCallback(data_cb, dispatcher());

  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                         hci_spec::ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId, AclDataChannel::PacketPriority::kLow));
  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                         hci_spec::ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId, AclDataChannel::PacketPriority::kLow));

  RunLoopUntilIdle();

  // The second packet should have been queued.
  ASSERT_EQ(1, data_cb_count);

  // Clear the packet count for |kHandle2|. The second packet should NOT go out.
  acl_data_channel()->UnregisterLink(kHandle);

  // Notify the processed packet with a Number Of Completed Packet HCI event.
  StaticByteBuffer event_buffer(0x13, 0x05,             // Event header
                                0x01,                   // Number of handles
                                0x01, 0x00, 0x01, 0x00  // 1 packet on handle 0x0001
  );
  test_device()->SendCommandChannelPacket(event_buffer);

  RunLoopUntilIdle();

  // The second packet should not have been sent
  ASSERT_EQ(1, data_cb_count);

  // Re-Register same link handle
  acl_data_channel()->RegisterLink(kHandle, bt::LinkType::kLE);

  auto packet = ACLDataPacket::New(kHandle, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                   hci_spec::ACLBroadcastFlag::kPointToPoint, 1);
  packet->mutable_view()->mutable_payload_data().Fill(1);

  ASSERT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kInvalidChannelId,
                                             AclDataChannel::PacketPriority::kLow));

  RunLoopUntilIdle();

  // The new packet should have been sent to the MockController.
  ASSERT_EQ(2, data_cb_count);
}

TEST_F(ACLDataChannelTest, UnregisterLinkDropsFutureSentPackets) {
  constexpr size_t kMaxMTU = 1024;
  constexpr size_t kMaxNumPackets = 1;
  constexpr hci_spec::ConnectionHandle kHandle = 1;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle, bt::LinkType::kLE);

  int packet_count = 0;
  test_device()->SetDataCallback([&](const auto&) { packet_count++; }, dispatcher());

  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                         hci_spec::ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId, AclDataChannel::PacketPriority::kLow));

  RunLoopUntilIdle();
  ASSERT_EQ(1, packet_count);

  acl_data_channel()->UnregisterLink(kHandle);

  // attempt to send packet on unregistered link
  ASSERT_FALSE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                         hci_spec::ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId, AclDataChannel::PacketPriority::kLow));

  RunLoopUntilIdle();
  // second packet should not have been sent
  ASSERT_EQ(1, packet_count);
}

TEST_F(ACLDataChannelTest, ReceiveData) {
  constexpr size_t kMaxMTU = 5;
  constexpr size_t kMaxNumPackets = 5;

  // It doesn't matter what we set the buffer values to since we're testing
  // incoming packets.
  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets), DataBufferInfo());

  constexpr size_t kExpectedPacketCount = 2u;
  size_t num_rx_packets = 0u;
  hci_spec::ConnectionHandle packet0_handle;
  hci_spec::ConnectionHandle packet1_handle;
  auto data_rx_cb = [&](ACLDataPacketPtr packet) {
    num_rx_packets++;
    if (num_rx_packets == 1) {
      packet0_handle = packet->connection_handle();
    } else if (num_rx_packets == 2) {
      packet1_handle = packet->connection_handle();
    } else {
      BT_PANIC("|num_rx_packets| has unexpected value: %zu", num_rx_packets);
    }
  };
  set_data_received_callback(std::move(data_rx_cb));

  // Malformed packet: smaller than the ACL header.
  StaticByteBuffer invalid0(0x01, 0x00, 0x00);

  // Malformed packet: the payload size given in the header doesn't match the
  // actual payload size.
  StaticByteBuffer invalid1(0x01, 0x00, 0x02, 0x00, 0x00);

  // Valid packet on handle 1.
  StaticByteBuffer valid0(0x01, 0x00, 0x01, 0x00, 0x00);

  // Valid packet on handle 2.
  StaticByteBuffer valid1(0x02, 0x00, 0x01, 0x00, 0x00);

  async::PostTask(dispatcher(), [&, this] {
    test_device()->SendACLDataChannelPacket(invalid0);
    test_device()->SendACLDataChannelPacket(invalid1);
    test_device()->SendACLDataChannelPacket(valid0);
    test_device()->SendACLDataChannelPacket(valid1);
  });

  RunLoopUntilIdle();

  EXPECT_EQ(kExpectedPacketCount, num_rx_packets);
  EXPECT_EQ(0x0001, packet0_handle);
  EXPECT_EQ(0x0002, packet1_handle);
}

TEST_F(ACLDataChannelTest, TransportClosedCallbackBothChannels) {
  InitializeACLDataChannel(DataBufferInfo(1u, 1u), DataBufferInfo(1u, 1u));

  int closed_cb_count = 0;
  auto closed_cb = [&closed_cb_count] { closed_cb_count++; };
  transport()->SetTransportClosedCallback(closed_cb);

  async::PostTask(dispatcher(), [this] { test_device()->Stop(ZX_ERR_PEER_CLOSED); });

  RunLoopUntilIdle();
  EXPECT_EQ(1, closed_cb_count);
}

// Make sure that a HCI "Number of completed packets" event received after shut
// down does not cause a crash.
TEST_F(ACLDataChannelTest, HciEventReceivedAfterShutDown) {
  InitializeACLDataChannel(DataBufferInfo(1u, 1u), DataBufferInfo(1u, 1u));

  // Notify the processed packets with a Number Of Completed Packet HCI event.
  StaticByteBuffer event_buffer(0x13, 0x09,              // Event header
                                0x02,                    // Number of handles
                                0x01, 0x00, 0x03, 0x00,  // 3 packets on handle 0x0001
                                0x02, 0x00, 0x02, 0x00   // 2 packets on handle 0x0002
  );

  // Shuts down ACLDataChannel and CommandChannel.
  DeleteTransport();

  test_device()->SendCommandChannelPacket(event_buffer);
  RunLoopUntilIdle();
}

TEST_F(ACLDataChannelTest, DropQueuedPacketsRemovesPacketsMatchingFilterFromQueue) {
  constexpr size_t kMaxMTU = 5;
  constexpr size_t kMaxNumPackets = 5;
  constexpr hci_spec::ConnectionHandle kHandle0 = 0x0001;
  constexpr hci_spec::ConnectionHandle kHandle1 = 0x0002;
  constexpr l2cap::ChannelId kChanId0 = 0x40;
  constexpr l2cap::ChannelId kChanId1 = 0x41;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle0, bt::LinkType::kLE);
  acl_data_channel()->RegisterLink(kHandle1, bt::LinkType::kACL);

  int handle0_packet_count = 0;
  int handle1_packet_count = 0;

  // Callback invoked by TestDevice when it receive a data packet from us.
  auto data_callback = [&](const ByteBuffer& bytes) {
    BT_DEBUG_ASSERT(bytes.size() >= sizeof(hci_spec::ACLDataHeader));

    PacketView<hci_spec::ACLDataHeader> packet(&bytes,
                                               bytes.size() - sizeof(hci_spec::ACLDataHeader));
    hci_spec::ConnectionHandle connection_handle =
        le16toh(packet.header().handle_and_flags) & 0xFFF;

    if (connection_handle == kHandle0) {
      handle0_packet_count++;
    } else {
      ASSERT_EQ(kHandle1, connection_handle);
      handle1_packet_count++;
    }
  };
  test_device()->SetDataCallback(data_callback, dispatcher());

  // Queue up 10 packets in total, distributed among the two connection handles.
  for (size_t i = 0; i < 2 * kMaxNumPackets; ++i) {
    hci_spec::ConnectionHandle handle = (i % 2) ? kHandle1 : kHandle0;
    auto packet = ACLDataPacket::New(handle, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMTU);
    EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), (i % 2) ? kChanId1 : kChanId0,
                                               AclDataChannel::PacketPriority::kLow));
  }

  RunLoopUntilIdle();

  // kMaxNumPackets is 5. The controller should have received 3 packets on
  // kHandle0 and 2 on kHandle1
  EXPECT_EQ(3, handle0_packet_count);
  EXPECT_EQ(2, handle1_packet_count);

  // Should remove 3 |kHandle1| packets from queue
  size_t predicate_count = 0;
  size_t predicate_true_count = 0;
  acl_data_channel()->DropQueuedPackets([&](const ACLDataPacketPtr& packet, l2cap::ChannelId id) {
    predicate_count++;
    // Verify that correct channels are passed to filter lambda
    if (packet->connection_handle() == kHandle0) {
      EXPECT_EQ(id, kChanId0);
    } else if (packet->connection_handle() == kHandle1) {
      EXPECT_EQ(id, kChanId1);
    }

    bool result = packet->connection_handle() == kHandle1;
    if (result) {
      predicate_true_count++;
    }
    return result;
  });
  // Should be called for each packet in queue (2 |kHandle0| packets + 3 |kHandle1| packets)
  EXPECT_EQ(predicate_count, 5u);
  // 3 |kHandle1| packets
  EXPECT_EQ(predicate_true_count, 3u);

  // Notify the processed packets with a Number Of Completed Packet HCI event.
  StaticByteBuffer event_buffer(0x13, 0x09,              // Event header
                                0x02,                    // Number of handles
                                0x01, 0x00, 0x03, 0x00,  // 3 packets on handle 0x0001
                                0x02, 0x00, 0x02, 0x00   // 2 packets on handle 0x0002
  );
  test_device()->SendCommandChannelPacket(event_buffer);

  RunLoopUntilIdle();

  EXPECT_EQ(5, handle0_packet_count);
  // Other 3 |kHandle1| packets should have been filtered out of queue
  EXPECT_EQ(2, handle1_packet_count);
}

TEST_F(ACLDataChannelTest, HighPriorityPacketsQueuedAfterLowPriorityPacketsAreSentFirst) {
  constexpr size_t kMaxMTU = 5;
  constexpr size_t kMaxNumPackets = 5;
  constexpr hci_spec::ConnectionHandle kHandle0 = 0x0001;
  constexpr hci_spec::ConnectionHandle kHandle1 = 0x0002;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle0, bt::LinkType::kACL);
  acl_data_channel()->RegisterLink(kHandle1, bt::LinkType::kACL);

  size_t handle0_packet_count = 0;
  size_t handle1_packet_count = 0;

  // Callback invoked by TestDevice when it receive a data packet from us.
  auto data_callback = [&](const ByteBuffer& bytes) {
    BT_DEBUG_ASSERT(bytes.size() >= sizeof(hci_spec::ACLDataHeader));

    PacketView<hci_spec::ACLDataHeader> packet(&bytes,
                                               bytes.size() - sizeof(hci_spec::ACLDataHeader));
    hci_spec::ConnectionHandle connection_handle =
        le16toh(packet.header().handle_and_flags) & 0xFFF;

    if (connection_handle == kHandle0) {
      handle0_packet_count++;
    } else {
      ASSERT_EQ(kHandle1, connection_handle);
      handle1_packet_count++;
    }
  };
  test_device()->SetDataCallback(data_callback, dispatcher());

  // Fill controller with |kMaxNumPackets| packets so queue can grow.
  for (size_t i = 0; i < kMaxNumPackets; ++i) {
    auto packet = ACLDataPacket::New(kHandle0, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMTU);
    EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kFirstDynamicChannelId,
                                               AclDataChannel::PacketPriority::kLow));
  }
  RunLoopUntilIdle();

  EXPECT_EQ(kMaxNumPackets, handle0_packet_count);
  handle0_packet_count = 0;

  // Queue up 10 packets in total, distributed among the two connection handles.
  for (size_t i = 0; i < 2 * kMaxNumPackets; ++i) {
    hci_spec::ConnectionHandle handle = (i % 2) ? kHandle1 : kHandle0;
    auto priority =
        (i % 2) ? AclDataChannel::PacketPriority::kLow : AclDataChannel::PacketPriority::kHigh;
    auto packet = ACLDataPacket::New(handle, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMTU);
    EXPECT_TRUE(
        acl_data_channel()->SendPacket(std::move(packet), l2cap::kFirstDynamicChannelId, priority));
  }

  RunLoopUntilIdle();

  // No packets should have been sent because controller buffer is full.
  EXPECT_EQ(0u, handle0_packet_count);
  EXPECT_EQ(0u, handle1_packet_count);

  // Notify the processed packets with a Number Of Completed Packet HCI event.
  StaticByteBuffer event_buffer(0x13, 0x05,             // Event header
                                0x01,                   // Number of handles
                                0x01, 0x00, 0x05, 0x00  // 5 packets on handle 0x0001
  );
  test_device()->SendCommandChannelPacket(event_buffer);

  RunLoopUntilIdle();

  // Only high priority packets should have been sent.
  EXPECT_EQ(5u, handle0_packet_count);
  EXPECT_EQ(0u, handle1_packet_count);

  // Notify the processed packets with a Number Of Completed Packet HCI event.
  test_device()->SendCommandChannelPacket(event_buffer);

  RunLoopUntilIdle();
  EXPECT_EQ(5u, handle0_packet_count);
  // Now low priority packets should have been sent.
  EXPECT_EQ(5u, handle1_packet_count);
}

TEST_F(ACLDataChannelTest, OutOfBoundsPacketCountsIgnored) {
  constexpr size_t kMaxMTU = 5;
  constexpr size_t kMaxNumPackets = 6;
  constexpr hci_spec::ConnectionHandle kHandle0 = 0x0001;
  constexpr hci_spec::ConnectionHandle kHandle1 = 0x0002;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle0, bt::LinkType::kACL);
  acl_data_channel()->RegisterLink(kHandle1, bt::LinkType::kACL);

  size_t handle0_packet_count = 0;
  size_t handle1_packet_count = 0;

  // Callback invoked by TestDevice when it receive a data packet from us.
  auto data_callback = [&](const ByteBuffer& bytes) {
    BT_DEBUG_ASSERT(bytes.size() >= sizeof(hci_spec::ACLDataHeader));

    PacketView<hci_spec::ACLDataHeader> packet(&bytes,
                                               bytes.size() - sizeof(hci_spec::ACLDataHeader));
    hci_spec::ConnectionHandle connection_handle =
        le16toh(packet.header().handle_and_flags) & 0xFFF;

    if (connection_handle == kHandle0) {
      handle0_packet_count++;
    } else {
      ASSERT_EQ(kHandle1, connection_handle);
      handle1_packet_count++;
    }
  };
  test_device()->SetDataCallback(data_callback, dispatcher());

  // Fill controller with |kMaxNumPackets| packets split evenly between the two handles.
  for (size_t i = 0; i < kMaxNumPackets; ++i) {
    hci_spec::ConnectionHandle handle = (i % 2) ? kHandle1 : kHandle0;
    auto packet = ACLDataPacket::New(handle, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMTU);
    EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kFirstDynamicChannelId,
                                               AclDataChannel::PacketPriority::kLow));
  }
  RunLoopUntilIdle();

  EXPECT_EQ(kMaxNumPackets / 2, handle0_packet_count);
  EXPECT_EQ(kMaxNumPackets / 2, handle1_packet_count);
  handle0_packet_count = 0;
  handle1_packet_count = 0;

  // Queue up 3 packets for each handle,
  for (size_t i = 0; i < kMaxNumPackets / 2; ++i) {
    auto packet = ACLDataPacket::New(kHandle0, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMTU);
    EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kFirstDynamicChannelId,
                                               AclDataChannel::PacketPriority::kLow));
  }
  for (size_t i = 0; i < kMaxNumPackets / 2; ++i) {
    auto packet = ACLDataPacket::New(kHandle1, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMTU);
    EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kFirstDynamicChannelId,
                                               AclDataChannel::PacketPriority::kLow));
  }

  RunLoopUntilIdle();

  // No packets should have been sent because controller buffer is full.
  EXPECT_EQ(0u, handle0_packet_count);
  EXPECT_EQ(0u, handle1_packet_count);

  // Notify the processed packets with a Number Of Completed Packet HCI event.
  StaticByteBuffer event_buffer(0x13, 0x09,              // Event header
                                0x01,                    // Number of handles
                                0x01, 0x00, 0x03, 0x00,  // 3 packets on handle 0x0001
                                0x02, 0x00, 0x05, 0x00   // (ignored, not indicated in handle count)
  );

  test_device()->SendCommandChannelPacket(event_buffer);
  RunLoopUntilIdle();

  // Only packets on handle0 should have been sent.
  EXPECT_EQ(3u, handle0_packet_count);
  EXPECT_EQ(0u, handle1_packet_count);

  StaticByteBuffer short_buffer(0x13, 0x05,             // Event header
                                0x02,                   // Number of handles
                                0x02, 0x00, 0x02, 0x00  // 2 packets on handle 0x0002
                                // (missing second handle, should be ignored)
  );

  test_device()->SendCommandChannelPacket(short_buffer);
  RunLoopUntilIdle();

  // handle1 packets should have been sent anyway
  EXPECT_EQ(3u, handle0_packet_count);
  EXPECT_EQ(2u, handle1_packet_count);
}

class AclPriorityTest : public HCI_ACLDataChannelTest,
                        public ::testing::WithParamInterface<std::pair<hci::AclPriority, bool>> {};
TEST_P(AclPriorityTest, RequestAclPriority) {
  const auto kPriority = GetParam().first;
  const bool kExpectSuccess = GetParam().second;

  const DataBufferInfo kBREDRBufferInfo(1024, 50);
  InitializeACLDataChannel(kBREDRBufferInfo, DataBufferInfo());

  // Arbitrary command payload larger than hci_spec::CommandHeader.
  const auto op_code = hci_spec::VendorOpCode(0x01);
  const StaticByteBuffer kEncodedCommand(LowerBits(op_code), UpperBits(op_code),  // op code
                                         0x04,                                    // parameter size
                                         0x00, 0x01, 0x02, 0x03);                 // test parameter

  constexpr hci_spec::ConnectionHandle kLinkHandle = 0x0001;

  std::optional<hci_spec::ConnectionHandle> connection;
  std::optional<hci::AclPriority> priority;
  set_encode_acl_priority_command_cb(
      [&](hci_spec::ConnectionHandle cb_connection, hci::AclPriority cb_priority) {
        connection = cb_connection;
        priority = cb_priority;
        return fit::ok(DynamicByteBuffer(kEncodedCommand));
      });

  auto cmd_complete = bt::testing::CommandCompletePacket(
      op_code,
      kExpectSuccess ? hci_spec::StatusCode::SUCCESS : hci_spec::StatusCode::UNKNOWN_COMMAND);
  EXPECT_CMD_PACKET_OUT(test_device(), kEncodedCommand, &cmd_complete);

  size_t request_cb_count = 0;
  acl_data_channel()->RequestAclPriority(kPriority, kLinkHandle, [&](auto result) {
    request_cb_count++;
    EXPECT_EQ(kExpectSuccess, result.is_ok());
  });

  RunLoopUntilIdle();
  EXPECT_EQ(request_cb_count, 1u);
  ASSERT_TRUE(connection);
  EXPECT_EQ(connection.value(), kLinkHandle);
  ASSERT_TRUE(priority);
  EXPECT_EQ(priority.value(), kPriority);
}

const std::array<std::pair<hci::AclPriority, bool>, 4> kPriorityParams = {
    {{hci::AclPriority::kSource, /*expect_success=*/false},
     {hci::AclPriority::kSource, true},
     {hci::AclPriority::kSink, true},
     {hci::AclPriority::kNormal, true}}};
INSTANTIATE_TEST_SUITE_P(ACLDataChannelTest, AclPriorityTest, ::testing::ValuesIn(kPriorityParams));

TEST_F(ACLDataChannelTest, RequestAclPriorityEncodeFails) {
  const DataBufferInfo kBREDRBufferInfo(1024, 50);
  InitializeACLDataChannel(kBREDRBufferInfo, DataBufferInfo());

  set_encode_acl_priority_command_cb([&](auto, auto) { return fit::error(ZX_ERR_INTERNAL); });

  size_t request_cb_count = 0;
  acl_data_channel()->RequestAclPriority(hci::AclPriority::kSink, kLinkHandle, [&](auto result) {
    request_cb_count++;
    EXPECT_TRUE(result.is_error());
  });

  RunLoopUntilIdle();
  EXPECT_EQ(request_cb_count, 1u);
}

TEST_F(ACLDataChannelTest, RequestAclPriorityEncodeReturnsTooSmallBuffer) {
  const DataBufferInfo kBREDRBufferInfo(1024, 50);
  InitializeACLDataChannel(kBREDRBufferInfo, DataBufferInfo());

  set_encode_acl_priority_command_cb(
      [](auto, auto) { return fit::ok(DynamicByteBuffer(StaticByteBuffer(0x00))); });

  size_t request_cb_count = 0;
  acl_data_channel()->RequestAclPriority(hci::AclPriority::kSink, kLinkHandle, [&](auto result) {
    request_cb_count++;
    EXPECT_TRUE(result.is_error());
  });

  RunLoopUntilIdle();
  EXPECT_EQ(request_cb_count, 1u);
}

TEST_F(ACLDataChannelTest,
       SendingLowPriorityBrEdrPacketsWhenTooManyAreQueuedDropsLeastRecentlySentPduOnSameChannel) {
  constexpr size_t kMaxMtu = 4;
  constexpr size_t kMaxNumPackets = 2;
  constexpr hci_spec::ConnectionHandle kHandle0 = 0x0001;
  constexpr hci_spec::ConnectionHandle kHandle1 = 0x0002;

  InitializeACLDataChannel(DataBufferInfo(kMaxMtu, kMaxNumPackets),
                           DataBufferInfo(kMaxMtu, kMaxNumPackets));

  acl_data_channel()->RegisterLink(kHandle0, bt::LinkType::kLE);
  acl_data_channel()->RegisterLink(kHandle1, bt::LinkType::kACL);

  inspect::Inspector inspector;
  acl_data_channel()->AttachInspect(inspector.GetRoot(), AclDataChannel::kInspectNodeName);

  // Fill up both LE and BR/EDR controller buffers
  for (hci_spec::ConnectionHandle handle : {kHandle0, kHandle1}) {
    for (size_t i = 0; i < kMaxNumPackets; ++i) {
      auto packet = ACLDataPacket::New(handle, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                       hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMtu);
      EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kFirstDynamicChannelId,
                                                 AclDataChannel::PacketPriority::kLow));
    }
  }
  RunLoopUntilIdle();

  // Callback invoked by TestDevice when it receive a data packet from us.
  size_t packet_count = 0;
  auto data_callback = [&](const ByteBuffer& bytes) {
    ASSERT_LE(sizeof(hci_spec::ACLDataHeader), bytes.size());
    PacketView<hci_spec::ACLDataHeader> packet(&bytes,
                                               bytes.size() - sizeof(hci_spec::ACLDataHeader));
    hci_spec::ConnectionHandle connection_handle =
        le16toh(packet.header().handle_and_flags) & 0xFFF;

    // LE is still waiting for controller credits
    EXPECT_EQ(kHandle1, connection_handle);

    if ((packet_count == 0) || (packet_count == 1)) {
      // The first low-priority queued packet and its continuation packet were dropped so the first
      // packets actually sent should be those for the second PDU
      EXPECT_EQ(1u, packet.payload_data()[0]);
    }

    packet_count++;
  };
  test_device()->SetDataCallback(data_callback, dispatcher());

  // Send enough data that the first PDU sent in this loop gets dropped
  for (size_t i = 0; i < AclDataChannel::kMaxAclPacketsPerChannel + 1; ++i) {
    // Send two fragments per PDU
    std::list<ACLDataPacketPtr> packets;
    for (auto pbf : {hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                     hci_spec::ACLPacketBoundaryFlag::kContinuingFragment}) {
      auto packet =
          ACLDataPacket::New(kHandle1, pbf, hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMtu);

      // Write a sequence number into the payload, starting at 0
      packet->mutable_view()->mutable_payload_data()[0] = static_cast<uint8_t>(i);
      packets.push_back(std::move(packet));
    }
    EXPECT_TRUE(acl_data_channel()->SendPackets(std::move(packets), l2cap::kFirstDynamicChannelId,
                                                AclDataChannel::PacketPriority::kLow));
  }

  test_device()->SendCommandChannelPacket(bt::testing::NumberOfCompletedPacketsPacket(kHandle1, 2));
  RunLoopUntilIdle();

  EXPECT_EQ(2u, packet_count);

#ifndef NINSPECT
  using namespace ::inspect::testing;
  auto adc_matcher = NodeMatches(
      PropertyList(AllOf(Contains(UintIs("num_overflow_packets", packet_count)),
                         Contains(UintIs("num_recent_overflow_packets", packet_count)))));

  auto hierarchy = inspect::ReadFromVmo(inspector.DuplicateVmo()).take_value();
  EXPECT_THAT(hierarchy, ChildrenMatch(ElementsAre(adc_matcher)));
#endif  // NINSPECT
}

TEST_F(ACLDataChannelTest,
       SendingLowPriorityPacketsThatDropDoNotAffectDataOnSameLinkDifferentChannel) {
  constexpr size_t kMaxMtu = 4;
  constexpr size_t kMaxNumPackets = 2;
  constexpr hci_spec::ConnectionHandle kHandle0 = 0x0001;
  constexpr UniqueChannelId kChannelId0 = l2cap::kFirstDynamicChannelId;
  constexpr UniqueChannelId kChannelId1 = l2cap::kFirstDynamicChannelId + 1;

  InitializeACLDataChannel(DataBufferInfo(kMaxMtu, kMaxNumPackets));

  acl_data_channel()->RegisterLink(kHandle0, bt::LinkType::kACL);

  // Fill up controller buffers
  for (size_t i = 0; i < kMaxNumPackets; ++i) {
    auto packet = ACLDataPacket::New(kHandle0, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMtu);
    EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kFirstDynamicChannelId,
                                               AclDataChannel::PacketPriority::kLow));
  }
  RunLoopUntilIdle();

  // Callback invoked by TestDevice when it receive a data packet from us.
  size_t packet_count = 0;
  auto data_callback = [&](const ByteBuffer& bytes) {
    ASSERT_LT(sizeof(hci_spec::ACLDataHeader), bytes.size());
    PacketView<hci_spec::ACLDataHeader> packet(&bytes,
                                               bytes.size() - sizeof(hci_spec::ACLDataHeader));

    // The first packet should be left in the queue because it is for kChannelId0 but the second
    // should be dropped as it was the least recently sent unsent packet for channel kChannelId1.
    if (packet_count == 0) {
      EXPECT_EQ(0u, packet.payload_data()[0]);
    } else if (packet_count == 1) {
      EXPECT_EQ(2u, packet.payload_data()[0]);
    }

    packet_count++;
  };
  test_device()->SetDataCallback(data_callback, dispatcher());

  // Send one packet on kChannelId0
  {
    auto packet = ACLDataPacket::New(kHandle0, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMtu);

    // Write a sequence number into the payload, starting at 0
    packet->mutable_view()->mutable_payload_data()[0] = static_cast<uint8_t>(0);
    EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), kChannelId0,
                                               AclDataChannel::PacketPriority::kLow));
  }

  // Send enough data on kChannel1 that the first PDU sent in this loop gets dropped
  for (size_t i = 0; i < AclDataChannel::kMaxAclPacketsPerChannel + 1; i++) {
    auto packet = ACLDataPacket::New(kHandle0, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMtu);

    // Write a sequence number into the payload, starting at 0
    packet->mutable_view()->mutable_payload_data()[0] = static_cast<uint8_t>(i + 1);
    EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), kChannelId1,
                                               AclDataChannel::PacketPriority::kLow));
  }

  test_device()->SendCommandChannelPacket(bt::testing::NumberOfCompletedPacketsPacket(kHandle0, 2));
  RunLoopUntilIdle();

  EXPECT_EQ(2u, packet_count);
}

TEST_F(ACLDataChannelTest, SendingLowPriorityPacketsThatDropDoNotAffectDataOnDifferentLink) {
  constexpr size_t kMaxMtu = 4;
  constexpr size_t kMaxNumPackets = 2;
  constexpr hci_spec::ConnectionHandle kHandle0 = 0x0001;
  constexpr hci_spec::ConnectionHandle kHandle1 = 0x0002;

  InitializeACLDataChannel(DataBufferInfo(kMaxMtu, kMaxNumPackets));

  acl_data_channel()->RegisterLink(kHandle0, bt::LinkType::kACL);
  acl_data_channel()->RegisterLink(kHandle1, bt::LinkType::kACL);

  // Fill up controller buffers
  for (size_t i = 0; i < kMaxNumPackets; ++i) {
    auto packet = ACLDataPacket::New(kHandle0, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMtu);
    EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kFirstDynamicChannelId,
                                               AclDataChannel::PacketPriority::kLow));
  }
  RunLoopUntilIdle();

  // Callback invoked by TestDevice when it receive a data packet from us.
  size_t packet_count = 0;
  auto data_callback = [&](const ByteBuffer& bytes) {
    ASSERT_LT(sizeof(hci_spec::ACLDataHeader), bytes.size());
    PacketView<hci_spec::ACLDataHeader> packet(&bytes,
                                               bytes.size() - sizeof(hci_spec::ACLDataHeader));
    hci_spec::ConnectionHandle connection_handle =
        le16toh(packet.header().handle_and_flags) & 0xFFF;

    // First packet on kHandle0 doesn't get dropped, but first packet on kHandle1 does get dropped
    // because there are too many queued for that channel on that link.
    if (packet_count == 0) {
      EXPECT_EQ(kHandle0, connection_handle);
      EXPECT_EQ(0u, packet.payload_data()[0]);
    } else if (packet_count == 1) {
      EXPECT_EQ(kHandle1, connection_handle);
      EXPECT_EQ(2u, packet.payload_data()[0]);
    }
    packet_count++;
  };
  test_device()->SetDataCallback(data_callback, dispatcher());

  // Send one data packet on kHandle0
  {
    auto packet = ACLDataPacket::New(kHandle0, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMtu);

    // Write a sequence number into the payload, starting at 0
    packet->mutable_view()->mutable_payload_data()[0] = static_cast<uint8_t>(0);
    EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kFirstDynamicChannelId,
                                               AclDataChannel::PacketPriority::kLow));
  }

  // Send enough data on kHandle1 that the first PDU sent in this loop gets dropped
  for (size_t i = 0; i < AclDataChannel::kMaxAclPacketsPerChannel + 1; i++) {
    auto packet = ACLDataPacket::New(kHandle1, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMtu);

    // Write a sequence number into the payload, starting at 0
    packet->mutable_view()->mutable_payload_data()[0] = static_cast<uint8_t>(i + 1);
    EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kFirstDynamicChannelId,
                                               AclDataChannel::PacketPriority::kLow));
  }

  test_device()->SendCommandChannelPacket(bt::testing::NumberOfCompletedPacketsPacket(kHandle0, 2));
  RunLoopUntilIdle();

  EXPECT_EQ(2u, packet_count);
}

TEST_F(ACLDataChannelTest, QueuedAclAndLePacketsAreSentUsingSeparateBufferCounts) {
  constexpr size_t kMaxMtu = 4;
  constexpr size_t kMaxNumPackets = 2;
  constexpr hci_spec::ConnectionHandle kHandle0 = 0x0001;
  constexpr hci_spec::ConnectionHandle kHandle1 = 0x0002;

  InitializeACLDataChannel(DataBufferInfo(kMaxMtu, kMaxNumPackets),
                           DataBufferInfo(kMaxMtu, kMaxNumPackets));

  acl_data_channel()->RegisterLink(kHandle0, bt::LinkType::kLE);
  acl_data_channel()->RegisterLink(kHandle1, bt::LinkType::kACL);

  // Fill up both LE and BR/EDR controller buffers, leaving one additional packet in the queue of
  // each type
  for (hci_spec::ConnectionHandle handle : {kHandle0, kHandle1}) {
    for (size_t i = 0; i < kMaxNumPackets + 1; ++i) {
      auto packet = ACLDataPacket::New(handle, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                       hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMtu);
      EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kFirstDynamicChannelId,
                                                 AclDataChannel::PacketPriority::kLow));
    }
  }
  RunLoopUntilIdle();

  // Callback invoked by TestDevice when it receive a data packet from us.
  size_t packet_count = 0;
  test_device()->SetDataCallback([&](auto&) { packet_count++; }, dispatcher());

  StaticByteBuffer event_buffer(0x13, 0x09,  // Event header
                                0x02,        // Number of handles
                                LowerBits(kHandle0), UpperBits(kHandle0), LowerBits(uint16_t{1}),
                                UpperBits(uint16_t{1}),  // 1 packets on kHandle0
                                LowerBits(kHandle1), UpperBits(kHandle1), LowerBits(uint16_t{1}),
                                UpperBits(uint16_t{1})  // 1 packets on kHandle1
  );
  test_device()->SendCommandChannelPacket(event_buffer);
  RunLoopUntilIdle();

  EXPECT_EQ(2u, packet_count);
}

TEST_F(ACLDataChannelTest, NumberOfCompletedPacketsExceedsPendingPackets) {
  constexpr size_t kMaxMtu = 4;
  constexpr size_t kMaxNumPackets = 2;
  constexpr hci_spec::ConnectionHandle kHandle0 = 0x0001;

  InitializeACLDataChannel(DataBufferInfo(kMaxMtu, kMaxNumPackets),
                           DataBufferInfo(kMaxMtu, kMaxNumPackets));

  acl_data_channel()->RegisterLink(kHandle0, bt::LinkType::kACL);

  size_t packet_count = 0;
  test_device()->SetDataCallback([&](auto&) { packet_count++; }, dispatcher());

  // Fill up both controller buffer, leaving one additional packet in the queue.
  for (size_t i = 0; i < kMaxNumPackets + 1; ++i) {
    auto packet = ACLDataPacket::New(kHandle0, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMtu);
    EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kFirstDynamicChannelId,
                                               AclDataChannel::PacketPriority::kLow));
  }
  RunLoopUntilIdle();
  EXPECT_EQ(packet_count, kMaxNumPackets);

  test_device()->SendCommandChannelPacket(
      bt::testing::NumberOfCompletedPacketsPacket(kHandle0, kMaxNumPackets + 1));
  RunLoopUntilIdle();
  // The pending packet count should have been cleared, and the queued packet should have been sent.
  EXPECT_EQ(packet_count, kMaxNumPackets + 1);
}

#ifndef NINSPECT
TEST_F(ACLDataChannelTest, InspectHierarchyContainsOutboundQueueState) {
  constexpr size_t kMaxMtu = 4;
  constexpr size_t kMaxNumPackets = 2;
  constexpr hci_spec::ConnectionHandle kHandle0 = 0x0001;
  constexpr hci_spec::ConnectionHandle kHandle1 = 0x0002;

  InitializeACLDataChannel(DataBufferInfo(kMaxMtu, kMaxNumPackets),
                           DataBufferInfo(kMaxMtu, kMaxNumPackets));

  acl_data_channel()->RegisterLink(kHandle0, bt::LinkType::kLE);
  acl_data_channel()->RegisterLink(kHandle1, bt::LinkType::kACL);

  // Fill up both LE and BR/EDR controller buffers, leaving one additional packet in the queue of
  // each type
  for (hci_spec::ConnectionHandle handle : {kHandle0, kHandle1}) {
    for (size_t i = 0; i < kMaxNumPackets + 1; ++i) {
      auto packet = ACLDataPacket::New(handle, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                       hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMtu);
      EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kFirstDynamicChannelId,
                                                 AclDataChannel::PacketPriority::kLow));
    }
  }
  RunLoopUntilIdle();

  inspect::Inspector inspector;
  const std::string kNodeName = "adc_node_name";
  acl_data_channel()->AttachInspect(inspector.GetRoot(), kNodeName);

  using namespace ::inspect::testing;
  auto bredr_matcher = NodeMatches(AllOf(
      NameMatches("bredr"), PropertyList(ElementsAre(UintIs("num_sent_packets", kMaxNumPackets)))));

  auto le_matcher = NodeMatches(
      AllOf(NameMatches("le"),
            PropertyList(UnorderedElementsAre(UintIs("num_sent_packets", kMaxNumPackets),
                                              BoolIs("independent_from_bredr", true)))));

  auto send_latency_matcher =
      NodeMatches(AllOf(NameMatches("send_latency"),
                        PropertyList(UnorderedElementsAre(IntIs("50th_percentile_us", 0),
                                                          IntIs("95th_percentile_us", 0),
                                                          IntIs("99th_percentile_us", 0)))));

  auto send_size_matcher =
      NodeMatches(AllOf(NameMatches("send_size"),
                        PropertyList(UnorderedElementsAre(UintIs("10th_percentile_bytes", 0),
                                                          UintIs("50th_percentile_bytes", 0),
                                                          UintIs("90th_percentile_bytes", 0)))));

  auto metrics_matcher =
      AllOf(NodeMatches(NameMatches("metrics")),
            ChildrenMatch(UnorderedElementsAre(send_latency_matcher, send_size_matcher)));

  auto adc_matcher = AllOf(
      NodeMatches(AllOf(NameMatches(kNodeName),
                        PropertyList(UnorderedElementsAre(
                            UintIs("num_queued_packets", 2), UintIs("num_overflow_packets", 0),
                            UintIs("num_recent_overflow_packets", 0))))),
      ChildrenMatch(UnorderedElementsAre(bredr_matcher, le_matcher, metrics_matcher)));

  auto hierarchy = inspect::ReadFromVmo(inspector.DuplicateVmo()).take_value();
  EXPECT_THAT(hierarchy, ChildrenMatch(ElementsAre(adc_matcher)));
}
#endif  // NINSPECT

#ifndef NINSPECT
TEST_F(ACLDataChannelTest, SendingPacketsUpdatesSendMetrics) {
  constexpr size_t kMaxMtu = 4;
  constexpr size_t kMaxNumPackets = 2;
  constexpr hci_spec::ConnectionHandle kHandle0 = 0x0001;

  InitializeACLDataChannel(DataBufferInfo(kMaxMtu, kMaxNumPackets));

  inspect::Inspector inspector;
  acl_data_channel()->AttachInspect(inspector.GetRoot(), AclDataChannel::kInspectNodeName);

  acl_data_channel()->RegisterLink(kHandle0, bt::LinkType::kACL);

  // Fill up controller buffers
  for (size_t i = 0; i < kMaxNumPackets; ++i) {
    auto packet = ACLDataPacket::New(kHandle0, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMtu);
    ASSERT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kFirstDynamicChannelId,
                                               AclDataChannel::PacketPriority::kLow));
  }
  RunLoopUntilIdle();

  // Send enough data that metrics are exported
  constexpr size_t kNumTestPackets = 256;
  constexpr zx::duration kSendLatency = zx::msec(1);
  for (size_t i = 0; i < kNumTestPackets; ++i) {
    auto packet = ACLDataPacket::New(kHandle0, hci_spec::ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     hci_spec::ACLBroadcastFlag::kPointToPoint, kMaxMtu);
    ASSERT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kFirstDynamicChannelId,
                                               AclDataChannel::PacketPriority::kLow));
    RunLoopFor(kSendLatency);
    test_device()->SendCommandChannelPacket(
        bt::testing::NumberOfCompletedPacketsPacket(kHandle0, 1));
    RunLoopUntilIdle();
  }

  // Wait for the metric writing task to catch up
  RunLoopRepeatedlyFor(zx::min(1));

  inspect::Hierarchy hierarchy = bt::testing::ReadInspect(inspector);
  const inspect::Hierarchy* const send_latency_node =
      hierarchy.GetByPath({AclDataChannel::kInspectNodeName, "metrics", "send_latency"});
  ASSERT_TRUE(send_latency_node);
  const auto* const send_latency_median_wrapped =
      send_latency_node->node().get_property<inspect::IntPropertyValue>("50th_percentile_us");
  ASSERT_TRUE(send_latency_median_wrapped);
  EXPECT_EQ(kSendLatency.to_usecs(), send_latency_median_wrapped->value());

  const inspect::Hierarchy* const send_size_node =
      hierarchy.GetByPath({AclDataChannel::kInspectNodeName, "metrics", "send_size"});
  ASSERT_TRUE(send_size_node);
  const auto* const send_size_median_wrapped =
      send_size_node->node().get_property<inspect::UintPropertyValue>("50th_percentile_bytes");
  ASSERT_TRUE(send_size_median_wrapped);
  EXPECT_EQ(kMaxMtu, send_size_median_wrapped->value());
}
#endif  // NINSPECT

}  // namespace
}  // namespace bt::hci
