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
#include "pw_rpc/internal/method.h"
#include "pw_rpc/internal/packet.h"
#include "pw_rpc/internal/test_method.h"
#include "pw_rpc/service.h"
#include "pw_rpc_private/internal_test_utils.h"

namespace pw::rpc {
namespace {

using std::byte;

using internal::Packet;
using internal::PacketType;
using internal::TestMethod;
using internal::TestMethodUnion;

class TestService : public Service {
 public:
  TestService(uint32_t service_id)
      : Service(service_id, methods_),
        methods_{
            TestMethod(100),
            TestMethod(200),
        } {}

  const TestMethod& method(uint32_t id) {
    for (TestMethodUnion& method : methods_) {
      if (method.method().id() == id) {
        return method.test_method();
      }
    }

    PW_CRASH("Invalid method ID %u", static_cast<unsigned>(id));
  }

 private:
  std::array<TestMethodUnion, 2> methods_;
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

  std::span<const byte> EncodeRequest(
      PacketType type,
      uint32_t channel_id,
      uint32_t service_id,
      uint32_t method_id,
      std::span<const byte> payload = kDefaultPayload) {
    auto result = Packet(type, channel_id, service_id, method_id, payload)
                      .Encode(request_buffer_);
    EXPECT_EQ(Status::Ok(), result.status());
    return result.value_or(ConstByteSpan());
  }

  TestOutput<128> output_;
  std::array<Channel, 3> channels_;
  Server server_;
  TestService service_;

 private:
  byte request_buffer_[64];
};

TEST_F(BasicServer, ProcessPacket_ValidMethod_InvokesMethod) {
  EXPECT_EQ(Status::Ok(),
            server_.ProcessPacket(
                EncodeRequest(PacketType::REQUEST, 1, 42, 100), output_));

  const TestMethod& method = service_.method(100);
  EXPECT_EQ(1u, method.last_channel_id());
  ASSERT_EQ(sizeof(kDefaultPayload), method.last_request().payload().size());
  EXPECT_EQ(std::memcmp(kDefaultPayload,
                        method.last_request().payload().data(),
                        method.last_request().payload().size()),
            0);
}

TEST_F(BasicServer, ProcessPacket_IncompletePacket_NothingIsInvoked) {
  EXPECT_EQ(Status::DataLoss(),
            server_.ProcessPacket(
                EncodeRequest(PacketType::REQUEST, 0, 42, 101), output_));
  EXPECT_EQ(Status::DataLoss(),
            server_.ProcessPacket(EncodeRequest(PacketType::REQUEST, 1, 0, 101),
                                  output_));
  EXPECT_EQ(Status::DataLoss(),
            server_.ProcessPacket(EncodeRequest(PacketType::REQUEST, 1, 42, 0),
                                  output_));

  EXPECT_EQ(0u, service_.method(100).last_channel_id());
  EXPECT_EQ(0u, service_.method(200).last_channel_id());
}

TEST_F(BasicServer, ProcessPacket_NoChannel_SendsNothing) {
  EXPECT_EQ(Status::DataLoss(),
            server_.ProcessPacket(
                EncodeRequest(PacketType::REQUEST, 0, 42, 101), output_));

  EXPECT_EQ(output_.packet_count(), 0u);
}

TEST_F(BasicServer, ProcessPacket_NoService_SendsDataLoss) {
  EXPECT_EQ(Status::DataLoss(),
            server_.ProcessPacket(EncodeRequest(PacketType::REQUEST, 1, 0, 101),
                                  output_));

  EXPECT_EQ(output_.sent_packet().type(), PacketType::SERVER_ERROR);
  EXPECT_EQ(output_.sent_packet().status(), Status::DataLoss());
}

TEST_F(BasicServer, ProcessPacket_NoMethod_SendsDataLoss) {
  EXPECT_EQ(Status::DataLoss(),
            server_.ProcessPacket(EncodeRequest(PacketType::REQUEST, 1, 42, 0),
                                  output_));

  EXPECT_EQ(output_.sent_packet().type(), PacketType::SERVER_ERROR);
  EXPECT_EQ(output_.sent_packet().status(), Status::DataLoss());
}

TEST_F(BasicServer, ProcessPacket_InvalidMethod_NothingIsInvoked) {
  EXPECT_EQ(Status::Ok(),
            server_.ProcessPacket(
                EncodeRequest(PacketType::REQUEST, 1, 42, 101), output_));

  EXPECT_EQ(0u, service_.method(100).last_channel_id());
  EXPECT_EQ(0u, service_.method(200).last_channel_id());
}

TEST_F(BasicServer, ProcessPacket_InvalidMethod_SendsError) {
  EXPECT_EQ(Status::Ok(),
            server_.ProcessPacket(EncodeRequest(PacketType::REQUEST, 1, 42, 27),
                                  output_));

  const Packet& packet = output_.sent_packet();
  EXPECT_EQ(packet.type(), PacketType::SERVER_ERROR);
  EXPECT_EQ(packet.channel_id(), 1u);
  EXPECT_EQ(packet.service_id(), 42u);
  EXPECT_EQ(packet.method_id(), 27u);  // No method ID 27
  EXPECT_EQ(packet.status(), Status::NotFound());
}

TEST_F(BasicServer, ProcessPacket_InvalidService_SendsError) {
  EXPECT_EQ(Status::Ok(),
            server_.ProcessPacket(EncodeRequest(PacketType::REQUEST, 1, 43, 27),
                                  output_));

  const Packet& packet = output_.sent_packet();
  EXPECT_EQ(packet.type(), PacketType::SERVER_ERROR);
  EXPECT_EQ(packet.channel_id(), 1u);
  EXPECT_EQ(packet.service_id(), 43u);  // No service ID 43
  EXPECT_EQ(packet.method_id(), 27u);
  EXPECT_EQ(packet.status(), Status::NotFound());
}

TEST_F(BasicServer, ProcessPacket_UnassignedChannel_AssignsToAvailableSlot) {
  TestOutput<128> unassigned_output;
  EXPECT_EQ(Status::Ok(),
            server_.ProcessPacket(
                EncodeRequest(PacketType::REQUEST, /*channel_id=*/99, 42, 100),
                unassigned_output));
  EXPECT_EQ(channels_[2].id(), 99u);
}

TEST_F(BasicServer,
       ProcessPacket_UnassignedChannel_SendsResourceExhaustedIfCannotAssign) {
  channels_[2] = Channel::Create<3>(&output_);  // Occupy only available channel

  EXPECT_EQ(Status::Ok(),
            server_.ProcessPacket(
                EncodeRequest(PacketType::REQUEST, /*channel_id=*/99, 42, 27),
                output_));

  const Packet& packet = output_.sent_packet();
  EXPECT_EQ(packet.status(), Status::ResourceExhausted());
  EXPECT_EQ(packet.channel_id(), 99u);
  EXPECT_EQ(packet.service_id(), 42u);
  EXPECT_EQ(packet.method_id(), 27u);
}

TEST_F(BasicServer, ProcessPacket_Cancel_MethodNotActive_SendsError) {
  // Set up a fake ServerWriter representing an ongoing RPC.
  EXPECT_EQ(Status::Ok(),
            server_.ProcessPacket(
                EncodeRequest(PacketType::CANCEL_SERVER_STREAM, 1, 42, 100),
                output_));

  const Packet& packet = output_.sent_packet();
  EXPECT_EQ(packet.type(), PacketType::SERVER_ERROR);
  EXPECT_EQ(packet.channel_id(), 1u);
  EXPECT_EQ(packet.service_id(), 42u);
  EXPECT_EQ(packet.method_id(), 100u);
  EXPECT_EQ(packet.status(), Status::FailedPrecondition());
}

class MethodPending : public BasicServer {
 protected:
  MethodPending()
      : call_(static_cast<internal::Server&>(server_),
              static_cast<internal::Channel&>(channels_[0]),
              service_,
              service_.method(100)),
        writer_(call_) {
    ASSERT_TRUE(writer_.open());
  }

  internal::ServerCall call_;
  internal::BaseServerWriter writer_;
};

TEST_F(MethodPending, ProcessPacket_Cancel_ClosesServerWriter) {
  EXPECT_EQ(Status::Ok(),
            server_.ProcessPacket(
                EncodeRequest(PacketType::CANCEL_SERVER_STREAM, 1, 42, 100),
                output_));

  EXPECT_FALSE(writer_.open());
}

TEST_F(MethodPending, ProcessPacket_Cancel_SendsStreamEndPacket) {
  EXPECT_EQ(Status::Ok(),
            server_.ProcessPacket(
                EncodeRequest(PacketType::CANCEL_SERVER_STREAM, 1, 42, 100),
                output_));

  const Packet& packet = output_.sent_packet();
  EXPECT_EQ(packet.type(), PacketType::SERVER_STREAM_END);
  EXPECT_EQ(packet.channel_id(), 1u);
  EXPECT_EQ(packet.service_id(), 42u);
  EXPECT_EQ(packet.method_id(), 100u);
  EXPECT_TRUE(packet.payload().empty());
  EXPECT_EQ(packet.status(), Status::Cancelled());
}

TEST_F(MethodPending,
       ProcessPacket_ClientError_ClosesServerWriterWithoutStreamEnd) {
  EXPECT_EQ(Status::OK,
            server_.ProcessPacket(
                EncodeRequest(PacketType::CLIENT_ERROR, 1, 42, 100), output_));

  EXPECT_FALSE(writer_.open());
  EXPECT_EQ(output_.packet_count(), 0u);
}

TEST_F(MethodPending, ProcessPacket_Cancel_IncorrectChannel) {
  EXPECT_EQ(Status::Ok(),
            server_.ProcessPacket(
                EncodeRequest(PacketType::CANCEL_SERVER_STREAM, 2, 42, 100),
                output_));

  EXPECT_EQ(output_.sent_packet().type(), PacketType::SERVER_ERROR);
  EXPECT_EQ(output_.sent_packet().status(), Status::FailedPrecondition());
  EXPECT_TRUE(writer_.open());
}

TEST_F(MethodPending, ProcessPacket_Cancel_IncorrectService) {
  EXPECT_EQ(Status::Ok(),
            server_.ProcessPacket(
                EncodeRequest(PacketType::CANCEL_SERVER_STREAM, 1, 43, 100),
                output_));

  EXPECT_EQ(output_.sent_packet().type(), PacketType::SERVER_ERROR);
  EXPECT_EQ(output_.sent_packet().status(), Status::NotFound());
  EXPECT_EQ(output_.sent_packet().service_id(), 43u);
  EXPECT_EQ(output_.sent_packet().method_id(), 100u);
  EXPECT_TRUE(writer_.open());
}

TEST_F(MethodPending, ProcessPacket_CancelIncorrectMethod) {
  EXPECT_EQ(Status::Ok(),
            server_.ProcessPacket(
                EncodeRequest(PacketType::CANCEL_SERVER_STREAM, 1, 42, 101),
                output_));
  EXPECT_EQ(output_.sent_packet().type(), PacketType::SERVER_ERROR);
  EXPECT_EQ(output_.sent_packet().status(), Status::NotFound());
  EXPECT_TRUE(writer_.open());
}

}  // namespace
}  // namespace pw::rpc
