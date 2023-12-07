// Copyright 2022 The Pigweed Authors
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

#include <atomic>

#include "pw_rpc/nanopb/client_server_testing_threaded.h"
#include "pw_rpc_test_protos/test.rpc.pb.h"
#include "pw_sync/binary_semaphore.h"
#include "pw_sync/mutex.h"
#include "pw_thread/non_portable_test_thread_options.h"
#include "pw_unit_test/framework.h"

namespace pw::rpc {
namespace {

using GeneratedService = ::pw::rpc::test::pw_rpc::nanopb::TestService;

class TestService final : public GeneratedService::Service<TestService> {
 public:
  Status TestUnaryRpc(const pw_rpc_test_TestRequest& request,
                      pw_rpc_test_TestResponse& response) {
    response.value = request.integer + 1;
    return static_cast<Status::Code>(request.status_code);
  }

  Status TestAnotherUnaryRpc(const pw_rpc_test_TestRequest& request,
                             pw_rpc_test_TestResponse& response) {
    typedef std::array<uint32_t, 3> ArgType;
    // The values array needs to be kept in memory until after this method call
    // returns since the response is not encoded until after returning from this
    // method.
    static const ArgType values = {7, 8, 9};
    response.repeated_field.funcs.encode = +[](pb_ostream_t* stream,
                                               const pb_field_t* field,
                                               void* const* arg) -> bool {
      // Note: nanopb passes the pointer to the repeated_filed.arg member as
      // arg, not its contents.
      for (auto elem : *static_cast<const ArgType*>(*arg)) {
        if (!pb_encode_tag_for_field(stream, field) ||
            !pb_encode_varint(stream, elem))
          return false;
      }
      return true;
    };
    response.repeated_field.arg = const_cast<ArgType*>(&values);
    return static_cast<Status::Code>(request.status_code);
  }

  static void TestServerStreamRpc(
      const pw_rpc_test_TestRequest&,
      ServerWriter<pw_rpc_test_TestStreamResponse>&) {}

  void TestClientStreamRpc(
      ServerReader<pw_rpc_test_TestRequest, pw_rpc_test_TestStreamResponse>&) {}

  void TestBidirectionalStreamRpc(
      ServerReaderWriter<pw_rpc_test_TestRequest,
                         pw_rpc_test_TestStreamResponse>&) {}
};

class RpcCaller {
 public:
  template <auto kMethod = GeneratedService::TestUnaryRpc>
  Status BlockOnResponse(uint32_t i, Client& client, uint32_t channel_id) {
    response_status_ = OkStatus();
    pw_rpc_test_TestRequest request{.integer = i,
                                    .status_code = OkStatus().code()};
    auto call = kMethod(
        client,
        channel_id,
        request,
        [this](const pw_rpc_test_TestResponse&, Status status) {
          response_status_ = status;
          semaphore_.release();
        },
        [this](Status status) {
          response_status_ = status;
          semaphore_.release();
        });

    semaphore_.acquire();
    return response_status_;
  }

 private:
  pw::sync::BinarySemaphore semaphore_;
  Status response_status_ = OkStatus();
};

TEST(NanopbClientServerTestContextThreaded, ReceivesUnaryRpcResponseThreaded) {
  NanopbClientServerTestContextThreaded<> ctx(
      // TODO: b/290860904 - Replace TestOptionsThread0 with TestThreadContext.
      thread::test::TestOptionsThread0());
  TestService service;
  ctx.server().RegisterService(service);

  RpcCaller caller;
  constexpr auto value = 1;
  EXPECT_EQ(caller.BlockOnResponse(value, ctx.client(), ctx.channel().id()),
            OkStatus());

  const auto request =
      ctx.request<test::pw_rpc::nanopb::TestService::TestUnaryRpc>(0);
  const auto response =
      ctx.response<test::pw_rpc::nanopb::TestService::TestUnaryRpc>(0);

  EXPECT_EQ(value, request.integer);
  EXPECT_EQ(value + 1, response.value);
}

TEST(NanopbClientServerTestContextThreaded, ReceivesMultipleResponsesThreaded) {
  NanopbClientServerTestContextThreaded<> ctx(
      thread::test::TestOptionsThread0());
  TestService service;
  ctx.server().RegisterService(service);

  RpcCaller caller;
  constexpr auto value1 = 1;
  constexpr auto value2 = 2;
  EXPECT_EQ(caller.BlockOnResponse(value1, ctx.client(), ctx.channel().id()),
            OkStatus());
  EXPECT_EQ(caller.BlockOnResponse(value2, ctx.client(), ctx.channel().id()),
            OkStatus());

  const auto request1 =
      ctx.request<test::pw_rpc::nanopb::TestService::TestUnaryRpc>(0);
  const auto request2 =
      ctx.request<test::pw_rpc::nanopb::TestService::TestUnaryRpc>(1);
  const auto response1 =
      ctx.response<test::pw_rpc::nanopb::TestService::TestUnaryRpc>(0);
  const auto response2 =
      ctx.response<test::pw_rpc::nanopb::TestService::TestUnaryRpc>(1);

  EXPECT_EQ(value1, request1.integer);
  EXPECT_EQ(value2, request2.integer);
  EXPECT_EQ(value1 + 1, response1.value);
  EXPECT_EQ(value2 + 1, response2.value);
}

TEST(NanopbClientServerTestContextThreaded,
     ReceivesMultipleResponsesThreadedWithPacketProcessor) {
  using ProtectedInt = std::pair<int, pw::sync::Mutex>;
  ProtectedInt server_counter{};
  auto server_processor = [&server_counter](
                              ClientServer& client_server,
                              pw::ConstByteSpan packet) -> pw::Status {
    server_counter.second.lock();
    ++server_counter.first;
    server_counter.second.unlock();
    return client_server.ProcessPacket(packet);
  };

  ProtectedInt client_counter{};
  auto client_processor = [&client_counter](
                              ClientServer& client_server,
                              pw::ConstByteSpan packet) -> pw::Status {
    client_counter.second.lock();
    ++client_counter.first;
    client_counter.second.unlock();
    return client_server.ProcessPacket(packet);
  };

  NanopbClientServerTestContextThreaded<> ctx(
      thread::test::TestOptionsThread0(), server_processor, client_processor);
  TestService service;
  ctx.server().RegisterService(service);

  RpcCaller caller;
  constexpr auto value1 = 1;
  constexpr auto value2 = 2;
  EXPECT_EQ(caller.BlockOnResponse(value1, ctx.client(), ctx.channel().id()),
            OkStatus());
  EXPECT_EQ(caller.BlockOnResponse(value2, ctx.client(), ctx.channel().id()),
            OkStatus());

  const auto request1 =
      ctx.request<test::pw_rpc::nanopb::TestService::TestUnaryRpc>(0);
  const auto request2 =
      ctx.request<test::pw_rpc::nanopb::TestService::TestUnaryRpc>(1);
  const auto response1 =
      ctx.response<test::pw_rpc::nanopb::TestService::TestUnaryRpc>(0);
  const auto response2 =
      ctx.response<test::pw_rpc::nanopb::TestService::TestUnaryRpc>(1);

  EXPECT_EQ(value1, request1.integer);
  EXPECT_EQ(value2, request2.integer);
  EXPECT_EQ(value1 + 1, response1.value);
  EXPECT_EQ(value2 + 1, response2.value);

  server_counter.second.lock();
  EXPECT_EQ(server_counter.first, 2);
  server_counter.second.unlock();
  client_counter.second.lock();
  EXPECT_EQ(client_counter.first, 2);
  client_counter.second.unlock();
}

TEST(NanopbClientServerTestContextThreaded, ResponseWithCallbacks) {
  NanopbClientServerTestContextThreaded<> ctx(
      thread::test::TestOptionsThread0());
  TestService service;
  ctx.server().RegisterService(service);

  RpcCaller caller;
  EXPECT_EQ(caller.BlockOnResponse<GeneratedService::TestAnotherUnaryRpc>(
                0, ctx.client(), ctx.channel().id()),
            OkStatus());

  // To decode a response object that requires to set pb_callback_t members,
  // pass it to the response() method as a parameter.
  constexpr size_t kMaxNumValues = 4;
  struct DecoderContext {
    uint32_t num_calls = 0;
    uint32_t values[kMaxNumValues];
    bool failed = false;
  } decoder_context;

  pw_rpc_test_TestResponse response = pw_rpc_test_TestResponse_init_default;
  response.repeated_field.funcs.decode = +[](pb_istream_t* stream,
                                             const pb_field_t* /* field */,
                                             void** arg) -> bool {
    DecoderContext* dec_ctx = static_cast<DecoderContext*>(*arg);
    uint64_t value;
    if (!pb_decode_varint(stream, &value)) {
      dec_ctx->failed = true;
      return false;
    }
    if (dec_ctx->num_calls < kMaxNumValues) {
      dec_ctx->values[dec_ctx->num_calls] = value;
    }
    dec_ctx->num_calls++;
    return true;
  };
  response.repeated_field.arg = &decoder_context;
  ctx.response<GeneratedService::TestAnotherUnaryRpc>(0, response);

  EXPECT_FALSE(decoder_context.failed);
  EXPECT_EQ(3u, decoder_context.num_calls);
  EXPECT_EQ(7u, decoder_context.values[0]);
  EXPECT_EQ(8u, decoder_context.values[1]);
  EXPECT_EQ(9u, decoder_context.values[2]);
}

}  // namespace
}  // namespace pw::rpc
