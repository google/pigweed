// Copyright 2020 The Pigweed Authors
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

#include "pw_rpc/channel.h"

#include <cstddef>

#include "pw_rpc/internal/packet.h"
#include "pw_rpc/internal/test_utils.h"
#include "pw_unit_test/framework.h"

namespace pw::rpc::internal {
namespace {

TEST(ChannelOutput, Name) {
  class NameTester : public ChannelOutput {
   public:
    NameTester(const char* name) : ChannelOutput(name) {}
    Status Send(span<const std::byte>) override { return OkStatus(); }
  };

  EXPECT_STREQ("hello_world", NameTester("hello_world").name());
  EXPECT_EQ(nullptr, NameTester(nullptr).name());
}

constexpr Packet kTestPacket(
    pwpb::PacketType::RESPONSE, 23, 42, 100, 0, {}, Status::NotFound());
const size_t kReservedSize = 2 /* type */ + 2 /* channel */ + 5 /* service */ +
                             5 /* method */ + 2 /* payload key */ +
                             2 /* status (if not OK) */;

enum class ChannelId {
  kOne = 1,
  kTwo = 2,
};

TEST(Channel, MaxSafePayload) {
  constexpr size_t kUint32Max = std::numeric_limits<uint32_t>::max();
  constexpr size_t kMaxPayloadSize = 64;

  constexpr size_t kTestPayloadSize = MaxSafePayloadSize(kMaxPayloadSize);

  // Because it's impractical to test a payload that nears the limits of a
  // uint32 varint, calculate the difference when using a smaller payload.
  constexpr size_t kPayloadSizeTestLimitations =
      varint::EncodedSize(kUint32Max) - varint::EncodedSize(kTestPayloadSize);

  // The buffer to use for encoding the RPC packet.
  std::array<std::byte, kMaxPayloadSize - kPayloadSizeTestLimitations> buffer;

  std::array<std::byte, kTestPayloadSize> payload;
  for (size_t i = 0; i < payload.size(); i++) {
    payload[i] = std::byte(i % std::numeric_limits<uint8_t>::max());
  }

  Packet packet(pwpb::PacketType::SERVER_STREAM,
                /*channel_id=*/kUint32Max,  // Varint, needs to be uint32_t max.
                /*service_id=*/42,       // Fixed-width. Value doesn't matter.
                /*method_id=*/100,       // Fixed-width. Value doesn't matter.
                /*call_id=*/kUint32Max,  // Varint, needs to be uint32_t max.
                payload,
                Status::Unauthenticated());

  Result<ConstByteSpan> result = packet.Encode(buffer);
  ASSERT_EQ(OkStatus(), result.status());
}

TEST(Channel, MaxSafePayload_OffByOne) {
  constexpr size_t kUint32Max = std::numeric_limits<uint32_t>::max();
  constexpr size_t kMaxPayloadSize = 64;

  constexpr size_t kTestPayloadSize = MaxSafePayloadSize(kMaxPayloadSize);

  // Because it's impractical to test a payload that nears the limits of a
  // uint32 varint, calculate the difference when using a smaller payload.
  constexpr size_t kPayloadSizeTestLimitations =
      varint::EncodedSize(kUint32Max) - varint::EncodedSize(kTestPayloadSize);

  // The buffer to use for encoding the RPC packet.
  std::array<std::byte, kMaxPayloadSize - kPayloadSizeTestLimitations - 1>
      buffer;

  std::array<std::byte, kTestPayloadSize> payload;
  for (size_t i = 0; i < payload.size(); i++) {
    payload[i] = std::byte(i % std::numeric_limits<uint8_t>::max());
  }

  Packet packet(pwpb::PacketType::SERVER_STREAM,
                /*channel_id=*/kUint32Max,  // Varint, needs to be uint32_t max.
                /*service_id=*/42,       // Fixed-width. Value doesn't matter.
                /*method_id=*/100,       // Fixed-width. Value doesn't matter.
                /*call_id=*/kUint32Max,  // Varint, needs to be uint32_t max.
                payload,
                Status::Unauthenticated());

  Result<ConstByteSpan> result = packet.Encode(buffer);
  ASSERT_EQ(Status::ResourceExhausted(), result.status());
}

TEST(Channel, Create_FromEnum) {
  constexpr rpc::Channel one = Channel::Create<ChannelId::kOne>(nullptr);
  constexpr rpc::Channel two = Channel::Create<ChannelId::kTwo>(nullptr);
  static_assert(one.id() == 1);
  static_assert(two.id() == 2);
}

TEST(Channel, TestPacket_ReservedSizeMatchesMinEncodedSizeBytes) {
  EXPECT_EQ(kReservedSize, kTestPacket.MinEncodedSizeBytes());
}

TEST(ExtractChannelId, ValidPacket) {
  std::byte buffer[64] = {};
  Result<ConstByteSpan> result = kTestPacket.Encode(buffer);
  ASSERT_EQ(result.status(), OkStatus());

  Result<uint32_t> channel_id = ExtractChannelId(*result);
  ASSERT_EQ(channel_id.status(), OkStatus());
  EXPECT_EQ(*channel_id, 23u);
}

TEST(ExtractChannelId, InvalidPacket) {
  constexpr std::byte buffer[64] = {std::byte{1}, std::byte{2}};

  Result<uint32_t> channel_id = ExtractChannelId(buffer);

  EXPECT_EQ(channel_id.status(), Status::DataLoss());
}

TEST(ExtractChannelId, ChangeChannelIdToValidValue) {
  std::byte buffer[64] = {};
  Result<ConstByteSpan> result = kTestPacket.Encode(buffer);
  ByteSpan packet(buffer, result.value().size());

  EXPECT_EQ(ChangeEncodedChannelId(packet, 0), OkStatus());
  EXPECT_EQ(ExtractChannelId(packet).value(), 0u);

  EXPECT_EQ(ChangeEncodedChannelId(packet, 1), OkStatus());
  EXPECT_EQ(ExtractChannelId(packet).value(), 1u);

  EXPECT_EQ(ChangeEncodedChannelId(packet, 23), OkStatus());
  EXPECT_EQ(ExtractChannelId(packet).value(), 23u);

  EXPECT_EQ(ChangeEncodedChannelId(packet, 127), OkStatus());
  EXPECT_EQ(ExtractChannelId(packet).value(), 127u);

  EXPECT_EQ(ChangeEncodedChannelId<0>(packet), OkStatus());
  EXPECT_EQ(ExtractChannelId(packet).value(), 0u);

  EXPECT_EQ(ChangeEncodedChannelId<1>(packet), OkStatus());
  EXPECT_EQ(ExtractChannelId(packet).value(), 1u);

  EXPECT_EQ(ChangeEncodedChannelId<23>(packet), OkStatus());
  EXPECT_EQ(ExtractChannelId(packet).value(), 23u);

  EXPECT_EQ(ChangeEncodedChannelId<127>(packet), OkStatus());
  EXPECT_EQ(ExtractChannelId(packet).value(), 127u);
}

TEST(ExtractChannelId, ChangeChannelIdTooLarge) {
  constexpr Packet kChannelIdTooLargePacket(
      pwpb::PacketType::RESPONSE, 128, 42, 100, 0, {}, Status::NotFound());

  std::byte buffer[64] = {};
  Result<ConstByteSpan> result = kChannelIdTooLargePacket.Encode(buffer);
  ByteSpan packet(buffer, result.value().size());

  EXPECT_EQ(ChangeEncodedChannelId(packet, 0), Status::OutOfRange());
  EXPECT_EQ(ChangeEncodedChannelId(packet, 1), Status::OutOfRange());
  EXPECT_EQ(ChangeEncodedChannelId(packet, 23), Status::OutOfRange());
  EXPECT_EQ(ChangeEncodedChannelId(packet, 127), Status::OutOfRange());
}

TEST(ExtractChannelId, ChangeChannelIdNoChannelFound) {
  std::byte packet[64] = {};

  EXPECT_EQ(ChangeEncodedChannelId(packet, 0), Status::DataLoss());
  EXPECT_EQ(ChangeEncodedChannelId(packet, 1), Status::DataLoss());
  EXPECT_EQ(ChangeEncodedChannelId(packet, 23), Status::DataLoss());
  EXPECT_EQ(ChangeEncodedChannelId(packet, 127), Status::DataLoss());
}

}  // namespace
}  // namespace pw::rpc::internal
