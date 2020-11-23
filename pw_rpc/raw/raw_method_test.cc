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

#include "pw_rpc/internal/raw_method.h"

#include <array>

#include "gtest/gtest.h"
#include "pw_bytes/array.h"
#include "pw_protobuf/decoder.h"
#include "pw_protobuf/encoder.h"
#include "pw_rpc/internal/raw_method_union.h"
#include "pw_rpc/server_context.h"
#include "pw_rpc/service.h"
#include "pw_rpc_private/internal_test_utils.h"
#include "pw_rpc_private/method_impl_tester.h"
#include "pw_rpc_test_protos/test.pwpb.h"

namespace pw::rpc::internal {
namespace {

// Create a fake service for use with the MethodImplTester.
class TestRawService final : public Service {
 public:
  StatusWithSize Unary(ServerContext&, ConstByteSpan, ByteSpan) {
    return StatusWithSize(0);
  }

  static StatusWithSize StaticUnary(ServerContext&, ConstByteSpan, ByteSpan) {
    return StatusWithSize(0);
  }

  void ServerStreaming(ServerContext&, ConstByteSpan, RawServerWriter&) {}

  static void StaticServerStreaming(ServerContext&,
                                    ConstByteSpan,
                                    RawServerWriter&) {}

  StatusWithSize UnaryWrongArg(ServerContext&, ConstByteSpan, ConstByteSpan) {
    return StatusWithSize(0);
  }

  static void StaticUnaryVoidReturn(ServerContext&, ConstByteSpan, ByteSpan) {}

  Status ServerStreamingBadReturn(ServerContext&,
                                  ConstByteSpan,
                                  RawServerWriter&) {
    return Status();
  }

  static void StaticServerStreamingMissingArg(ConstByteSpan, RawServerWriter&) {
  }
};

TEST(MethodImplTester, RawMethod) {
  constexpr MethodImplTester<RawMethod, TestRawService> method_tester;
  EXPECT_TRUE(method_tester.MethodImplIsValid());
}

struct {
  int64_t integer;
  uint32_t status_code;
} last_request;
RawServerWriter last_writer;

void DecodeRawTestRequest(ConstByteSpan request) {
  protobuf::Decoder decoder(request);

  while (decoder.Next().ok()) {
    test::TestRequest::Fields field =
        static_cast<test::TestRequest::Fields>(decoder.FieldNumber());

    switch (field) {
      case test::TestRequest::Fields::INTEGER:
        decoder.ReadInt64(&last_request.integer);
        break;
      case test::TestRequest::Fields::STATUS_CODE:
        decoder.ReadUint32(&last_request.status_code);
        break;
    }
  }
};

StatusWithSize AddFive(ServerContext&,
                       ConstByteSpan request,
                       ByteSpan response) {
  DecodeRawTestRequest(request);

  protobuf::NestedEncoder encoder(response);
  test::TestResponse::Encoder test_response(&encoder);
  test_response.WriteValue(last_request.integer + 5);
  ConstByteSpan payload;
  encoder.Encode(&payload);

  return StatusWithSize::Unauthenticated(payload.size());
}

void StartStream(ServerContext&,
                 ConstByteSpan request,
                 RawServerWriter& writer) {
  DecodeRawTestRequest(request);
  last_writer = std::move(writer);
}

class FakeService : public Service {
 public:
  FakeService(uint32_t id) : Service(id, kMethods) {}

  static constexpr std::array<RawMethodUnion, 2> kMethods = {
      RawMethod::Unary<AddFive>(10u),
      RawMethod::ServerStreaming<StartStream>(11u),
  };
};

TEST(RawMethod, UnaryRpc_SendsResponse) {
  std::byte buffer[16];
  protobuf::NestedEncoder encoder(buffer);
  test::TestRequest::Encoder test_request(&encoder);
  test_request.WriteInteger(456);
  test_request.WriteStatusCode(7);

  const RawMethod& method = std::get<0>(FakeService::kMethods).raw_method();
  ServerContextForTest<FakeService> context(method);
  method.Invoke(context.get(), context.packet(encoder.Encode().value()));

  EXPECT_EQ(last_request.integer, 456);
  EXPECT_EQ(last_request.status_code, 7u);

  const Packet& response = context.output().sent_packet();
  EXPECT_EQ(response.status(), Status::Unauthenticated());

  protobuf::Decoder decoder(response.payload());
  ASSERT_TRUE(decoder.Next().ok());
  int64_t value;
  EXPECT_EQ(decoder.ReadInt64(&value), Status::Ok());
  EXPECT_EQ(value, 461);
}

TEST(RawMethod, ServerStreamingRpc_SendsNothingWhenInitiallyCalled) {
  std::byte buffer[16];
  protobuf::NestedEncoder encoder(buffer);
  test::TestRequest::Encoder test_request(&encoder);
  test_request.WriteInteger(777);
  test_request.WriteStatusCode(2);

  const RawMethod& method = std::get<1>(FakeService::kMethods).raw_method();
  ServerContextForTest<FakeService> context(method);

  method.Invoke(context.get(), context.packet(encoder.Encode().value()));

  EXPECT_EQ(0u, context.output().packet_count());
  EXPECT_EQ(777, last_request.integer);
  EXPECT_EQ(2u, last_request.status_code);
  EXPECT_TRUE(last_writer.open());
  last_writer.Finish();
}

TEST(RawServerWriter, Write_SendsPreviouslyAcquiredBuffer) {
  const RawMethod& method = std::get<1>(FakeService::kMethods).raw_method();
  ServerContextForTest<FakeService> context(method);

  method.Invoke(context.get(), context.packet({}));

  auto buffer = last_writer.PayloadBuffer();

  constexpr auto data = bytes::Array<0x0d, 0x06, 0xf0, 0x0d>();
  std::memcpy(buffer.data(), data.data(), data.size());

  EXPECT_EQ(last_writer.Write(buffer.first(data.size())), Status::Ok());

  const internal::Packet& packet = context.output().sent_packet();
  EXPECT_EQ(packet.type(), internal::PacketType::RESPONSE);
  EXPECT_EQ(packet.channel_id(), context.kChannelId);
  EXPECT_EQ(packet.service_id(), context.kServiceId);
  EXPECT_EQ(packet.method_id(), context.get().method().id());
  EXPECT_EQ(std::memcmp(packet.payload().data(), data.data(), data.size()), 0);
  EXPECT_EQ(packet.status(), Status::Ok());
}

TEST(RawServerWriter, Write_SendsExternalBuffer) {
  const RawMethod& method = std::get<1>(FakeService::kMethods).raw_method();
  ServerContextForTest<FakeService> context(method);

  method.Invoke(context.get(), context.packet({}));

  constexpr auto data = bytes::Array<0x0d, 0x06, 0xf0, 0x0d>();
  EXPECT_EQ(last_writer.Write(data), Status::Ok());

  const internal::Packet& packet = context.output().sent_packet();
  EXPECT_EQ(packet.type(), internal::PacketType::RESPONSE);
  EXPECT_EQ(packet.channel_id(), context.kChannelId);
  EXPECT_EQ(packet.service_id(), context.kServiceId);
  EXPECT_EQ(packet.method_id(), context.get().method().id());
  EXPECT_EQ(std::memcmp(packet.payload().data(), data.data(), data.size()), 0);
  EXPECT_EQ(packet.status(), Status::Ok());
}

TEST(RawServerWriter, Write_Closed_ReturnsFailedPrecondition) {
  const RawMethod& method = std::get<1>(FakeService::kMethods).raw_method();
  ServerContextForTest<FakeService, 16> context(method);

  method.Invoke(context.get(), context.packet({}));

  last_writer.Finish();
  constexpr auto data = bytes::Array<0x0d, 0x06, 0xf0, 0x0d>();
  EXPECT_EQ(last_writer.Write(data), Status::FailedPrecondition());
}

TEST(RawServerWriter, Write_BufferTooSmall_ReturnsOutOfRange) {
  const RawMethod& method = std::get<1>(FakeService::kMethods).raw_method();
  ServerContextForTest<FakeService, 16> context(method);

  method.Invoke(context.get(), context.packet({}));

  constexpr auto data =
      bytes::Array<0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16>();
  EXPECT_EQ(last_writer.Write(data), Status::OutOfRange());
}

TEST(RawServerWriter,
     Destructor_ReleasesAcquiredBufferWithoutSendingAndCloses) {
  const RawMethod& method = std::get<1>(FakeService::kMethods).raw_method();
  ServerContextForTest<FakeService> context(method);

  method.Invoke(context.get(), context.packet({}));

  {
    RawServerWriter writer = std::move(last_writer);
    auto buffer = writer.PayloadBuffer();
    buffer[0] = std::byte{'!'};
    // Don't release the buffer.
  }

  auto output = context.output();
  EXPECT_EQ(output.packet_count(), 1u);
  EXPECT_EQ(output.sent_packet().type(), PacketType::SERVER_STREAM_END);
}

}  // namespace
}  // namespace pw::rpc::internal
