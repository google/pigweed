// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/hci/acl_data_channel.h"

#include <lib/async/cpp/task.h>
#include <zircon/assert.h>

#include <unordered_map>

#include "src/connectivity/bluetooth/core/bt-host/hci/connection.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/defaults.h"
#include "src/connectivity/bluetooth/core/bt-host/hci/transport.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/fake_controller_test.h"
#include "src/connectivity/bluetooth/core/bt-host/testing/test_controller.h"

namespace bt {
namespace hci {
namespace {

using TestingBase =
    bt::testing::FakeControllerTest<bt::testing::TestController>;

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

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets),
                           DataBufferInfo());

  int handle0_packet_count = 0;
  int handle1_packet_count = 0;

  // Callback invoked by TestDevice when it receive a data packet from us.
  auto data_callback = [&](const ByteBuffer& bytes) {
    ZX_DEBUG_ASSERT(bytes.size() >= sizeof(ACLDataHeader));

    PacketView<hci::ACLDataHeader> packet(&bytes,
                                          bytes.size() - sizeof(ACLDataHeader));
    ConnectionHandle connection_handle =
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
  async::PostTask(dispatcher(), [this, kHandle0, kHandle1] {
    for (int i = 0; i < 10; ++i) {
      ConnectionHandle handle = (i % 2) ? kHandle1 : kHandle0;
      Connection::LinkType ll_type =
          (i % 2) ? Connection::LinkType::kACL : Connection::LinkType::kLE;
      auto packet =
          ACLDataPacket::New(handle, ACLPacketBoundaryFlag::kFirstNonFlushable,
                             ACLBroadcastFlag::kPointToPoint, kMaxMTU);
      EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), ll_type));
    }
  });

  RunLoopUntilIdle();

  // kMaxNumPackets is 5. The controller should have received 3 packets on
  // kHandle0 and 2 on kHandle1
  EXPECT_EQ(3, handle0_packet_count);
  EXPECT_EQ(2, handle1_packet_count);

  // Notify the processed packets with a Number Of Completed Packet HCI event.
  auto event_buffer = CreateStaticByteBuffer(
      0x13, 0x09,              // Event header
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

  InitializeACLDataChannel(DataBufferInfo(),
                           DataBufferInfo(kMaxMTU, kBufferNumPackets));

  // This should fail because the payload exceeds the MTU.
  auto packet =
      ACLDataPacket::New(kHandle1, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, kMaxMTU + 1);
  EXPECT_FALSE(acl_data_channel()->SendPacket(std::move(packet),
                                              Connection::LinkType::kACL));

  size_t handle0_packet_count = 0;
  size_t handle1_packet_count = 0;
  auto data_callback = [&](const ByteBuffer& bytes) {
    ZX_DEBUG_ASSERT(bytes.size() >= sizeof(ACLDataHeader));

    PacketView<hci::ACLDataHeader> packet(&bytes,
                                          bytes.size() - sizeof(ACLDataHeader));
    ConnectionHandle connection_handle =
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
  async::PostTask(dispatcher(), [this, kHandle0, kHandle1] {
    for (size_t i = 0; i < kTotalAttempts; ++i) {
      ConnectionHandle handle = (i % 2) ? kHandle1 : kHandle0;
      auto packet =
          ACLDataPacket::New(handle, ACLPacketBoundaryFlag::kFirstNonFlushable,
                             ACLBroadcastFlag::kPointToPoint, kMaxMTU);
      if (i % 2) {
        // ACL-U packets should fail due to 0 MTU size.
        EXPECT_FALSE(acl_data_channel()->SendPacket(
            std::move(packet), Connection::LinkType::kACL));
      } else {
        EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet),
                                                   Connection::LinkType::kLE));
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

  // This should fail because the payload exceeds the LE MTU.
  auto packet =
      ACLDataPacket::New(kHandle1, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, kMaxMTU);
  EXPECT_FALSE(acl_data_channel()->SendPacket(std::move(packet),
                                              Connection::LinkType::kLE));

  int handle0_packet_count = 0;
  int handle1_packet_count = 0;
  auto data_callback = [&](const ByteBuffer& bytes) {
    ZX_DEBUG_ASSERT(bytes.size() >= sizeof(ACLDataHeader));

    PacketView<hci::ACLDataHeader> packet(&bytes,
                                          bytes.size() - sizeof(ACLDataHeader));
    ConnectionHandle connection_handle =
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
  async::PostTask(dispatcher(), [this, kHandle0, kHandle1] {
    for (int i = 0; i < 10; ++i) {
      ConnectionHandle handle = (i % 2) ? kHandle1 : kHandle0;
      Connection::LinkType ll_type =
          Connection::LinkType::kLE;  // Send LE packets only.
      auto packet =
          ACLDataPacket::New(handle, ACLPacketBoundaryFlag::kFirstNonFlushable,
                             ACLBroadcastFlag::kPointToPoint, kLEMaxMTU);
      EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), ll_type));
    }
  });

  RunLoopUntilIdle();

  // ACLDataChannel should be looking at kLEMaxNumPackets, which is 5. The
  // controller should have received 3 packets on kHandle0 and 2 on kHandle1
  EXPECT_EQ(3, handle0_packet_count);
  EXPECT_EQ(2, handle1_packet_count);

  // Notify the processed packets with a Number Of Completed Packet HCI event.
  auto event_buffer = CreateStaticByteBuffer(
      0x13, 0x09,              // Event header
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

  // This should fail because the payload exceeds the ACL MTU.
  auto packet =
      ACLDataPacket::New(kHandle1, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, kLEMaxMTU);
  EXPECT_FALSE(acl_data_channel()->SendPacket(std::move(packet),
                                              Connection::LinkType::kACL));

  int handle0_packet_count = 0;
  int handle1_packet_count = 0;
  auto data_callback = [&](const ByteBuffer& bytes) {
    ZX_DEBUG_ASSERT(bytes.size() >= sizeof(ACLDataHeader));

    PacketView<hci::ACLDataHeader> packet(&bytes,
                                          bytes.size() - sizeof(ACLDataHeader));
    ConnectionHandle connection_handle =
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
  async::PostTask(dispatcher(), [this, kHandle0, kHandle1] {
    for (int i = 0; i < 10; ++i) {
      ConnectionHandle handle = (i % 2) ? kHandle1 : kHandle0;
      Connection::LinkType ll_type =
          Connection::LinkType::kACL;  // Send BR/EDR packets only.
      auto packet =
          ACLDataPacket::New(handle, ACLPacketBoundaryFlag::kFirstNonFlushable,
                             ACLBroadcastFlag::kPointToPoint, kMaxMTU);
      EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet), ll_type));
    }
  });

  RunLoopUntilIdle();

  // ACLDataChannel should be looking at kLEMaxNumPackets, which is 5. The
  // controller should have received 3 packets on kHandle0 and 2 on kHandle1
  EXPECT_EQ(3, handle0_packet_count);
  EXPECT_EQ(2, handle1_packet_count);

  // Notify the processed packets with a Number Of Completed Packet HCI event.
  auto event_buffer = CreateStaticByteBuffer(
      0x13, 0x09,              // Event header
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

    PacketView<hci::ACLDataHeader> packet(&bytes,
                                          bytes.size() - sizeof(ACLDataHeader));
    ConnectionHandle connection_handle =
        le16toh(packet.header().handle_and_flags) & 0xFFF;

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
      auto event_buffer = CreateStaticByteBuffer(
          0x13, 0x0D,  // Event header
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

  // On 3 threads (for each connection handle) we each send 6 packets up to a
  // total of 18.
  auto thread_func = [this](ConnectionHandle handle) {
    for (int i = 0; i < kExpectedTotalPacketCount / 3; ++i) {
      auto packet =
          ACLDataPacket::New(handle, ACLPacketBoundaryFlag::kFirstNonFlushable,
                             ACLBroadcastFlag::kPointToPoint, kLEMaxMTU);
      EXPECT_TRUE(acl_data_channel()->SendPacket(std::move(packet),
                                                 Connection::LinkType::kLE));
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
  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, 100), DataBufferInfo());

  // Empty packet list.
  EXPECT_FALSE(acl_data_channel()->SendPackets(LinkedList<ACLDataPacket>(),
                                               Connection::LinkType::kACL));

  // Packet exceeds MTU
  LinkedList<ACLDataPacket> packets;
  packets.push_back(
      ACLDataPacket::New(0x0001, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, kMaxMTU + 1));
  EXPECT_FALSE(acl_data_channel()->SendPackets(std::move(packets),
                                               Connection::LinkType::kACL));
}

// Tests sending multiple packets in a single call.
TEST_F(HCI_ACLDataChannelTest, SendPackets) {
  constexpr int kExpectedPacketCount = 5;
  InitializeACLDataChannel(DataBufferInfo(1024, 100), DataBufferInfo());

  bool pass = true;
  int seq_no = 0;
  auto data_cb = [&pass, &seq_no](const ByteBuffer& bytes) {
    ZX_DEBUG_ASSERT(bytes.size() >= sizeof(ACLDataHeader));
    PacketView<hci::ACLDataHeader> packet(&bytes,
                                          bytes.size() - sizeof(ACLDataHeader));
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
    auto packet =
        ACLDataPacket::New(1, ACLPacketBoundaryFlag::kFirstNonFlushable,
                           ACLBroadcastFlag::kPointToPoint, 1);
    packet->mutable_view()->mutable_payload_bytes()[0] = i;
    packets.push_back(std::move(packet));
  }

  EXPECT_TRUE(acl_data_channel()->SendPackets(std::move(packets),
                                              Connection::LinkType::kLE));

  RunLoopUntilIdle();

  EXPECT_TRUE(pass);
  EXPECT_EQ(kExpectedPacketCount, seq_no);
}

// Test sending batches of packets atomically across multiple threads.
TEST_F(HCI_ACLDataChannelTest, SendPacketsAtomically) {
  constexpr size_t kThreadCount = 10;
  constexpr size_t kPacketsPerThread = 10;
  constexpr size_t kExpectedPacketCount = kThreadCount * kPacketsPerThread;

  InitializeACLDataChannel(DataBufferInfo(1024, 100), DataBufferInfo());

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
      auto packet =
          ACLDataPacket::New(1, ACLPacketBoundaryFlag::kFirstNonFlushable,
                             ACLBroadcastFlag::kPointToPoint, 1);
      packet->mutable_view()->mutable_payload_bytes()[0] = j;
      packets[i].push_back(std::move(packet));
    }
  }

  // Send each packet sequence on a different thread and make sure that each
  // sequence arrives atomically.
  std::thread threads[kThreadCount];
  auto thread_func = [&packets, this](size_t i) {
    EXPECT_TRUE(acl_data_channel()->SendPackets(std::move(packets[i]),
                                                Connection::LinkType::kLE));
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
    PacketView<hci::ACLDataHeader> packet(
        received[i].get(), received[i]->size() - sizeof(ACLDataHeader));
    EXPECT_EQ(1u, packet.payload_size());
    EXPECT_EQ((i % kPacketsPerThread) + 1, packet.payload_bytes()[0]);
  }
}

TEST_F(HCI_ACLDataChannelTest, ClearLinkState) {
  constexpr size_t kMaxMTU = 1024;
  constexpr size_t kMaxNumPackets = 2;
  constexpr ConnectionHandle kHandle1 = 1;
  constexpr ConnectionHandle kHandle2 = 2;

  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets),
                           DataBufferInfo());

  int packet_count = 0;
  test_device()->SetDataCallback([&](const auto&) { packet_count++; },
                                 dispatcher());

  // Send 3 packets on two links. This is enough to fill up the data buffers.
  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle1, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, 1),
      Connection::LinkType::kLE));
  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle2, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, 1),
      Connection::LinkType::kLE));
  ASSERT_TRUE(acl_data_channel()->SendPacket(
      ACLDataPacket::New(kHandle1, ACLPacketBoundaryFlag::kFirstNonFlushable,
                         ACLBroadcastFlag::kPointToPoint, 1),
      Connection::LinkType::kLE));

  RunLoopUntilIdle();

  // The third packet should have been queued.
  ASSERT_EQ(2, packet_count);

  // Clear the packet count for |kHandle2|. The next packet should go out.
  acl_data_channel()->ClearLinkState(kHandle2);
  RunLoopUntilIdle();
  ASSERT_EQ(3, packet_count);
}

TEST_F(HCI_ACLDataChannelTest, ReceiveData) {
  constexpr size_t kMaxMTU = 5;
  constexpr size_t kMaxNumPackets = 5;

  // It doesn't matter what we set the buffer values to since we're testing
  // incoming packets.
  InitializeACLDataChannel(DataBufferInfo(kMaxMTU, kMaxNumPackets),
                           DataBufferInfo());

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
  transport()->SetTransportClosedCallback(closed_cb, dispatcher());

  async::PostTask(dispatcher(),
                  [this] { test_device()->CloseACLDataChannel(); });
  RunLoopUntilIdle();
  EXPECT_TRUE(closed_cb_called);
}

TEST_F(HCI_ACLDataChannelTest, TransportClosedCallbackBothChannels) {
  InitializeACLDataChannel(DataBufferInfo(1u, 1u), DataBufferInfo(1u, 1u));

  int closed_cb_count = 0;
  auto closed_cb = [&closed_cb_count] { closed_cb_count++; };
  transport()->SetTransportClosedCallback(closed_cb, dispatcher());

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
  auto event_buffer = CreateStaticByteBuffer(
      0x13, 0x09,              // Event header
      0x02,                    // Number of handles
      0x01, 0x00, 0x03, 0x00,  // 3 packets on handle 0x0001
      0x02, 0x00, 0x02, 0x00   // 2 packets on handle 0x0002
  );
  test_device()->SendCommandChannelPacket(event_buffer);

  // Since ACLDataChannel registers the HCI event handler with a dispatcher, it
  // will be processed in a deferred task. We post a ShutDown() task so that
  // events are processed in the following order:
  //
  //   1. Wait task to read the HCI event.
  //   2. ShutDown() task
  //   3. Event handler for the HCI event.
  async::PostTask(dispatcher(), [this] { transport()->ShutDown(); });

  RunLoopUntilIdle();
}

}  // namespace
}  // namespace hci
}  // namespace bt
