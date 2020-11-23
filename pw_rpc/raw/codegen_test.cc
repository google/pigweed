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
#include "pw_rpc/raw_test_method_context.h"
#include "pw_rpc_test_protos/test.pwpb.h"
#include "pw_rpc_test_protos/test.raw_rpc.pb.h"

namespace pw::rpc {
namespace test {

class TestService final : public generated::TestService<TestService> {
 public:
  static StatusWithSize TestRpc(ServerContext&,
                                ConstByteSpan request,
                                ByteSpan response) {
    int64_t integer;
    Status status;
    DecodeRequest(request, integer, status);

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
    DecodeRequest(request, integer, status);

    for (int i = 0; i < integer; ++i) {
      ByteSpan buffer = writer.PayloadBuffer();
      protobuf::NestedEncoder encoder(buffer);
      TestStreamResponse::Encoder test_stream_response(&encoder);
      test_stream_response.WriteNumber(i);
      writer.Write(encoder.Encode().value());
    }

    writer.Finish(status);
  }

 private:
  static void DecodeRequest(ConstByteSpan request,
                            int64_t& integer,
                            Status& status) {
    protobuf::Decoder decoder(request);

    while (decoder.Next().ok()) {
      switch (static_cast<TestRequest::Fields>(decoder.FieldNumber())) {
        case TestRequest::Fields::INTEGER:
          decoder.ReadInt64(&integer);
          break;
        case TestRequest::Fields::STATUS_CODE: {
          uint32_t status_code;
          decoder.ReadUint32(&status_code);
          status = static_cast<Status::Code>(status_code);
          break;
        }
      }
    }
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

  std::byte buffer[64];
  protobuf::NestedEncoder encoder(buffer);
  test::TestRequest::Encoder test_request(&encoder);
  test_request.WriteInteger(123);
  test_request.WriteStatusCode(Status::Ok().code());

  auto sws = context.call(encoder.Encode().value());
  EXPECT_EQ(Status::Ok(), sws.status());

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

  std::byte buffer[64];
  protobuf::NestedEncoder encoder(buffer);
  test::TestRequest::Encoder test_request(&encoder);
  test_request.WriteInteger(5);
  test_request.WriteStatusCode(Status::Unauthenticated().code());

  context.call(encoder.Encode().value());
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

}  // namespace
}  // namespace pw::rpc
