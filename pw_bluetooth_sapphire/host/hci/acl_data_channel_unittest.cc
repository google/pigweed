// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/hci/acl_data_channel.h"

#include <lib/async/cpp/task.h>
#include <zircon/assert.h>

#include <unordered_map>

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"
#include "src/connectivity/bluetooth/core/bt-host/common/test_helpers.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/defaults.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/transport.h"
#include "src/connectivity/bluetooth/core/bt-host/l2cap/l2cap.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_controller.h"

namespace bt {
namespace hci {
namespace {

using TestingBase = bt::testing::FakeControllerTest<bt::testing::TestController>;

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
  DISALLOW_COPY_AND_ASSIGN_ALLOW_MOVE(ACLDataChannelTest);
};

using HCI_ACLDataChannelTest = ACLDataChannelTest;

TEST_F(HCI_ACLDataChannelTest, VerifyMTUs) {
  const DataBufferInfo kBREDRBufferInfo(1024, 50);
  const DataBufferInfo kLEBufferInfo(64, 16);

  // BR/EDR buffer only.
  InitializeACLDataChannel(kBREDRBufferInfo, DataBufferInfo());
  EXPECT_EQ(kBREDRBufferInfo, acl_data_channel()->GetBufferInfo());
  EXPECT_EQ(kBREDRBufferInfo, acl_data_channel()->GetLEBufferInfo());

  TearDown();
  SetUp();

  // LE buffer only.
  InitializeACLDataChannel(DataBufferInfo(), kLEBufferInfo);
  EXPECT_EQ(DataBufferInfo(), acl_data_channel()->GetBufferInfo());
  EXPECT_EQ(kLEBufferInfo, acl_data_channel()->GetLEBufferInfo());

  TearDown();
  SetUp();

  // Both buffers available.
  InitializeACLDataChannel(kBREDRBufferInfo, kLEBufferInfo);
  EXPECT_EQ(kBREDRBufferInfo, acl_data_channel()->GetBufferInfo());
  EXPECT_EQ(kLEBufferInfo, acl_data_channel()->GetLEBufferInfo());
}

// Test that SendPacket works using only the BR/EDR buffer.
TEST_F(HCI_ACLDataChannelTest, SendPacketBREDRBuffer) {
  constexpr size_t kMaxMTU = 5;
  constexpr size_t kMaxNumPackets = 5;
  constexpr ConnectionHandle kHandle0 = 0x0001;
  constexpr ConnectionHandle kHandle1 = 0x0002;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle0, Connection::LinkType::kLE);
  acl_data_channel()->RegisterLink(kHandle1, Connection::LinkType::kACL);

  int handle0_packet_count = 0;
  int handle1_packet_count = 0;

  // Callback invoked by TestDevice when it receive a data packet from us.
  auto data_callback = [&](const ByteBuffer& bytes) {
    ZX_DEBUG_ASSERT(bytes.size() >= sizeof(ACLDataHeader));

    PacketView<hci::ACLDataHeader> packet(&bytes, bytes.size() - sizeof(ACLDataHeader));
    ConnectionHandle connection_handle = le16toh(packet.header().handle_and_flags) & 0xFFF;

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
      ConnectionHandle handle = (i % 2) ? kHandle1 : kHandle0;
      auto packet = ACLDataPacket::New(handle, ACLPacketBoundaryFlag::kFirstNonFlushable,
                                       ACLBroadcastFlag::kPointToPoint, kMaxMTU);
      EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kInvalidChannelId));
    }
  });

  RunLoopUntilIdle();

  // kMaxNumPackets is 5. The controller should have received 3 packets on
  // kHandle0 and 2 on kHandle1
  EXPECT_EQ(3, handle0_packet_count);
  EXPECT_EQ(2, handle1_packet_count);

  // Notify the processed packets with a Number Of Completed Packet HCI event.
  auto event_buffer = CreateStaticByteBuffer(0x13, 0x09,              // Event header
                                             0x02,                    // Number of handles
                                             0x01, 0x00, 0x03, 0x00,  // 3 packets on handle 0x0001
                                             0x02, 0x00, 0x02, 0x00   // 2 packets on handle 0x0002
  );
  test_device()->SendCommandChannelPacket(event_buffer);

  RunLoopUntilIdle();

  EXPECT_EQ(5, handle0_packet_count);
  EXPECT_EQ(5, handle1_packet_count);
}

// Test that SendPacket works using the LE buffer when no BR/EDR buffer is
// available.
TEST_F(HCI_ACLDataChannelTest, SendPacketLEBuffer) {
  constexpr size_t kMaxMTU = 5;
  constexpr size_t kTotalAttempts = 12;
  constexpr size_t kTotalExpected = 6;
  constexpr size_t kBufferNumPackets = 3;
  constexpr ConnectionHandle kHandle0 = 0x0001;
  constexpr ConnectionHandle kHandle1 = 0x0002;

  InitializeACLDataChannel(DataBufferInfo(), DataBufferInfo(kMaxMTU, kBufferNumPackets));

  acl_data_channel()->RegisterLink(kHandle0, Connection::LinkType::kLE);
  acl_data_channel()->RegisterLink(kHandle1, Connection::LinkType::kACL);

  // This should fail because the payload exceeds the MTU.
  auto packet = ACLDataPacket::New(kHandle1, ACLPacketBoundaryFlag::kFirstNonFlushable,
                                   ACLBroadcastFlag::kPointToPoint, kMaxMTU + 1);
  EXPECT_FALSE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kInvalidChannelId));

  size_t handle0_packet_count = 0;
  size_t handle1_packet_count = 0;
  auto data_callback = [&](const ByteBuffer& bytes) {
    ZX_DEBUG_ASSERT(bytes.size() >= sizeof(ACLDataHeader));

    PacketView<hci::ACLDataHeader> packet(&bytes, bytes.size() - sizeof(ACLDataHeader));
    ConnectionHandle connection_handle = le16toh(packet.header().handle_and_flags) & 0xFFF;

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
      ConnectionHandle handle = (i % 2) ? kHandle1 : kHandle0;
      auto packet = ACLDataPacket::New(handle, ACLPacketBoundaryFlag::kFirstNonFlushable,
                                       ACLBroadcastFlag::kPointToPoint, kMaxMTU);
      if (i % 2) {
        // ACL-U packets should fail due to 0 MTU size.
        EXPECT_FALSE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kInvalidChannelId));
      } else {
        EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kInvalidChannelId));
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
  auto event_buffer = CreateStaticByteBuffer(0x13, 0x05,  // Event header
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
TEST_F(HCI_ACLDataChannelTest, SendLEPacketBothBuffers) {
  constexpr size_t kMaxMTU = 200;
  constexpr size_t kMaxNumPackets = 50;
  constexpr size_t kLEMaxMTU = 5;
  constexpr size_t kLEMaxNumPackets = 5;
  constexpr ConnectionHandle kHandle0 = 0x0001;
  constexpr ConnectionHandle kHandle1 = 0x0002;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets),
                           DataBufferInfo(kLEMaxMTU, kLEMaxNumPackets));

  acl_data_channel()->RegisterLink(kHandle0, Connection::LinkType::kLE);
  acl_data_channel()->RegisterLink(kHandle1, Connection::LinkType::kLE);

  // This should fail because the payload exceeds the LE MTU.
  auto packet = ACLDataPacket::New(kHandle1, ACLPacketBoundaryFlag::kFirstNonFlushable,
                                   ACLBroadcastFlag::kPointToPoint, kMaxMTU);
  EXPECT_FALSE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kInvalidChannelId));

  int handle0_packet_count = 0;
  int handle1_packet_count = 0;
  auto data_callback = [&](const ByteBuffer& bytes) {
    ZX_DEBUG_ASSERT(bytes.size() >= sizeof(ACLDataHeader));

    PacketView<hci::ACLDataHeader> packet(&bytes, bytes.size() - sizeof(ACLDataHeader));
    ConnectionHandle connection_handle = le16toh(packet.header().handle_and_flags) & 0xFFF;

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
      ConnectionHandle handle = (i % 2) ? kHandle1 : kHandle0;
      auto packet = ACLDataPacket::New(handle, ACLPacketBoundaryFlag::kFirstNonFlushable,
                                       ACLBroadcastFlag::kPointToPoint, kLEMaxMTU);
      EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kInvalidChannelId));
    }
  });

  RunLoopUntilIdle();

  // ACLDataChannel should be looking at kLEMaxNumPackets, which is 5. The
  // controller should have received 3 packets on kHandle0 and 2 on kHandle1
  EXPECT_EQ(3, handle0_packet_count);
  EXPECT_EQ(2, handle1_packet_count);

  // Notify the processed packets with a Number Of Completed Packet HCI event.
  auto event_buffer = CreateStaticByteBuffer(0x13, 0x09,              // Event header
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
TEST_F(HCI_ACLDataChannelTest, SendBREDRPacketBothBuffers) {
  constexpr size_t kLEMaxMTU = 200;
  constexpr size_t kLEMaxNumPackets = 50;
  constexpr size_t kMaxMTU = 5;
  constexpr size_t kMaxNumPackets = 5;
  constexpr ConnectionHandle kHandle0 = 0x0001;
  constexpr ConnectionHandle kHandle1 = 0x0002;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets),
                           DataBufferInfo(kLEMaxMTU, kLEMaxNumPackets));

  acl_data_channel()->RegisterLink(kHandle0, Connection::LinkType::kACL);
  acl_data_channel()->RegisterLink(kHandle1, Connection::LinkType::kACL);

  // This should fail because the payload exceeds the ACL MTU.
  auto packet = ACLDataPacket::New(kHandle1, ACLPacketBoundaryFlag::kFirstNonFlushable,
                                   ACLBroadcastFlag::kPointToPoint, kLEMaxMTU);
  EXPECT_FALSE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kInvalidChannelId));

  int handle0_packet_count = 0;
  int handle1_packet_count = 0;
  auto data_callback = [&](const ByteBuffer& bytes) {
    ZX_DEBUG_ASSERT(bytes.size() >= sizeof(ACLDataHeader));

    PacketView<hci::ACLDataHeader> packet(&bytes, bytes.size() - sizeof(ACLDataHeader));
    ConnectionHandle connection_handle = le16toh(packet.header().handle_and_flags) & 0xFFF;

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
      ConnectionHandle handle = (i % 2) ? kHandle1 : kHandle0;
      auto packet = ACLDataPacket::New(handle, ACLPacketBoundaryFlag::kFirstNonFlushable,
                                       ACLBroadcastFlag::kPointToPoint, kMaxMTU);
      EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kInvalidChannelId));
    }
  });

  RunLoopUntilIdle();

  // ACLDataChannel should be looking at kLEMaxNumPackets, which is 5. The
  // controller should have received 3 packets on kHandle0 and 2 on kHandle1
  EXPECT_EQ(3, handle0_packet_count);
  EXPECT_EQ(2, handle1_packet_count);

  // Notify the processed packets with a Number Of Completed Packet HCI event.
  auto event_buffer = CreateStaticByteBuffer(0x13, 0x09,              // Event header
                                             0x02,                    // Number of handles
                                             0x01, 0x00, 0x03, 0x00,  // 3 packets on handle 0x0001
                                             0x02, 0x00, 0x02, 0x00   // 2 packets on handle 0x0002
  );
  test_device()->SendCommandChannelPacket(event_buffer);

  RunLoopUntilIdle();

  EXPECT_EQ(5, handle0_packet_count);
  EXPECT_EQ(5, handle1_packet_count);
}

TEST_F(HCI_ACLDataChannelTest, SendPacketFromMultipleThreads) {
  constexpr size_t kMaxMTU = 1;
  constexpr size_t kMaxNumPackets = 1;
  constexpr size_t kLEMaxMTU = 5;
  constexpr size_t kLEMaxNumPackets = 5;

  constexpr ConnectionHandle kHandle0 = 0x0001;
  constexpr ConnectionHandle kHandle1 = 0x0002;
  constexpr ConnectionHandle kHandle2 = 0x0003;

  constexpr int kExpectedTotalPacketCount = 18;

  int handle0_total_packet_count = 0;
  int handle1_total_packet_count = 0;
  int handle2_total_packet_count = 0;
  int handle0_processed_count = 0;
  int handle1_processed_count = 0;
  int handle2_processed_count = 0;
  int total_packet_count = 0;
  auto data_cb = [&](const ByteBuffer& bytes) {
    ZX_DEBUG_ASSERT(bytes.size() >= sizeof(ACLDataHeader));

    PacketView<hci::ACLDataHeader> packet(&bytes, bytes.size() - sizeof(ACLDataHeader));
    ConnectionHandle connection_handle = le16toh(packet.header().handle_and_flags) & 0xFFF;

    if (connection_handle == kHandle0) {
      handle0_total_packet_count++;
      handle0_processed_count++;
    } else if (connection_handle == kHandle1) {
      handle1_total_packet_count++;
      handle1_processed_count++;
    } else {
      ASSERT_EQ(kHandle2, connection_handle);
      handle2_total_packet_count++;
      handle2_processed_count++;
    }
    total_packet_count++;

    // For every kLEMaxNumPackets packets processed, we notify the host.
    if ((total_packet_count % kLEMaxNumPackets) == 0) {
      // NOTE(armansito): Here we include handles even when the processed-count
      // is 0. This is OK; ACLDataChannel should ignore those.
      auto event_buffer =
          CreateStaticByteBuffer(0x13, 0x0D,  // Event header
                                 0x03,        // Number of handles

                                 // handle 0x0001
                                 0x01, 0x00, static_cast<uint8_t>(handle0_processed_count), 0x00,
                                 // handle 0x0002
                                 0x02, 0x00, static_cast<uint8_t>(handle1_processed_count), 0x00,
                                 // handle 0x0003
                                 0x03, 0x00, static_cast<uint8_t>(handle2_processed_count), 0x00);
      handle0_processed_count = 0;
      handle1_processed_count = 0;
      handle2_processed_count = 0;
      test_device()->SendCommandChannelPacket(event_buffer);
    }
  };
  test_device()->SetDataCallback(data_cb, dispatcher());

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets),
                           DataBufferInfo(kLEMaxMTU, kLEMaxNumPackets));

  acl_data_channel()->RegisterLink(kHandle0, Connection::LinkType::kLE);
  acl_data_channel()->RegisterLink(kHandle1, Connection::LinkType::kLE);
  acl_data_channel()->RegisterLink(kHandle2, Connection::LinkType::kLE);

  // On 3 threads (for each connection handle) we each send 6 packets up to a
  // total of 18.
  auto thread_func = [this](ConnectionHandle handle) {
    for (int i = 0; i < kExpectedTotalPacketCount / 3; ++i) {
      auto packet = ACLDataPacket::New(handle, ACLPacketBoundaryFlag::kFirstNonFlushable,
                                       ACLBroadcastFlag::kPointToPoint, kLEMaxMTU);
      EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kInvalidChannelId));
    }
  };

  auto t1 = std::thread(thread_func, kHandle0);
  auto t2 = std::thread(thread_func, kHandle1);
  auto t3 = std::thread(thread_func, kHandle2);

  if (t1.joinable())
    t1.join();
  if (t2.joinable())
    t2.join();
  if (t3.joinable())
    t3.join();

  // Messages are sent on another thread, so wait here until they arrive.
  RunLoopUntilIdle();

  EXPECT_EQ(kExpectedTotalPacketCount / 3, handle0_total_packet_count);
  EXPECT_EQ(kExpectedTotalPacketCount / 3, handle1_total_packet_count);
  EXPECT_EQ(kExpectedTotalPacketCount / 3, handle2_total_packet_count);
  EXPECT_EQ(kExpectedTotalPacketCount, total_packet_count);
}

TEST_F(HCI_ACLDataChannelTest, SendPacketsFailure) {
  constexpr size_t kMaxMTU = 5;
  constexpr ConnectionHandle kHandle = 0x0001;
  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, 100), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle, Connection::LinkType::kACL);

  // Empty packet list.
  EXPECT_FALSE(
      acl_data_channel()->SendPackets(LinkedList<ACLDataPacket>(), l2cap::kInvalidChannelId));

  // Packet exceeds MTU
  LinkedList<ACLDataPacket> packets;
  packets.push_back(ACLDataPacket::New(kHandle, ACLPacketBoundaryFlag::kFirstNonFlushable,
                                       ACLBroadcastFlag::kPointToPoint, kMaxMTU + 1));
  EXPECT_FALSE(acl_data_channel()->SendPackets(std::move(packets), l2cap::kInvalidChannelId));
}

// Tests sending multiple packets in a single call.
TEST_F(HCI_ACLDataChannelTest, SendPackets) {
  constexpr int kExpectedPacketCount = 5;
  constexpr ConnectionHandle kHandle = 0x0001;

  InitializeACLDataChannel(DataBufferInfo(1024, 100), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle, Connection::LinkType::kLE);

  bool pass = true;
  int seq_no = 0;
  auto data_cb = [&pass, &seq_no](const ByteBuffer& bytes) {
    ZX_DEBUG_ASSERT(bytes.size() >= sizeof(ACLDataHeader));
    PacketView<hci::ACLDataHeader> packet(&bytes, bytes.size() - sizeof(ACLDataHeader));
    EXPECT_EQ(1u, packet.payload_size());

    int cur_no = packet.payload_bytes()[0];
    if (cur_no != seq_no + 1) {
      pass = false;
      return;
    }

    seq_no = cur_no;
  };
  test_device()->SetDataCallback(data_cb, dispatcher());

  LinkedList<ACLDataPacket> packets;
  for (int i = 1; i <= kExpectedPacketCount; ++i) {
    auto packet = ACLDataPacket::New(kHandle, ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     ACLBroadcastFlag::kPointToPoint, 1);
    packet->mutable_view()->mutable_payload_bytes()[0] = i;
    packets.push_back(std::move(packet));
  }

  EXPECT_TRUE(acl_data_channel()->SendPackets(std::move(packets), l2cap::kInvalidChannelId));

  RunLoopUntilIdle();

  EXPECT_TRUE(pass);
  EXPECT_EQ(kExpectedPacketCount, seq_no);
}

// Test sending batches of packets atomically across multiple threads.
TEST_F(HCI_ACLDataChannelTest, SendPacketsAtomically) {
  constexpr size_t kThreadCount = 10;
  constexpr size_t kPacketsPerThread = 10;
  constexpr size_t kExpectedPacketCount = kThreadCount * kPacketsPerThread;
  constexpr ConnectionHandle kHandle = 0x0001;

  InitializeACLDataChannel(DataBufferInfo(1024, 100), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle, Connection::LinkType::kLE);

  std::vector<std::unique_ptr<ByteBuffer>> received;
  auto data_cb = [&received](const ByteBuffer& bytes) {
    ZX_DEBUG_ASSERT(bytes.size() >= sizeof(ACLDataHeader));
    received.push_back(std::make_unique<DynamicByteBuffer>(bytes));
  };
  test_device()->SetDataCallback(data_cb, dispatcher());

  // Each thread will send a sequence of kPacketsPerThread packets. The payload
  // of each packet encodes an integer
  LinkedList<ACLDataPacket> packets[kThreadCount];
  for (size_t i = 0; i < kThreadCount; ++i) {
    for (size_t j = 1; j <= kPacketsPerThread; ++j) {
      auto packet = ACLDataPacket::New(kHandle, ACLPacketBoundaryFlag::kFirstNonFlushable,
                                       ACLBroadcastFlag::kPointToPoint, 1);
      packet->mutable_view()->mutable_payload_bytes()[0] = j;
      packets[i].push_back(std::move(packet));
    }
  }

  // Send each packet sequence on a different thread and make sure that each
  // sequence arrives atomically.
  std::thread threads[kThreadCount];
  auto thread_func = [&packets, this](size_t i) {
    EXPECT_TRUE(acl_data_channel()->SendPackets(std::move(packets[i]), l2cap::kInvalidChannelId));
  };

  for (size_t i = 0; i < kThreadCount; ++i) {
    threads[i] = std::thread(thread_func, i);
  }

  // Wait until all threads have queued their packets.
  for (size_t i = 0; i < kThreadCount; ++i) {
    if (threads[i].joinable())
      threads[i].join();
  }

  // Messages are sent on another thread, so wait here until they arrive.
  RunLoopUntilIdle();

  ASSERT_EQ(kExpectedPacketCount, received.size());

  // Verify that the contents of |received| are in the correct sequence.
  for (size_t i = 0; i < kExpectedPacketCount; ++i) {
    PacketView<hci::ACLDataHeader> packet(received[i].get(),
                                          received[i]->size() - sizeof(ACLDataHeader));
    EXPECT_EQ(1u, packet.payload_size());
    EXPECT_EQ((i % kPacketsPerThread) + 1, packet.payload_bytes()[0]);
  }
}

TEST_F(HCI_ACLDataChannelTest,
       UnregisterLinkDoesNotClearNumSentPacketsAndClearControllerPacketCountDoes) {
  constexpr size_t kMaxMTU = 1024;
  constexpr size_t kMaxNumPackets = 2;
  constexpr ConnectionHandle kHandle1 = 1;
  constexpr ConnectionHandle kHandle2 = 2;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle1, Connection::LinkType::kLE);
  acl_data_channel()->RegisterLink(kHandle2, Connection::LinkType::kLE);

  int packet_count = 0;
  test_device()->SetDataCallback([&](const auto&) { packet_count++; }, dispatcher());

  // Send 3 packets on two links. This is enough to fill up the data buffers.
  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle1, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId));
  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle2, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId));
  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle1, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId));

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

TEST_F(HCI_ACLDataChannelTest, SendingPacketsOnUnregisteredLinkDropsPackets) {
  constexpr size_t kMaxMTU = 1024;
  constexpr size_t kMaxNumPackets = 2;
  constexpr ConnectionHandle kHandle = 1;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets), DataBufferInfo());

  int packet_count = 0;
  test_device()->SetDataCallback([&](const auto&) { packet_count++; }, dispatcher());

  // Send packet with unregistered handle.
  ASSERT_FALSE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId));

  RunLoopUntilIdle();

  // Packet should not have been sent.
  ASSERT_EQ(0, packet_count);

  // Now register link. Packet should have been dropped and should still
  // not be sent.
  acl_data_channel()->RegisterLink(kHandle, Connection::LinkType::kLE);

  RunLoopUntilIdle();

  // Packet should not have been sent.
  ASSERT_EQ(0, packet_count);

  // Unregister a link that has not been registered
  acl_data_channel()->UnregisterLink(kHandle);
  RunLoopUntilIdle();
  ASSERT_EQ(0, packet_count);
}

TEST_F(HCI_ACLDataChannelTest, UnregisterLinkClearsPendingPackets) {
  constexpr size_t kMaxMTU = 1024;
  constexpr size_t kMaxNumPackets = 1;
  constexpr ConnectionHandle kHandle1 = 1;
  constexpr ConnectionHandle kHandle2 = 2;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle1, Connection::LinkType::kLE);
  acl_data_channel()->RegisterLink(kHandle2, Connection::LinkType::kLE);

  int handle1_packet_count = 0;
  int handle2_packet_count = 0;
  auto data_callback = [&](const ByteBuffer& bytes) {
    ZX_DEBUG_ASSERT(bytes.size() >= sizeof(ACLDataHeader));

    PacketView<hci::ACLDataHeader> packet(&bytes, bytes.size() - sizeof(ACLDataHeader));
    ConnectionHandle connection_handle = le16toh(packet.header().handle_and_flags) & 0xFFF;

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
      ACLDataPacket::New(kHandle1, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId));
  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle2, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId));
  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle1, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId));

  RunLoopUntilIdle();

  // Only kHandle1 packet should have been sent
  ASSERT_EQ(1, handle1_packet_count);
  ASSERT_EQ(0, handle2_packet_count);

  // Clear pending packet for |kHandle2|
  acl_data_channel()->UnregisterLink(kHandle2);

  // Notify the processed packet with a Number Of Completed Packet HCI event.
  auto event_buffer = CreateStaticByteBuffer(0x13, 0x05,             // Event header
                                             0x01,                   // Number of handles
                                             0x01, 0x00, 0x01, 0x00  // 1 packet on handle 0x0001
  );
  test_device()->SendCommandChannelPacket(event_buffer);

  RunLoopUntilIdle();

  // second |kHandle1| packet should have been sent
  ASSERT_EQ(2, handle1_packet_count);
  ASSERT_EQ(0, handle2_packet_count);
}

TEST_F(HCI_ACLDataChannelTest, PacketsQueuedByFlowControlAreNotSentAfterUnregisterLink) {
  constexpr size_t kMaxMTU = 1024;
  constexpr size_t kMaxNumPackets = 1;
  constexpr ConnectionHandle kHandle1 = 1;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle1, Connection::LinkType::kLE);

  int packet_count = 0;
  test_device()->SetDataCallback([&](const auto&) { packet_count++; }, dispatcher());

  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle1, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId));
  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle1, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId));

  RunLoopUntilIdle();

  // The second packet should have been queued.
  ASSERT_EQ(1, packet_count);

  // Clear the packet count for |kHandle2|. The second packet should NOT go out.
  acl_data_channel()->UnregisterLink(kHandle1);

  // Notify the processed packet with a Number Of Completed Packet HCI event.
  auto event_buffer = CreateStaticByteBuffer(0x13, 0x05,             // Event header
                                             0x01,                   // Number of handles
                                             0x01, 0x00, 0x01, 0x00  // 1 packet on handle 0x0001
  );
  test_device()->SendCommandChannelPacket(event_buffer);

  RunLoopUntilIdle();

  // The second packet should not have been sent
  ASSERT_EQ(1, packet_count);
}

TEST_F(HCI_ACLDataChannelTest,
       StalePacketsBufferedBeforeFirstUnregisterAndBeforeSecondRegisterAreNotSent) {
  constexpr size_t kMaxMTU = 1024;
  constexpr size_t kMaxNumPackets = 1;
  constexpr ConnectionHandle kHandle = 1;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle, Connection::LinkType::kLE);

  // Unique packet to send to re-registered link with same handle.
  const auto kPacket = CreateStaticByteBuffer(
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
      ACLDataPacket::New(kHandle, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId));
  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId));

  RunLoopUntilIdle();

  // The second packet should have been queued.
  ASSERT_EQ(1, data_cb_count);

  // Clear the packet count for |kHandle2|. The second packet should NOT go out.
  acl_data_channel()->UnregisterLink(kHandle);

  // Notify the processed packet with a Number Of Completed Packet HCI event.
  auto event_buffer = CreateStaticByteBuffer(0x13, 0x05,             // Event header
                                             0x01,                   // Number of handles
                                             0x01, 0x00, 0x01, 0x00  // 1 packet on handle 0x0001
  );
  test_device()->SendCommandChannelPacket(event_buffer);

  RunLoopUntilIdle();

  // The second packet should not have been sent
  ASSERT_EQ(1, data_cb_count);

  // Re-Register same link handle
  acl_data_channel()->RegisterLink(kHandle, Connection::LinkType::kLE);

  auto packet = ACLDataPacket::New(kHandle, ACLPacketBoundaryFlag::kFirstNonFlushable,
                                   ACLBroadcastFlag::kPointToPoint, 1);
  packet->mutable_view()->mutable_payload_data().Fill(1);

  ASSERT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kInvalidChannelId));

  RunLoopUntilIdle();

  // The new packet should have been sent to the TestController.
  ASSERT_EQ(2, data_cb_count);
}

TEST_F(HCI_ACLDataChannelTest, UnregisterLinkDropsFutureSentPackets) {
  constexpr size_t kMaxMTU = 1024;
  constexpr size_t kMaxNumPackets = 1;
  constexpr ConnectionHandle kHandle = 1;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle, Connection::LinkType::kLE);

  int packet_count = 0;
  test_device()->SetDataCallback([&](const auto&) { packet_count++; }, dispatcher());

  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId));

  RunLoopUntilIdle();
  ASSERT_EQ(1, packet_count);

  acl_data_channel()->UnregisterLink(kHandle);

  // attempt to send packet on unregistered link
  ASSERT_FALSE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, 1),
      l2cap::kInvalidChannelId));

  RunLoopUntilIdle();
  // second packet should not have been sent
  ASSERT_EQ(1, packet_count);
}

TEST_F(HCI_ACLDataChannelTest, ReceiveData) {
  constexpr size_t kMaxMTU = 5;
  constexpr size_t kMaxNumPackets = 5;

  // It doesn't matter what we set the buffer values to since we're testing
  // incoming packets.
  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets), DataBufferInfo());

  constexpr size_t kExpectedPacketCount = 2u;
  size_t num_rx_packets = 0u;
  ConnectionHandle packet0_handle;
  ConnectionHandle packet1_handle;
  auto data_rx_cb = [&](ACLDataPacketPtr packet) {
    num_rx_packets++;
    if (num_rx_packets == 1) {
      packet0_handle = packet->connection_handle();
    } else if (num_rx_packets == 2) {
      packet1_handle = packet->connection_handle();
    } else {
      ZX_PANIC("|num_rx_packets| has unexpected value: %zu", num_rx_packets);
    }
  };
  set_data_received_callback(std::move(data_rx_cb));

  // Malformed packet: smaller than the ACL header.
  auto invalid0 = CreateStaticByteBuffer(0x01, 0x00, 0x00);

  // Malformed packet: the payload size given in the header doesn't match the
  // actual payload size.
  auto invalid1 = CreateStaticByteBuffer(0x01, 0x00, 0x02, 0x00, 0x00);

  // Valid packet on handle 1.
  auto valid0 = CreateStaticByteBuffer(0x01, 0x00, 0x01, 0x00, 0x00);

  // Valid packet on handle 2.
  auto valid1 = CreateStaticByteBuffer(0x02, 0x00, 0x01, 0x00, 0x00);

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

TEST_F(HCI_ACLDataChannelTest, TransportClosedCallback) {
  InitializeACLDataChannel(DataBufferInfo(1u, 1u), DataBufferInfo(1u, 1u));

  bool closed_cb_called = false;
  auto closed_cb = [&closed_cb_called] { closed_cb_called = true; };
  transport()->SetTransportClosedCallback(closed_cb);

  async::PostTask(dispatcher(), [this] { test_device()->CloseACLDataChannel(); });
  RunLoopUntilIdle();
  EXPECT_TRUE(closed_cb_called);
}

TEST_F(HCI_ACLDataChannelTest, TransportClosedCallbackBothChannels) {
  InitializeACLDataChannel(DataBufferInfo(1u, 1u), DataBufferInfo(1u, 1u));

  int closed_cb_count = 0;
  auto closed_cb = [&closed_cb_count] { closed_cb_count++; };
  transport()->SetTransportClosedCallback(closed_cb);

  // We'll send closed events for both channels. The closed callback should get
  // invoked only once.
  async::PostTask(dispatcher(), [this] {
    test_device()->CloseACLDataChannel();
    test_device()->CloseCommandChannel();
  });

  RunLoopUntilIdle();
  EXPECT_EQ(1, closed_cb_count);
}

// Make sure that a HCI "Number of completed packets" event received after shut
// down does not cause a crash.
TEST_F(HCI_ACLDataChannelTest, HciEventReceivedAfterShutDown) {
  InitializeACLDataChannel(DataBufferInfo(1u, 1u), DataBufferInfo(1u, 1u));

  // Notify the processed packets with a Number Of Completed Packet HCI event.
  auto event_buffer = CreateStaticByteBuffer(0x13, 0x09,              // Event header
                                             0x02,                    // Number of handles
                                             0x01, 0x00, 0x03, 0x00,  // 3 packets on handle 0x0001
                                             0x02, 0x00, 0x02, 0x00   // 2 packets on handle 0x0002
  );

  // Shuts down ACLDataChannel and CommandChannel.
  DeleteTransport();

  test_device()->SendCommandChannelPacket(event_buffer);
  RunLoopUntilIdle();
}

TEST_F(HCI_ACLDataChannelTest, DropQueuedPacketsRemovesPacketsMatchingFilterFromQueue) {
  constexpr size_t kMaxMTU = 5;
  constexpr size_t kMaxNumPackets = 5;
  constexpr ConnectionHandle kHandle0 = 0x0001;
  constexpr ConnectionHandle kHandle1 = 0x0002;
  constexpr l2cap::ChannelId kChanId0 = 0x40;
  constexpr l2cap::ChannelId kChanId1 = 0x41;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle0, Connection::LinkType::kLE);
  acl_data_channel()->RegisterLink(kHandle1, Connection::LinkType::kACL);

  int handle0_packet_count = 0;
  int handle1_packet_count = 0;

  // Callback invoked by TestDevice when it receive a data packet from us.
  auto data_callback = [&](const ByteBuffer& bytes) {
    ZX_DEBUG_ASSERT(bytes.size() >= sizeof(ACLDataHeader));

    PacketView<hci::ACLDataHeader> packet(&bytes, bytes.size() - sizeof(ACLDataHeader));
    ConnectionHandle connection_handle = le16toh(packet.header().handle_and_flags) & 0xFFF;

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
    ConnectionHandle handle = (i % 2) ? kHandle1 : kHandle0;
    auto packet = ACLDataPacket::New(handle, ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     ACLBroadcastFlag::kPointToPoint, kMaxMTU);
    EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), (i % 2) ? kChanId1 : kChanId0));
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
  auto event_buffer = CreateStaticByteBuffer(0x13, 0x09,              // Event header
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

TEST_F(HCI_ACLDataChannelTest, HighPriorityPacketsQueuedAfterLowPriorityPacketsAreSentFirst) {
  constexpr size_t kMaxMTU = 5;
  constexpr size_t kMaxNumPackets = 5;
  constexpr ConnectionHandle kHandle0 = 0x0001;
  constexpr ConnectionHandle kHandle1 = 0x0002;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets), DataBufferInfo());

  acl_data_channel()->RegisterLink(kHandle0, Connection::LinkType::kACL);
  acl_data_channel()->RegisterLink(kHandle1, Connection::LinkType::kACL);

  size_t handle0_packet_count = 0;
  size_t handle1_packet_count = 0;

  // Callback invoked by TestDevice when it receive a data packet from us.
  auto data_callback = [&](const ByteBuffer& bytes) {
    ZX_DEBUG_ASSERT(bytes.size() >= sizeof(ACLDataHeader));

    PacketView<hci::ACLDataHeader> packet(&bytes, bytes.size() - sizeof(ACLDataHeader));
    ConnectionHandle connection_handle = le16toh(packet.header().handle_and_flags) & 0xFFF;

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
    auto packet = ACLDataPacket::New(kHandle0, ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     ACLBroadcastFlag::kPointToPoint, kMaxMTU);
    EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), l2cap::kFirstDynamicChannelId,
                                               ACLDataChannel::PacketPriority::kLow));
  }
  RunLoopUntilIdle();

  EXPECT_EQ(kMaxNumPackets, handle0_packet_count);
  handle0_packet_count = 0;

  // Queue up 10 packets in total, distributed among the two connection handles.
  for (size_t i = 0; i < 2 * kMaxNumPackets; ++i) {
    ConnectionHandle handle = (i % 2) ? kHandle1 : kHandle0;
    auto priority =
        (i % 2) ? ACLDataChannel::PacketPriority::kLow : ACLDataChannel::PacketPriority::kHigh;
    auto packet = ACLDataPacket::New(handle, ACLPacketBoundaryFlag::kFirstNonFlushable,
                                     ACLBroadcastFlag::kPointToPoint, kMaxMTU);
    EXPECT_TRUE(
        acl_data_channel()->SendPacket(std::move(packet), l2cap::kFirstDynamicChannelId, priority));
  }

  RunLoopUntilIdle();

  // No packets should have been sent because controller buffer is full.
  EXPECT_EQ(0u, handle0_packet_count);
  EXPECT_EQ(0u, handle1_packet_count);

  // Notify the processed packets with a Number Of Completed Packet HCI event.
  auto event_buffer = CreateStaticByteBuffer(0x13, 0x05,             // Event header
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

}  // namespace
}  // namespace hci
}  // namespace bt
