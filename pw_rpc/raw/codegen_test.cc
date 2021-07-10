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

#include "gtest/gtest.h"
#include "pw_protobuf/decoder.h"
#include "pw_rpc/internal/hash.h"
#include "pw_rpc/raw/test_method_context.h"
#include "pw_rpc_test_protos/test.pwpb.h"
#include "pw_rpc_test_protos/test.raw_rpc.pb.h"

namespace pw::rpc {
namespace {

Vector<std::byte, 64> EncodeRequest(int integer, Status status) {
  Vector<std::byte, 64> buffer(64);
  test::TestRequest::RamEncoder test_request(buffer);

  test_request.WriteInteger(integer);
  test_request.WriteStatusCode(status.code());

  EXPECT_EQ(OkStatus(), test_request.status());
  buffer.resize(test_request.size());
  return buffer;
}

Vector<std::byte, 64> EncodeResponse(int number) {
  Vector<std::byte, 64> buffer(64);
  test::TestStreamResponse::RamEncoder test_response(buffer);

  test_response.WriteNumber(number);

  EXPECT_EQ(OkStatus(), test_response.status());
  buffer.resize(test_response.size());
  return buffer;
}

}  // namespace

namespace test {

class TestService final : public generated::TestService<TestService> {
 public:
  static StatusWithSize TestRpc(ServerContext&,
                                ConstByteSpan request,
                                ByteSpan response) {
    int64_t integer;
    Status status;

    if (!DecodeRequest(request, integer, status)) {
      return StatusWithSize::DataLoss();
    }

    protobuf::NestedEncoder encoder(response);
    TestResponse::Encoder test_response(&encoder);
    test_response.WriteValue(integer + 1);

    return StatusWithSize(status, encoder.Encode().value().size());
  }

  void TestStreamRpc(ServerContext&,
                     ConstByteSpan request,
                     RawServerWriter& writer) {
    int64_t integer;
    Status status;

    ASSERT_TRUE(DecodeRequest(request, integer, status));
    for (int i = 0; i < integer; ++i) {
      ByteSpan buffer = writer.PayloadBuffer();
      protobuf::NestedEncoder encoder(buffer);
      TestStreamResponse::Encoder test_stream_response(&encoder);
      test_stream_response.WriteNumber(i);
      writer.Write(encoder.Encode().value());
    }

    writer.Finish(status);
  }

  void TestClientStreamRpc(ServerContext&, RawServerReader& reader) {
    last_reader_ = std::move(reader);

    last_reader_.set_on_next([this](ConstByteSpan payload) {
      last_reader_.Finish(EncodeResponse(ReadInteger(payload)),
                          Status::Unauthenticated());
    });
  }

  void TestBidirectionalStreamRpc(ServerContext&,
                                  RawServerReaderWriter& reader_writer) {
    last_reader_writer_ = std::move(reader_writer);

    last_reader_writer_.set_on_next([this](ConstByteSpan payload) {
      last_reader_writer_.Write(EncodeResponse(ReadInteger(payload)));
      last_reader_writer_.Finish(Status::NotFound());
    });
  }

 protected:
  RawServerReader last_reader_;
  RawServerReaderWriter last_reader_writer_;

 private:
  static uint32_t ReadInteger(ConstByteSpan request) {
    uint32_t integer = 0;

    protobuf::Decoder decoder(request);
    while (decoder.Next().ok()) {
      switch (static_cast<TestRequest::Fields>(decoder.FieldNumber())) {
        case TestRequest::Fields::INTEGER:
          EXPECT_EQ(OkStatus(), decoder.ReadUint32(&integer));
          break;
        case TestRequest::Fields::STATUS_CODE:
          break;
        default:
          ADD_FAILURE();
      }
    }

    return integer;
  }

  static bool DecodeRequest(ConstByteSpan request,
                            int64_t& integer,
                            Status& status) {
    protobuf::Decoder decoder(request);
    Status decode_status;
    bool has_integer = false;
    bool has_status = false;

    while (decoder.Next().ok()) {
      switch (static_cast<TestRequest::Fields>(decoder.FieldNumber())) {
        case TestRequest::Fields::INTEGER:
          decode_status = decoder.ReadInt64(&integer);
          EXPECT_EQ(OkStatus(), decode_status);
          has_integer = decode_status.ok();
          break;
        case TestRequest::Fields::STATUS_CODE: {
          uint32_t status_code;
          decode_status = decoder.ReadUint32(&status_code);
          EXPECT_EQ(OkStatus(), decode_status);
          has_status = decode_status.ok();
          status = static_cast<Status::Code>(status_code);
          break;
        }
      }
    }
    EXPECT_TRUE(has_integer);
    EXPECT_TRUE(has_status);
    return has_integer && has_status;
  }
};

}  // namespace test

namespace {

TEST(RawCodegen, CompilesProperly) {
  test::TestService service;
  EXPECT_EQ(service.id(), internal::Hash("pw.rpc.test.TestService"));
  EXPECT_STREQ(service.name(), "TestService");
}

TEST(RawCodegen, Server_InvokeUnaryRpc) {
  PW_RAW_TEST_METHOD_CONTEXT(test::TestService, TestRpc) context;

  auto sws = context.call(EncodeRequest(123, OkStatus()));
  EXPECT_EQ(OkStatus(), sws.status());

  protobuf::Decoder decoder(context.response());

  while (decoder.Next().ok()) {
    switch (static_cast<test::TestResponse::Fields>(decoder.FieldNumber())) {
      case test::TestResponse::Fields::VALUE: {
        int32_t value;
        decoder.ReadInt32(&value);
        EXPECT_EQ(value, 124);
        break;
      }
    }
  }
}

TEST(RawCodegen, Server_InvokeServerStreamingRpc) {
  PW_RAW_TEST_METHOD_CONTEXT(test::TestService, TestStreamRpc) context;

  context.call(EncodeRequest(5, Status::Unauthenticated()));
  EXPECT_TRUE(context.done());
  EXPECT_EQ(Status::Unauthenticated(), context.status());
  EXPECT_EQ(context.total_responses(), 5u);

  protobuf::Decoder decoder(context.responses().back());
  while (decoder.Next().ok()) {
    switch (
        static_cast<test::TestStreamResponse::Fields>(decoder.FieldNumber())) {
      case test::TestStreamResponse::Fields::NUMBER: {
        int32_t value;
        decoder.ReadInt32(&value);
        EXPECT_EQ(value, 4);
        break;
      }
      case test::TestStreamResponse::Fields::CHUNK:
        FAIL();
        break;
    }
  }
}

int32_t ReadResponseNumber(ConstByteSpan data) {
  int32_t value = -1;
  protobuf::Decoder decoder(data);
  while (decoder.Next().ok()) {
    switch (
        static_cast<test::TestStreamResponse::Fields>(decoder.FieldNumber())) {
      case test::TestStreamResponse::Fields::NUMBER: {
        decoder.ReadInt32(&value);
        break;
      }
      default:
        ADD_FAILURE();
        break;
    }
  }

  return value;
}

TEST(RawCodegen, Server_InvokeClientStreamingRpc) {
  PW_RAW_TEST_METHOD_CONTEXT(test::TestService, TestClientStreamRpc) ctx;

  ctx.call();
  ctx.SendClientStream(EncodeRequest(123, OkStatus()));

  ASSERT_TRUE(ctx.done());
  EXPECT_EQ(Status::Unauthenticated(), ctx.status());
  EXPECT_EQ(ctx.total_responses(), 1u);
  EXPECT_EQ(ReadResponseNumber(ctx.responses().back()), 123);
}

TEST(RawCodegen, Server_InvokeBidirectionalStreamingRpc) {
  PW_RAW_TEST_METHOD_CONTEXT(test::TestService, TestBidirectionalStreamRpc)
  ctx;

  ctx.call();
  ctx.SendClientStream(EncodeRequest(456, OkStatus()));

  ASSERT_TRUE(ctx.done());
  EXPECT_EQ(Status::NotFound(), ctx.status());
  ASSERT_EQ(ctx.total_responses(), 1u);
  EXPECT_EQ(ReadResponseNumber(ctx.responses().back()), 456);
}

}  // namespace
}  // namespace pw::rpc
