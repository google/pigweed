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
#include "pw_rpc/internal/hash.h"
#include "pw_rpc/nanopb_test_method_context.h"
#include "pw_rpc_nanopb_private/internal_test_utils.h"
#include "pw_rpc_private/internal_test_utils.h"
#include "pw_rpc_test_protos/test.rpc.pb.h"

namespace pw::rpc {
namespace test {

class TestService final : public generated::TestService<TestService> {
 public:
  Status TestRpc(ServerContext&,
                 const pw_rpc_test_TestRequest& request,
                 pw_rpc_test_TestResponse& response) {
    response.value = request.integer + 1;
    return static_cast<Status::Code>(request.status_code);
  }

  static void TestStreamRpc(
      ServerContext&,
      const pw_rpc_test_TestRequest& request,
      ServerWriter<pw_rpc_test_TestStreamResponse>& writer) {
    for (int i = 0; i < request.integer; ++i) {
      writer.Write({.chunk = {}, .number = static_cast<uint32_t>(i)});
    }

    writer.Finish(static_cast<Status::Code>(request.status_code));
  }
};

}  // namespace test

namespace {

TEST(NanopbCodegen, CompilesProperly) {
  test::TestService service;
  EXPECT_EQ(service.id(), internal::Hash("pw.rpc.test.TestService"));
  EXPECT_STREQ(service.name(), "TestService");
}

TEST(NanopbCodegen, Server_InvokeUnaryRpc) {
  PW_NANOPB_TEST_METHOD_CONTEXT(test::TestService, TestRpc) context;

  EXPECT_EQ(Status::Ok(),
            context.call({.integer = 123, .status_code = Status::Ok().code()}));

  EXPECT_EQ(124, context.response().value);

  EXPECT_EQ(Status::InvalidArgument(),
            context.call({.integer = 999,
                          .status_code = Status::InvalidArgument().code()}));
  EXPECT_EQ(1000, context.response().value);
}

TEST(NanopbCodegen, Server_InvokeStreamingRpc) {
  PW_NANOPB_TEST_METHOD_CONTEXT(test::TestService, TestStreamRpc) context;

  context.call({.integer = 0, .status_code = Status::Aborted().code()});

  EXPECT_EQ(Status::Aborted(), context.status());
  EXPECT_TRUE(context.done());
  EXPECT_TRUE(context.responses().empty());
  EXPECT_EQ(0u, context.total_responses());

  context.call({.integer = 4, .status_code = Status::Ok().code()});

  ASSERT_EQ(4u, context.responses().size());
  ASSERT_EQ(4u, context.total_responses());

  for (size_t i = 0; i < context.responses().size(); ++i) {
    EXPECT_EQ(context.responses()[i].number, i);
  }

  EXPECT_EQ(Status::Ok().code(), context.status());
}

TEST(NanopbCodegen,
     Server_InvokeStreamingRpc_ContextKeepsFixedNumberOfResponses) {
  PW_NANOPB_TEST_METHOD_CONTEXT(test::TestService, TestStreamRpc, 3) context;

  ASSERT_EQ(3u, context.responses().max_size());

  context.call({.integer = 5, .status_code = Status::NotFound().code()});

  ASSERT_EQ(3u, context.responses().size());
  ASSERT_EQ(5u, context.total_responses());

  EXPECT_EQ(context.responses()[0].number, 0u);
  EXPECT_EQ(context.responses()[1].number, 1u);
  EXPECT_EQ(context.responses()[2].number, 4u);
}

TEST(NanopbCodegen, Server_InvokeStreamingRpc_ManualWriting) {
  PW_NANOPB_TEST_METHOD_CONTEXT(test::TestService, TestStreamRpc, 3) context;

  ASSERT_EQ(3u, context.responses().max_size());

  auto writer = context.writer();

  writer.Write({.chunk = {}, .number = 3});
  writer.Write({.chunk = {}, .number = 6});
  writer.Write({.chunk = {}, .number = 9});

  EXPECT_FALSE(context.done());

  writer.Finish(Status::Cancelled());
  ASSERT_TRUE(context.done());
  EXPECT_EQ(Status::Cancelled(), context.status());

  ASSERT_EQ(3u, context.responses().size());
  ASSERT_EQ(3u, context.total_responses());

  EXPECT_EQ(context.responses()[0].number, 3u);
  EXPECT_EQ(context.responses()[1].number, 6u);
  EXPECT_EQ(context.responses()[2].number, 9u);
}

using TestServiceClient = test::nanopb::TestServiceClient;
using internal::TestServerStreamingResponseHandler;
using internal::TestUnaryResponseHandler;

TEST(NanopbCodegen, Client_InvokesUnaryRpcWithCallback) {
  constexpr uint32_t service_id = internal::Hash("pw.rpc.test.TestService");
  constexpr uint32_t method_id = internal::Hash("TestRpc");

  ClientContextForTest<128, 128, 99, service_id, method_id> context;
  TestUnaryResponseHandler<pw_rpc_test_TestResponse> handler;

  auto call = TestServiceClient::TestRpc(
      context.channel(), {.integer = 123, .status_code = 0}, handler);
  EXPECT_EQ(context.output().packet_count(), 1u);
  auto packet = context.output().sent_packet();
  EXPECT_EQ(packet.channel_id(), context.channel().id());
  EXPECT_EQ(packet.service_id(), service_id);
  EXPECT_EQ(packet.method_id(), method_id);
  PW_DECODE_PB(pw_rpc_test_TestRequest, sent_proto, packet.payload());
  EXPECT_EQ(sent_proto.integer, 123);

  PW_ENCODE_PB(pw_rpc_test_TestResponse, response, .value = 42);
  context.SendResponse(Status::Ok(), response);
  ASSERT_EQ(handler.responses_received(), 1u);
  EXPECT_EQ(handler.last_status(), Status::Ok());
  EXPECT_EQ(handler.last_response().value, 42);
}

TEST(NanopbCodegen, Client_InvokesServerStreamingRpcWithCallback) {
  constexpr uint32_t service_id = internal::Hash("pw.rpc.test.TestService");
  constexpr uint32_t method_id = internal::Hash("TestStreamRpc");

  ClientContextForTest<128, 128, 99, service_id, method_id> context;
  TestServerStreamingResponseHandler<pw_rpc_test_TestStreamResponse> handler;

  auto call = TestServiceClient::TestStreamRpc(
      context.channel(), {.integer = 123, .status_code = 0}, handler);
  EXPECT_EQ(context.output().packet_count(), 1u);
  auto packet = context.output().sent_packet();
  EXPECT_EQ(packet.channel_id(), context.channel().id());
  EXPECT_EQ(packet.service_id(), service_id);
  EXPECT_EQ(packet.method_id(), method_id);
  PW_DECODE_PB(pw_rpc_test_TestRequest, sent_proto, packet.payload());
  EXPECT_EQ(sent_proto.integer, 123);

  PW_ENCODE_PB(
      pw_rpc_test_TestStreamResponse, response, .chunk = {}, .number = 11u);
  context.SendResponse(Status::Ok(), response);
  ASSERT_EQ(handler.responses_received(), 1u);
  EXPECT_EQ(handler.last_response().number, 11u);

  context.SendPacket(internal::PacketType::SERVER_STREAM_END,
                     Status::NotFound());
  EXPECT_FALSE(handler.active());
  EXPECT_EQ(handler.status(), Status::NotFound());
}

}  // namespace
}  // namespace pw::rpc
