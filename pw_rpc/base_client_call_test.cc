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

#include "pw_rpc/internal/base_client_call.h"

#include "gtest/gtest.h"
#include "pw_rpc_private/internal_test_utils.h"

namespace pw::rpc::internal {
namespace {

TEST(BaseClientCall, RegistersAndRemovesItselfFromClient) {
  ClientContextForTest context;
  EXPECT_EQ(context.client().active_calls(), 0u);

  {
    BaseClientCall call(&context.channel(),
                        context.kServiceId,
                        context.kMethodId,
                        [](BaseClientCall&, const Packet&) {});
    EXPECT_EQ(context.client().active_calls(), 1u);
  }

  EXPECT_EQ(context.client().active_calls(), 0u);
}

class FakeClientCall : public BaseClientCall {
 public:
  constexpr FakeClientCall(rpc::Channel* channel,
                           uint32_t service_id,
                           uint32_t method_id,
                           ResponseHandler handler)
      : BaseClientCall(channel, service_id, method_id, handler) {}

  Status SendPacket(std::span<const std::byte> payload) {
    std::span buffer = AcquirePayloadBuffer();
    std::memcpy(buffer.data(), payload.data(), payload.size());
    return ReleasePayloadBuffer(buffer.first(payload.size()));
  }
};

TEST(BaseClientCall, SendsPacketWithPayload) {
  ClientContextForTest context;
  FakeClientCall call(&context.channel(),
                      context.kServiceId,
                      context.kMethodId,
                      [](BaseClientCall&, const Packet&) {});

  constexpr std::byte payload[]{std::byte{0x08}, std::byte{0x39}};
  call.SendPacket(payload);

  EXPECT_EQ(context.output().packet_count(), 1u);
  Packet packet = context.output().sent_packet();
  EXPECT_EQ(packet.channel_id(), context.channel().id());
  EXPECT_EQ(packet.service_id(), context.kServiceId);
  EXPECT_EQ(packet.method_id(), context.kMethodId);
  EXPECT_EQ(std::memcmp(packet.payload().data(), payload, sizeof(payload)), 0);
}

}  // namespace
}  // namespace pw::rpc::internal
