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

#include "pw_rpc/client.h"

#include "gtest/gtest.h"
#include "pw_rpc/internal/packet.h"
#include "pw_rpc_private/internal_test_utils.h"

namespace pw::rpc {
namespace {

using internal::BaseClientCall;
using internal::Packet;
using internal::PacketType;

class TestClientCall : public BaseClientCall {
 public:
  constexpr TestClientCall(Channel* channel,
                           uint32_t service_id,
                           uint32_t method_id)
      : BaseClientCall(channel, service_id, method_id, ProcessPacket) {}

  static void ProcessPacket(BaseClientCall& call, const Packet& packet) {
    static_cast<TestClientCall&>(call).HandlePacket(packet);
  }

  void HandlePacket(const Packet&) { invoked_ = true; }

  constexpr bool invoked() const { return invoked_; }

 private:
  bool invoked_ = false;
};

TEST(Client, ProcessPacket_InvokesARegisteredClientCall) {
  ClientContextForTest context;

  TestClientCall call(
      &context.channel(), context.kServiceId, context.kMethodId);
  EXPECT_EQ(context.SendResponse(Status::Ok(), {}), Status::Ok());

  EXPECT_TRUE(call.invoked());
}

TEST(Client, ProcessPacket_SendsClientErrorOnUnregisteredCall) {
  ClientContextForTest context;

  EXPECT_EQ(context.SendResponse(Status::OK, {}), Status::NotFound());

  ASSERT_EQ(context.output().packet_count(), 1u);
  const Packet& packet = context.output().sent_packet();
  EXPECT_EQ(packet.type(), PacketType::CLIENT_ERROR);
  EXPECT_EQ(packet.channel_id(), context.kChannelId);
  EXPECT_EQ(packet.service_id(), context.kServiceId);
  EXPECT_EQ(packet.method_id(), context.kMethodId);
  EXPECT_TRUE(packet.payload().empty());
  EXPECT_EQ(packet.status(), Status::FailedPrecondition());
}

TEST(Client, ProcessPacket_ReturnsDataLossOnBadPacket) {
  ClientContextForTest context;

  constexpr std::byte bad_packet[]{
      std::byte{0xab}, std::byte{0xcd}, std::byte{0xef}};
  EXPECT_EQ(context.client().ProcessPacket(bad_packet), Status::DataLoss());
}

TEST(Client, ProcessPacket_ReturnsInvalidArgumentOnServerPacket) {
  ClientContextForTest context;
  EXPECT_EQ(context.SendPacket(PacketType::REQUEST), Status::InvalidArgument());
  EXPECT_EQ(context.SendPacket(PacketType::CANCEL_SERVER_STREAM),
            Status::InvalidArgument());
}

}  // namespace
}  // namespace pw::rpc
