// Copyright 2023 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "pw_bluetooth_sapphire/internal/host/transport/slab_allocators.h"

#include <gtest/gtest.h>

#include <forward_list>
#include <list>

#include "pw_bluetooth_sapphire/internal/host/transport/acl_data_packet.h"
#include "pw_bluetooth_sapphire/internal/host/transport/control_packets.h"

namespace bt::hci::allocators {
namespace {

constexpr hci_spec::OpCode kTestOpCode = 0xFFFF;

TEST(SlabAllocatorsTest, CommandPacket) {
  auto packet = CommandPacket::New(kTestOpCode, 5);
  EXPECT_TRUE(packet);
  EXPECT_EQ(5u + sizeof(hci_spec::CommandHeader), packet->view().size());

  packet = CommandPacket::New(kTestOpCode, kSmallControlPayloadSize);
  EXPECT_TRUE(packet);
  EXPECT_GE(packet->view().size(), kSmallControlPacketSize);

  packet = CommandPacket::New(kTestOpCode, kSmallControlPayloadSize + 1);
  EXPECT_TRUE(packet);
  EXPECT_EQ(kSmallControlPacketSize + 1, packet->view().size());
}

TEST(SlabAllocatorsTest, CommandPacketFallBack) {
  // Maximum number of packets we can expect to obtain from all the slab
  // allocators.
  const size_t kMaxSlabPackets = kMaxNumSlabs * kNumSmallControlPackets +
                                 kMaxNumSlabs * kNumLargeControlPackets;

  std::list<std::unique_ptr<hci::CommandPacket>> packets;
  for (size_t num_packets = 0; num_packets < kMaxSlabPackets; num_packets++) {
    auto packet = CommandPacket::New(kTestOpCode, 5);
    packets.push_front(std::move(packet));
  }

  // Command allocator can fall back on system allocator after slabs are
  // exhausted.
  auto packet = CommandPacket::New(kTestOpCode, 5);
  ASSERT_TRUE(packet);
}

TEST(SlabAllocatorsTest, ACLDataPacket) {
  auto packet = ACLDataPacket::New(5);
  EXPECT_TRUE(packet);
  EXPECT_EQ(packet->view().size(), 5u + sizeof(hci_spec::ACLDataHeader));

  packet = ACLDataPacket::New(kSmallACLDataPayloadSize);
  EXPECT_TRUE(packet);
  EXPECT_EQ(kSmallACLDataPacketSize, packet->view().size());

  packet = ACLDataPacket::New(kSmallACLDataPayloadSize + 1);
  EXPECT_TRUE(packet);
  EXPECT_EQ(kSmallACLDataPacketSize + 1, packet->view().size());

  packet = ACLDataPacket::New(kMediumACLDataPayloadSize + 1);
  EXPECT_EQ(kMediumACLDataPacketSize + 1, packet->view().size());
}

TEST(SlabAllocatorsTest, ACLDataPacketFallBack) {
  // Maximum number of packets we can expect to obtain from all the slab
  // allocators.
  const size_t kMaxSlabPackets = kMaxNumSlabs * kNumSmallACLDataPackets +
                                 kMaxNumSlabs * kNumMediumACLDataPackets +
                                 kMaxNumSlabs * kNumLargeACLDataPackets;
  const size_t kPayloadSize = 5;
  std::list<hci::ACLDataPacketPtr> packets;

  for (size_t num_packets = 0; num_packets < kMaxSlabPackets; num_packets++) {
    auto packet = ACLDataPacket::New(kPayloadSize);
    EXPECT_TRUE(packet);
    packets.push_front(std::move(packet));
  }

  // ACL allocator can fall back on system allocator after slabs are exhausted.
  auto packet = ACLDataPacket::New(kPayloadSize);
  ASSERT_TRUE(packet);

  // Fallback-allocated packet should still function as expected.
  EXPECT_EQ(sizeof(hci_spec::ACLDataHeader) + kPayloadSize,
            packet->view().size());

  // Write over the whole allocation (errors to be caught by sanitizer
  // instrumentation).
  packet->mutable_view()->mutable_data().Fill('m');
}

TEST(SlabAllocatorsTest, LargeACLDataPacketFallback) {
  // Maximum number of packets we can expect to obtain from the large slab
  // allocator.
  const size_t kMaxSlabPackets = kMaxNumSlabs * kNumLargeACLDataPackets;
  const size_t kPayloadSize = kLargeACLDataPayloadSize;
  std::list<hci::ACLDataPacketPtr> packets;

  for (size_t num_packets = 0; num_packets < kMaxSlabPackets; num_packets++) {
    auto packet = ACLDataPacket::New(kPayloadSize);
    EXPECT_TRUE(packet);
    packets.push_front(std::move(packet));
  }

  // ACL allocator can fall back on system allocator after slabs are exhausted.
  auto packet = ACLDataPacket::New(kPayloadSize);
  ASSERT_TRUE(packet);

  // Fallback-allocated packet should still function as expected.
  EXPECT_EQ(sizeof(hci_spec::ACLDataHeader) + kPayloadSize,
            packet->view().size());

  // Write over the whole allocation (errors to be caught by sanitizer
  // instrumentation).
  packet->mutable_view()->mutable_data().Fill('m');
}

}  // namespace
}  // namespace bt::hci::allocators
