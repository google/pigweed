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

#include "pw_rpc/raw/internal/method.h"

#include <array>

#include "gtest/gtest.h"
#include "pw_bytes/array.h"
#include "pw_protobuf/decoder.h"
#include "pw_protobuf/encoder.h"
#include "pw_rpc/internal/method_impl_tester.h"
#include "pw_rpc/internal/test_utils.h"
#include "pw_rpc/raw/internal/method_union.h"
#include "pw_rpc/server_context.h"
#include "pw_rpc/service.h"
#include "pw_rpc_test_protos/test.pwpb.h"

namespace pw::rpc::internal {
namespace {

namespace TestRequest = ::pw::rpc::test::TestRequest;
namespace TestResponse = ::pw::rpc::test::TestResponse;

// Create a fake service for use with the MethodImplTester.
class TestRawService final : public Service {
 public:
  // Unary signatures

  StatusWithSize Unary(ServerContext&, ConstByteSpan, ByteSpan) {
    return StatusWithSize(0);
  }

  static StatusWithSize StaticUnary(ServerContext&, ConstByteSpan, ByteSpan) {
    return StatusWithSize(0);
  }

  StatusWithSize UnaryWrongArg(ServerContext&, ConstByteSpan, ConstByteSpan) {
    return StatusWithSize(0);
  }

  // Server streaming signatures

  void ServerStreaming(ServerContext&, ConstByteSpan, RawServerWriter&) {}

  static void StaticServerStreaming(ServerContext&,
                                    ConstByteSpan,
                                    RawServerWriter&) {}

  static void StaticUnaryVoidReturn(ServerContext&, ConstByteSpan, ByteSpan) {}

  Status ServerStreamingBadReturn(ServerContext&,
                                  ConstByteSpan,
                                  RawServerWriter&) {
    return Status();
  }

  static void StaticServerStreamingMissingArg(ConstByteSpan, RawServerWriter&) {
  }

  // Client streaming signatures

  void ClientStreaming(ServerContext&, RawServerReader&) {}

  static void StaticClientStreaming(ServerContext&, RawServerReader&) {}

  int ClientStreamingBadReturn(ServerContext&, RawServerReader&) { return 0; }

  static void StaticClientStreamingMissingArg(RawServerReader&) {}

  // Bidirectional streaming signatures

  void BidirectionalStreaming(ServerContext&, RawServerReaderWriter&) {}

  static void StaticBidirectionalStreaming(ServerContext&,
                                           RawServerReaderWriter&) {}

  int BidirectionalStreamingBadReturn(ServerContext&, RawServerReaderWriter&) {
    return 0;
  }

  static void StaticBidirectionalStreamingMissingArg(RawServerReaderWriter&) {}
};

static_assert(MethodImplTests<RawMethod, TestRawService>().Pass());

struct {
  int64_t integer;
  uint32_t status_code;
} last_request;

RawServerWriter last_writer;

void DecodeRawTestRequest(ConstByteSpan request) {
  protobuf::Decoder decoder(request);

  while (decoder.Next().ok()) {
    TestRequest::Fields field =
        static_cast<TestRequest::Fields>(decoder.FieldNumber());

    switch (field) {
      case TestRequest::Fields::INTEGER:
        ASSERT_EQ(OkStatus(), decoder.ReadInt64(&last_request.integer));
        break;
      case TestRequest::Fields::STATUS_CODE:
        ASSERT_EQ(OkStatus(), decoder.ReadUint32(&last_request.status_code));
        break;
    }
  }
};

StatusWithSize AddFive(ServerContext&,
                       ConstByteSpan request,
                       ByteSpan response) {
  DecodeRawTestRequest(request);

  TestResponse::MemoryEncoder test_response(response);
  EXPECT_EQ(OkStatus(), test_response.WriteValue(last_request.integer + 5));
  ConstByteSpan payload(test_response);

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
  stream::MemoryWriter writer(buffer);
  TestRequest::StreamEncoder test_request(writer, ByteSpan());
  ASSERT_EQ(OkStatus(), test_request.WriteInteger(456));
  ASSERT_EQ(OkStatus(), test_request.WriteStatusCode(7));

  const RawMethod& method = std::get<0>(FakeService::kMethods).raw_method();
  ServerContextForTest<FakeService> context(method);
  method.Invoke(context.get(), context.request(writer.WrittenData()));

  EXPECT_EQ(last_request.integer, 456);
  EXPECT_EQ(last_request.status_code, 7u);

  const Packet& response = context.output().sent_packet();
  EXPECT_EQ(response.status(), Status::Unauthenticated());

  protobuf::Decoder decoder(response.payload());
  ASSERT_TRUE(decoder.Next().ok());
  int64_t value;
  EXPECT_EQ(decoder.ReadInt64(&value), OkStatus());
  EXPECT_EQ(value, 461);
}

TEST(RawMethod, ServerStreamingRpc_SendsNothingWhenInitiallyCalled) {
  std::byte buffer[16];
  stream::MemoryWriter writer(buffer);
  TestRequest::StreamEncoder test_request(writer, ByteSpan());
  ASSERT_EQ(OkStatus(), test_request.WriteInteger(777));
  ASSERT_EQ(OkStatus(), test_request.WriteStatusCode(2));

  const RawMethod& method = std::get<1>(FakeService::kMethods).raw_method();
  ServerContextForTest<FakeService> context(method);

  method.Invoke(context.get(), context.request(writer.WrittenData()));

  EXPECT_EQ(0u, context.output().packet_count());
  EXPECT_EQ(777, last_request.integer);
  EXPECT_EQ(2u, last_request.status_code);
  EXPECT_TRUE(last_writer.open());
  EXPECT_EQ(OkStatus(), last_writer.Finish());
}

TEST(RawServerWriter, Write_SendsPreviouslyAcquiredBuffer) {
  const RawMethod& method = std::get<1>(FakeService::kMethods).raw_method();
  ServerContextForTest<FakeService> context(method);

  method.Invoke(context.get(), context.request({}));

  auto buffer = last_writer.PayloadBuffer();

  constexpr auto data = bytes::Array<0x0d, 0x06, 0xf0, 0x0d>();
  std::memcpy(buffer.data(), data.data(), data.size());

  EXPECT_EQ(last_writer.Write(buffer.first(data.size())), OkStatus());

  const internal::Packet& packet = context.output().sent_packet();
  EXPECT_EQ(packet.type(), internal::PacketType::SERVER_STREAM);
  EXPECT_EQ(packet.channel_id(), context.channel_id());
  EXPECT_EQ(packet.service_id(), context.service_id());
  EXPECT_EQ(packet.method_id(), context.get().method().id());
  EXPECT_EQ(std::memcmp(packet.payload().data(), data.data(), data.size()), 0);
  EXPECT_EQ(packet.status(), OkStatus());
}

TEST(RawServerWriter, Write_SendsExternalBuffer) {
  const RawMethod& method = std::get<1>(FakeService::kMethods).raw_method();
  ServerContextForTest<FakeService> context(method);

  method.Invoke(context.get(), context.request({}));

  constexpr auto data = bytes::Array<0x0d, 0x06, 0xf0, 0x0d>();
  EXPECT_EQ(last_writer.Write(data), OkStatus());

  const internal::Packet& packet = context.output().sent_packet();
  EXPECT_EQ(packet.type(), internal::PacketType::SERVER_STREAM);
  EXPECT_EQ(packet.channel_id(), context.channel_id());
  EXPECT_EQ(packet.service_id(), context.service_id());
  EXPECT_EQ(packet.method_id(), context.get().method().id());
  EXPECT_EQ(std::memcmp(packet.payload().data(), data.data(), data.size()), 0);
  EXPECT_EQ(packet.status(), OkStatus());
}

TEST(RawServerWriter, Write_EmptyBuffer) {
  const RawMethod& method = std::get<1>(FakeService::kMethods).raw_method();
  ServerContextForTest<FakeService> context(method);

  method.Invoke(context.get(), context.request({}));

  ASSERT_EQ(last_writer.Write({}), OkStatus());

  const internal::Packet& packet = context.output().sent_packet();
  EXPECT_EQ(packet.type(), internal::PacketType::SERVER_STREAM);
  EXPECT_EQ(packet.channel_id(), context.channel_id());
  EXPECT_EQ(packet.service_id(), context.service_id());
  EXPECT_EQ(packet.method_id(), context.get().method().id());
  EXPECT_TRUE(packet.payload().empty());
  EXPECT_EQ(packet.status(), OkStatus());
}

TEST(RawServerWriter, Write_Closed_ReturnsFailedPrecondition) {
  const RawMethod& method = std::get<1>(FakeService::kMethods).raw_method();
  ServerContextForTest<FakeService> context(method);

  method.Invoke(context.get(), context.request({}));

  EXPECT_EQ(OkStatus(), last_writer.Finish());
  constexpr auto data = bytes::Array<0x0d, 0x06, 0xf0, 0x0d>();
  EXPECT_EQ(last_writer.Write(data), Status::FailedPrecondition());
}

TEST(RawServerWriter, Write_BufferTooSmall_ReturnsOutOfRange) {
  const RawMethod& method = std::get<1>(FakeService::kMethods).raw_method();
  ServerContextForTest<FakeService, 16> context(method);

  method.Invoke(context.get(), context.request({}));

  constexpr auto data =
      bytes::Array<0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16>();
  EXPECT_EQ(last_writer.Write(data), Status::OutOfRange());
}

TEST(RawServerWriter,
     Destructor_ReleasesAcquiredBufferWithoutSendingAndCloses) {
  const RawMethod& method = std::get<1>(FakeService::kMethods).raw_method();
  ServerContextForTest<FakeService> context(method);

  method.Invoke(context.get(), context.request({}));

  {
    RawServerWriter writer = std::move(last_writer);
    auto buffer = writer.PayloadBuffer();
    buffer[0] = std::byte{'!'};
    // Don't release the buffer.
  }

  auto output = context.output();
  EXPECT_EQ(output.packet_count(), 1u);
  EXPECT_EQ(output.sent_packet().type(), PacketType::RESPONSE);
}

}  // namespace
}  // namespace pw::rpc::internal
