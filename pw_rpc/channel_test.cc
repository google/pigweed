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

#include "gtest/gtest.h"
#include "pw_rpc/internal/packet.h"
#include "pw_rpc/internal/test_utils.h"

namespace pw::rpc::internal {
namespace {

TEST(ChannelOutput, Name) {
  class NameTester : public ChannelOutput {
   public:
    NameTester(const char* name) : ChannelOutput(name) {}
    Status Send(std::span<const std::byte>) override { return OkStatus(); }
  };

  EXPECT_STREQ("hello_world", NameTester("hello_world").name());
  EXPECT_EQ(nullptr, NameTester(nullptr).name());
}

constexpr Packet kTestPacket(
    PacketType::RESPONSE, 23, 42, 100, 0, {}, Status::NotFound());
const size_t kReservedSize = 2 /* type */ + 2 /* channel */ + 5 /* service */ +
                             5 /* method */ + 2 /* payload key */ +
                             2 /* status (if not OK) */;

enum class ChannelId {
  kOne = 1,
  kTwo = 2,
};

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

}  // namespace
}  // namespace pw::rpc::internal
