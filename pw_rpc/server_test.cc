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

#include <array>
#include <cstdint>

#include "gtest/gtest.h"
#include "pw_assert/assert.h"
#include "pw_rpc/internal/packet.h"
#include "pw_rpc/internal/service.h"
#include "pw_rpc_private/test_utils.h"

namespace pw::rpc {
namespace {

using std::byte;

using internal::Method;
using internal::Packet;
using internal::PacketType;

class TestService : public internal::Service {
 public:
  TestService(uint32_t service_id)
      : internal::Service(service_id, methods_),
        methods_{
            Method(100),
            Method(200),
        } {}

  Method& method(uint32_t id) {
    for (Method& method : methods_) {
      if (method.id() == id) {
        return method;
      }
    }

    PW_CRASH("Invalid method ID %u", static_cast<unsigned>(id));
  }

 private:
  std::array<Method, 2> methods_;
};

class BasicServer : public ::testing::Test {
 protected:
  static constexpr byte kDefaultPayload[] = {
      byte(0x82), byte(0x02), byte(0xff), byte(0xff)};

  BasicServer()
      : channels_{
            Channel::Create<1>(&output_),
            Channel::Create<2>(&output_),
            Channel(),  // available for assignment
        },
        server_(channels_),
        service_(42) {
    server_.RegisterService(service_);
  }

  TestOutput<128> output_;
  std::array<Channel, 3> channels_;
  Server server_;
  TestService service_;

  span<const byte> EncodeRequest(PacketType type,
                                 uint32_t channel_id,
                                 uint32_t service_id,
                                 uint32_t method_id,
                                 span<const byte> payload = kDefaultPayload) {
    auto sws = Packet(type, channel_id, service_id, method_id, payload)
                   .Encode(request_buffer_);
    EXPECT_EQ(Status::OK, sws.status());
    return span(request_buffer_, sws.size());
  }

 private:
  byte request_buffer_[64];
};

TEST_F(BasicServer, ProcessPacket_ValidMethod_InvokesMethod) {
  server_.ProcessPacket(EncodeRequest(PacketType::RPC, 1, 42, 100), output_);

  const Method& method = service_.method(100);
  EXPECT_EQ(1u, method.last_channel_id());
  EXPECT_EQ(sizeof(kDefaultPayload), method.last_request().size());
  EXPECT_EQ(std::memcmp(kDefaultPayload,
                        method.last_request().data(),
                        method.last_request().size()),
            0);
}

TEST_F(BasicServer, ProcessPacket_ValidMethod_SendsOkResponse) {
  server_.ProcessPacket(EncodeRequest(PacketType::RPC, 1, 42, 100), output_);

  Packet packet = Packet::FromBuffer(output_.sent_packet());
  EXPECT_EQ(packet.channel_id(), 1u);
  EXPECT_EQ(packet.service_id(), 42u);
  EXPECT_EQ(packet.method_id(), 100u);
  EXPECT_TRUE(packet.payload().empty());
  EXPECT_EQ(packet.status(), Status::OK);
}

TEST_F(BasicServer, ProcessPacket_ValidMethod_SendsErrorResponse) {
  constexpr byte resp[] = {byte{0xf0}, byte{0x0d}};
  service_.method(200).set_response(resp);
  service_.method(200).set_status(Status::FAILED_PRECONDITION);

  server_.ProcessPacket(EncodeRequest(PacketType::RPC, 2, 42, 200), output_);

  Packet packet = Packet::FromBuffer(output_.sent_packet());
  EXPECT_EQ(packet.channel_id(), 2u);
  EXPECT_EQ(packet.service_id(), 42u);
  EXPECT_EQ(packet.method_id(), 200u);
  EXPECT_EQ(packet.status(), Status::FAILED_PRECONDITION);
  ASSERT_EQ(sizeof(resp), packet.payload().size());
  EXPECT_EQ(std::memcmp(packet.payload().data(), resp, sizeof(resp)), 0);
}

TEST_F(BasicServer, ProcessPacket_InvalidMethod_NothingIsInvoked) {
  server_.ProcessPacket(EncodeRequest(PacketType::RPC, 1, 42, 101), output_);

  EXPECT_EQ(0u, service_.method(100).last_channel_id());
  EXPECT_EQ(0u, service_.method(200).last_channel_id());
}

TEST_F(BasicServer, ProcessPacket_InvalidMethod_SendsNotFound) {
  server_.ProcessPacket(EncodeRequest(PacketType::RPC, 1, 42, 27), output_);

  Packet packet = Packet::FromBuffer(output_.sent_packet());
  EXPECT_EQ(packet.channel_id(), 1u);
  EXPECT_EQ(packet.service_id(), 42u);
  EXPECT_EQ(packet.method_id(), 0u);  // No method ID 27
  EXPECT_EQ(packet.status(), Status::NOT_FOUND);
}

TEST_F(BasicServer, ProcessPacket_InvalidService_SendsNotFound) {
  server_.ProcessPacket(EncodeRequest(PacketType::RPC, 1, 43, 27), output_);

  Packet packet = Packet::FromBuffer(output_.sent_packet());
  EXPECT_EQ(packet.status(), Status::NOT_FOUND);
  EXPECT_EQ(packet.channel_id(), 1u);
  EXPECT_EQ(packet.service_id(), 0u);
}

TEST_F(BasicServer, ProcessPacket_UnassignedChannel_AssignsToAvalableSlot) {
  TestOutput<128> unassigned_output;
  server_.ProcessPacket(
      EncodeRequest(PacketType::RPC, /*channel_id=*/99, 42, 27),
      unassigned_output);
  ASSERT_EQ(channels_[2].id(), 99u);

  Packet packet = Packet::FromBuffer(unassigned_output.sent_packet());
  EXPECT_EQ(packet.channel_id(), 99u);
  EXPECT_EQ(packet.service_id(), 42u);
  EXPECT_EQ(packet.method_id(), 0u);  // No method ID 27
  EXPECT_EQ(packet.status(), Status::NOT_FOUND);
}

TEST_F(BasicServer,
       ProcessPacket_UnassignedChannel_SendsResourceExhaustedIfCannotAssign) {
  channels_[2] = Channel::Create<3>(&output_);  // Occupy only available channel

  server_.ProcessPacket(
      EncodeRequest(PacketType::RPC, /*channel_id=*/99, 42, 27), output_);

  Packet packet = Packet::FromBuffer(output_.sent_packet());
  EXPECT_EQ(packet.status(), Status::RESOURCE_EXHAUSTED);
  EXPECT_EQ(packet.channel_id(), 0u);
  EXPECT_EQ(packet.service_id(), 0u);
  EXPECT_EQ(packet.method_id(), 0u);
}

}  // namespace
}  // namespace pw::rpc
