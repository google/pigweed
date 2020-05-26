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

#include "pw_rpc/server.h"

#include "gtest/gtest.h"
#include "pw_rpc/internal/packet.h"

namespace pw::rpc {
namespace {

using internal::Packet;
using internal::PacketType;
using std::byte;

template <size_t buffer_size>
class TestOutput : public ChannelOutput {
 public:
  constexpr TestOutput(uint32_t id) : ChannelOutput(id), sent_packet_({}) {}

  span<byte> AcquireBuffer() override { return buffer_; }

  void SendAndReleaseBuffer(size_t size) override {
    sent_packet_ = {buffer_, size};
  }

  span<const byte> sent_packet() const { return sent_packet_; }

 private:
  byte buffer_[buffer_size];
  span<const byte> sent_packet_;
};

Packet MakePacket(uint32_t channel_id,
                  uint32_t service_id,
                  uint32_t method_id,
                  span<const byte> payload) {
  Packet packet = Packet::Empty(PacketType::RPC);
  packet.set_channel_id(channel_id);
  packet.set_service_id(service_id);
  packet.set_method_id(method_id);
  packet.set_payload(payload);
  return packet;
}

TEST(Server, ProcessPacket_SendsResponse) {
  TestOutput<128> output(1);
  Channel channels[] = {
      Channel::Create<1>(&output),
      Channel::Create<2>(&output),
  };
  Server server(channels);
  internal::Service service(42, {});
  server.RegisterService(service);

  byte encoded_packet[64];
  constexpr byte payload[] = {byte(0x82), byte(0x02), byte(0xff), byte(0xff)};
  Packet request = MakePacket(1, 42, 27, payload);
  auto sws = request.Encode(encoded_packet);

  server.ProcessPacket(span(encoded_packet, sws.size()), output);
  Packet packet = Packet::FromBuffer(output.sent_packet());
  EXPECT_EQ(packet.status(), Status::OK);
  EXPECT_EQ(packet.channel_id(), 1u);
  EXPECT_EQ(packet.service_id(), 42u);
}

TEST(Server, ProcessPacket_SendsNotFoundOnInvalidService) {
  TestOutput<128> output(1);
  Channel channels[] = {
      Channel::Create<1>(&output),
      Channel::Create<2>(&output),
  };
  Server server(channels);
  internal::Service service(42, {});
  server.RegisterService(service);

  byte encoded_packet[64];
  constexpr byte payload[] = {byte(0x82), byte(0x02), byte(0xff), byte(0xff)};
  Packet request = MakePacket(1, 43, 27, payload);
  auto sws = request.Encode(encoded_packet);

  server.ProcessPacket(span(encoded_packet, sws.size()), output);
  Packet packet = Packet::FromBuffer(output.sent_packet());
  EXPECT_EQ(packet.status(), Status::NOT_FOUND);
  EXPECT_EQ(packet.channel_id(), 1u);
  EXPECT_EQ(packet.service_id(), 0u);
}

TEST(Server, ProcessPacket_AssignsAnUnassignedChannel) {
  TestOutput<128> output(1);
  Channel channels[] = {
      Channel::Create<1>(&output),
      Channel::Create<2>(&output),
      Channel(),
  };
  Server server(channels);
  internal::Service service(42, {});
  server.RegisterService(service);

  byte encoded_packet[64];
  constexpr byte payload[] = {byte(0x82), byte(0x02), byte(0xff), byte(0xff)};
  Packet request = MakePacket(/*channel_id=*/99, 42, 27, payload);
  auto sws = request.Encode(encoded_packet);

  TestOutput<128> unassigned_output(2);
  server.ProcessPacket(span(encoded_packet, sws.size()), unassigned_output);
  ASSERT_EQ(channels[2].id(), 99u);

  Packet packet = Packet::FromBuffer(unassigned_output.sent_packet());
  EXPECT_EQ(packet.status(), Status::OK);
  EXPECT_EQ(packet.channel_id(), 99u);
  EXPECT_EQ(packet.service_id(), 42u);
}

TEST(Server, ProcessPacket_SendsResourceExhaustedWhenChannelCantBeAssigned) {
  TestOutput<128> output(1);
  Channel channels[] = {
      Channel::Create<1>(&output),
      Channel::Create<2>(&output),
  };
  Server server(channels);
  internal::Service service(42, {});
  server.RegisterService(service);

  byte encoded_packet[64];
  constexpr byte payload[] = {byte(0x82), byte(0x02), byte(0xff), byte(0xff)};
  Packet request = MakePacket(/*channel_id=*/99, 42, 27, payload);
  auto sws = request.Encode(encoded_packet);

  server.ProcessPacket(span(encoded_packet, sws.size()), output);

  Packet packet = Packet::FromBuffer(output.sent_packet());
  EXPECT_EQ(packet.status(), Status::RESOURCE_EXHAUSTED);
  EXPECT_EQ(packet.channel_id(), 0u);
  EXPECT_EQ(packet.service_id(), 0u);
  EXPECT_EQ(packet.method_id(), 0u);
}

}  // namespace
}  // namespace pw::rpc
