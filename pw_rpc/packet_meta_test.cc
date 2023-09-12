// Copyright 2022 The Pigweed Authors
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

#include "pw_rpc/packet_meta.h"

#include "gtest/gtest.h"
#include "pw_fuzzer/fuzztest.h"
#include "pw_rpc/internal/packet.h"

namespace pw::rpc {
namespace {

using namespace fuzzer;

void FromBufferDecodesValidMinimalPacket(uint32_t channel_id,
                                         uint32_t service_id,
                                         uint32_t method_id) {
  internal::Packet packet;
  packet.set_channel_id(channel_id);
  packet.set_service_id(service_id);
  packet.set_type(internal::pwpb::PacketType::RESPONSE);
  packet.set_method_id(method_id);

  std::byte buffer[128];
  Result<ConstByteSpan> encode_result = packet.Encode(buffer);
  ASSERT_EQ(encode_result.status(), OkStatus());

  Result<PacketMeta> decode_result = PacketMeta::FromBuffer(*encode_result);
  ASSERT_EQ(decode_result.status(), OkStatus());
  EXPECT_EQ(decode_result->channel_id(), channel_id);
  EXPECT_EQ(decode_result->service_id(), internal::WrapServiceId(service_id));
  EXPECT_TRUE(decode_result->destination_is_client());
}

TEST(PacketMeta, FromBufferDecodesValidMinimalPacketConst) {
  const uint32_t kChannelId = 12;
  const uint32_t kServiceId = 0xdeadbeef;
  const uint32_t kMethodId = 44;
  FromBufferDecodesValidMinimalPacket(kChannelId, kServiceId, kMethodId);
}

FUZZ_TEST(PacketMeta, FromBufferDecodesValidMinimalPacket)
    .WithDomains(NonZero<uint32_t>(), NonZero<uint32_t>(), NonZero<uint32_t>());

TEST(PacketMeta, FromBufferFailsOnIncompletePacket) {
  internal::Packet packet;

  std::byte buffer[128];
  Result<ConstByteSpan> encode_result = packet.Encode(buffer);
  ASSERT_EQ(encode_result.status(), OkStatus());

  Result<PacketMeta> decode_result = PacketMeta::FromBuffer(*encode_result);
  ASSERT_EQ(decode_result.status(), Status::DataLoss());
}

}  // namespace
}  // namespace pw::rpc
