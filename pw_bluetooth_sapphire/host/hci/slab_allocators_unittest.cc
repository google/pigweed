// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <forward_list>

#include "gtest/gtest.h"

#include "acl_data_packet.h"
#include "control_packets.h"
#include "slab_allocators.h"

namespace btlib {
namespace hci {
namespace slab_allocators {
namespace {

constexpr OpCode kTestOpCode = 0xFFFF;

TEST(HCI_SlabAllocatorsTest, CommandPacket) {
  auto packet = CommandPacket::New(kTestOpCode, 5);
  EXPECT_TRUE(packet);
  EXPECT_EQ(5u + sizeof(CommandHeader), packet->view().size());

  packet = CommandPacket::New(kTestOpCode, kSmallControlPayloadSize);
  EXPECT_TRUE(packet);
  EXPECT_GE(packet->view().size(), kSmallControlPacketSize);

  packet = CommandPacket::New(kTestOpCode, kSmallControlPayloadSize + 1);
  EXPECT_TRUE(packet);
  EXPECT_EQ(kSmallControlPacketSize + 1, packet->view().size());
}

TEST(HCI_SlabAllocatorsTest, CommandPacketFallBack) {
  size_t num_packets = 0;
  common::LinkedList<Packet<CommandHeader>> packets;

  // Allocate a lot of small packets. We should be able to allocate two
  // allocators' worth of packets until we fail.
  while (auto packet = CommandPacket::New(kTestOpCode, 5)) {
    packets.push_front(std::move(packet));
    num_packets++;
  }

  EXPECT_EQ(kMaxNumSlabs * kNumSmallControlPackets +
                kMaxNumSlabs * kNumLargeControlPackets,
            num_packets);
}

TEST(HCI_SlabAllocatorsTest, ACLDataPacket) {
  auto packet = ACLDataPacket::New(5);
  EXPECT_TRUE(packet);
  EXPECT_EQ(packet->view().size(), 5u + sizeof(ACLDataHeader));

  packet = ACLDataPacket::New(kSmallACLDataPayloadSize);
  EXPECT_TRUE(packet);
  EXPECT_EQ(kSmallACLDataPacketSize, packet->view().size());

  packet = ACLDataPacket::New(kSmallACLDataPayloadSize + 1);
  EXPECT_TRUE(packet);
  EXPECT_EQ(kSmallACLDataPacketSize + 1, packet->view().size());

  packet = ACLDataPacket::New(kMediumACLDataPayloadSize + 1);
  EXPECT_EQ(kMediumACLDataPacketSize + 1, packet->view().size());
}

TEST(HCI_SlabAllocatorsTest, ACLDataPacketFallBack) {
  size_t num_packets = 0;
  common::LinkedList<Packet<ACLDataHeader>> packets;

  // Allocate a lot of small packets. We should be able to allocate three
  // allocators' worth of packets until we fail.
  while (auto packet = ACLDataPacket::New(5)) {
    packets.push_front(std::move(packet));
    num_packets++;
  }

  EXPECT_EQ(kMaxNumSlabs * kNumSmallACLDataPackets +
                kMaxNumSlabs * kNumMediumACLDataPackets +
                kMaxNumSlabs * kNumLargeACLDataPackets,
            num_packets);
}

}  // namespace
}  // namespace slab_allocators
}  // namespace hci
}  // namespace btlib
