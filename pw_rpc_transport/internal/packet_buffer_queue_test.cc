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

#include "pw_rpc_transport/internal/packet_buffer_queue.h"

#include <array>

#include "gtest/gtest.h"
#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_status/status.h"

namespace pw::rpc::internal {
namespace {

constexpr size_t kMaxPacketSize = 256;

TEST(PacketBufferQueueTest, CopyAndGetPacket) {
  PacketBufferQueue<kMaxPacketSize>::PacketBuffer packet_buffer = {};
  std::array<std::byte, 42> input{};
  std::fill(input.begin(), input.end(), std::byte{0x42});

  auto packet = packet_buffer.GetPacket();
  ASSERT_EQ(packet.status(), OkStatus());
  EXPECT_EQ(packet->size(), 0ul);

  ASSERT_EQ(packet_buffer.CopyPacket(input), OkStatus());

  packet = packet_buffer.GetPacket();
  ASSERT_EQ(packet.status(), OkStatus());
  EXPECT_EQ(packet->size(), 42ul);

  EXPECT_TRUE(std::equal(input.begin(), input.end(), packet->begin()));

  std::array<std::byte, 300> long_input{};
  EXPECT_EQ(packet_buffer.CopyPacket(long_input), Status::ResourceExhausted());
}

TEST(PacketBufferQueueTest, PopWhenEmptyFails) {
  PacketBufferQueue<kMaxPacketSize> queue;
  EXPECT_EQ(queue.Pop().status(), Status::ResourceExhausted());
}

TEST(PacketBufferQueueTest, PopAllSucceeds) {
  constexpr auto kPacketQueueSize = 3;

  std::array<PacketBufferQueue<kMaxPacketSize>::PacketBuffer, kPacketQueueSize>
      packets;
  PacketBufferQueue<kMaxPacketSize> queue(packets);

  for (size_t i = 0; i < kPacketQueueSize; ++i) {
    EXPECT_EQ(queue.Pop().status(), OkStatus());
  }

  EXPECT_EQ(queue.Pop().status(), Status::ResourceExhausted());
}

TEST(PacketQueueTest, PushPopSucceeds) {
  PacketBufferQueue<kMaxPacketSize>::PacketBuffer packet_buffer;
  PacketBufferQueue<kMaxPacketSize> queue;

  pw::Result<PacketBufferQueue<kMaxPacketSize>::PacketBuffer*>
      popped_packet_buffer = queue.Pop();
  EXPECT_EQ(popped_packet_buffer.status(), Status::ResourceExhausted());

  queue.Push(packet_buffer);
  popped_packet_buffer = queue.Pop();
  EXPECT_EQ(popped_packet_buffer.status(), OkStatus());
}

}  // namespace
}  // namespace pw::rpc::internal
