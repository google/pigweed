// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "slab_allocators.h"

#include <forward_list>

#include <gtest/gtest.h>

#include "acl_data_packet.h"
#include "control_packets.h"

namespace bt::hci::slab_allocators {
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
  LinkedList<Packet<CommandHeader>> packets;

  // Allocate a lot of small packets. We should be able to allocate two
  // allocators' worth of packets until we fail.
  while (auto packet = CommandPacket::New(kTestOpCode, 5)) {
    packets.push_front(std::move(packet));
    num_packets++;
  }

  EXPECT_EQ(kMaxNumSlabs * kNumSmallControlPackets + kMaxNumSlabs * kNumLargeControlPackets,
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
  // Maximum number of packets we can expect to obtain from all the slab allocators.
  const size_t kMaxSlabPackets = kMaxNumSlabs * kNumSmallACLDataPackets +
                                 kMaxNumSlabs * kNumMediumACLDataPackets +
                                 kMaxNumSlabs * kNumLargeACLDataPackets;
  const size_t kPayloadSize = 5;
  LinkedList<Packet<ACLDataHeader>> packets;

  for (size_t num_packets = 0; num_packets < kMaxSlabPackets; num_packets++) {
    auto packet = ACLDataPacket::New(kPayloadSize);
    EXPECT_TRUE(packet);
    packets.push_front(std::move(packet));
  }

  // ACL allocator can fall back on system allocator after slabs are exhausted.
  auto packet = ACLDataPacket::New(kPayloadSize);
  ASSERT_TRUE(packet);

  // Fallback-allocated packet should still function as expected.
  EXPECT_EQ(sizeof(ACLDataHeader) + kPayloadSize, packet->view().size());

  // Write over the whole allocation (errors to be caught by sanitizer instrumentation).
  packet->mutable_view()->mutable_data().Fill('m');
}

TEST(HCI_SlabAllocatorsTest, LargeACLDataPacketFallback) {
  // Maximum number of packets we can expect to obtain from the large slab allocator.
  const size_t kMaxSlabPackets = kMaxNumSlabs * kNumLargeACLDataPackets;
  const size_t kPayloadSize = kLargeACLDataPayloadSize;
  LinkedList<Packet<ACLDataHeader>> packets;

  for (size_t num_packets = 0; num_packets < kMaxSlabPackets; num_packets++) {
    auto packet = ACLDataPacket::New(kPayloadSize);
    EXPECT_TRUE(packet);
    packets.push_front(std::move(packet));
  }

  // ACL allocator can fall back on system allocator after slabs are exhausted.
  auto packet = ACLDataPacket::New(kPayloadSize);
  ASSERT_TRUE(packet);

  // Fallback-allocated packet should still function as expected.
  EXPECT_EQ(sizeof(ACLDataHeader) + kPayloadSize, packet->view().size());

  // Write over the whole allocation (errors to be caught by sanitizer instrumentation).
  packet->mutable_view()->mutable_data().Fill('m');
}

}  // namespace
}  // namespace bt::hci::slab_allocators
