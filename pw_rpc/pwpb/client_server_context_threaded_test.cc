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
#include <iostream>

#include "gtest/gtest.h"
#include "pw_function/function.h"
#include "pw_rpc/pwpb/client_server_testing_threaded.h"
#include "pw_rpc_test_protos/test.rpc.pwpb.h"
#include "pw_sync/binary_semaphore.h"
#include "pw_thread/non_portable_test_thread_options.h"

namespace pw::rpc {
namespace {

namespace TestRequest = ::pw::rpc::test::pwpb::TestRequest;
namespace TestResponse = ::pw::rpc::test::pwpb::TestResponse;
namespace TestStreamResponse = ::pw::rpc::test::pwpb::TestStreamResponse;

}  // namespace

namespace test {

using GeneratedService = ::pw::rpc::test::pw_rpc::pwpb::TestService;

class TestService final : public GeneratedService::Service<TestService> {
 public:
  Status TestUnaryRpc(const TestRequest::Message& request,
                      TestResponse::Message& response) {
    response.value = request.integer + 1;
    return static_cast<Status::Code>(request.status_code);
  }

  Status TestAnotherUnaryRpc(const TestRequest::Message& request,
                             TestResponse::Message& response) {
    response.value = 42;
    response.repeated_field.SetEncoder(
        [](TestResponse::StreamEncoder& encoder) {
          constexpr std::array<uint32_t, 3> kValues = {7, 8, 9};
          return encoder.WriteRepeatedField(kValues);
        });
    return static_cast<Status::Code>(request.status_code);
  }

  static void TestServerStreamRpc(const TestRequest::Message&,
                                  ServerWriter<TestStreamResponse::Message>&) {}

  void TestClientStreamRpc(
      ServerReader<TestRequest::Message, TestStreamResponse::Message>&) {}

  void TestBidirectionalStreamRpc(
      ServerReaderWriter<TestRequest::Message, TestStreamResponse::Message>&) {}
};

}  // namespace test

namespace {

class RpcCaller {
 public:
  template <auto kMethod = test::GeneratedService::TestUnaryRpc>
  Status BlockOnResponse(uint32_t i, Client& client, uint32_t channel_id) {
    TestRequest::Message request{.integer = i,
                                 .status_code = OkStatus().code()};
    response_status_ = OkStatus();
    auto call = kMethod(
        client,
        channel_id,
        request,
        [this](const TestResponse::Message&, Status status) {
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
  Status response_status_ = OkStatus();
  pw::sync::BinarySemaphore semaphore_;
};

TEST(PwpbClientServerTestContextThreaded, ReceivesUnaryRpcResponseThreaded) {
  // TODO(b/290860904): Replace TestOptionsThread0 with TestThreadContext.
  PwpbClientServerTestContextThreaded<> ctx(thread::test::TestOptionsThread0());
  test::TestService service;
  ctx.server().RegisterService(service);

  RpcCaller caller;
  constexpr auto value = 1;
  EXPECT_EQ(caller.BlockOnResponse(value, ctx.client(), ctx.channel().id()),
            OkStatus());

  const auto request =
      ctx.request<test::pw_rpc::pwpb::TestService::TestUnaryRpc>(0);
  const auto response =
      ctx.response<test::pw_rpc::pwpb::TestService::TestUnaryRpc>(0);

  EXPECT_EQ(value, request.integer);
  EXPECT_EQ(value + 1, response.value);
}

TEST(PwpbClientServerTestContextThreaded, ReceivesMultipleResponsesThreaded) {
  PwpbClientServerTestContextThreaded<> ctx(thread::test::TestOptionsThread0());
  test::TestService service;
  ctx.server().RegisterService(service);

  RpcCaller caller;
  constexpr auto value1 = 1;
  constexpr auto value2 = 2;
  EXPECT_EQ(caller.BlockOnResponse(value1, ctx.client(), ctx.channel().id()),
            OkStatus());
  EXPECT_EQ(caller.BlockOnResponse(value2, ctx.client(), ctx.channel().id()),
            OkStatus());

  const auto request1 =
      ctx.request<test::pw_rpc::pwpb::TestService::TestUnaryRpc>(0);
  const auto request2 =
      ctx.request<test::pw_rpc::pwpb::TestService::TestUnaryRpc>(1);
  const auto response1 =
      ctx.response<test::pw_rpc::pwpb::TestService::TestUnaryRpc>(0);
  const auto response2 =
      ctx.response<test::pw_rpc::pwpb::TestService::TestUnaryRpc>(1);

  EXPECT_EQ(value1, request1.integer);
  EXPECT_EQ(value2, request2.integer);
  EXPECT_EQ(value1 + 1, response1.value);
  EXPECT_EQ(value2 + 1, response2.value);
}

TEST(PwpbClientServerTestContextThreaded,
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

  PwpbClientServerTestContextThreaded<> ctx(
      thread::test::TestOptionsThread0(), server_processor, client_processor);
  test::TestService service;
  ctx.server().RegisterService(service);

  RpcCaller caller;
  constexpr auto value1 = 1;
  constexpr auto value2 = 2;
  EXPECT_EQ(caller.BlockOnResponse(value1, ctx.client(), ctx.channel().id()),
            OkStatus());
  EXPECT_EQ(caller.BlockOnResponse(value2, ctx.client(), ctx.channel().id()),
            OkStatus());

  const auto request1 =
      ctx.request<test::pw_rpc::pwpb::TestService::TestUnaryRpc>(0);
  const auto request2 =
      ctx.request<test::pw_rpc::pwpb::TestService::TestUnaryRpc>(1);
  const auto response1 =
      ctx.response<test::pw_rpc::pwpb::TestService::TestUnaryRpc>(0);
  const auto response2 =
      ctx.response<test::pw_rpc::pwpb::TestService::TestUnaryRpc>(1);

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

TEST(PwpbClientServerTestContextThreaded, ResponseWithCallbacks) {
  PwpbClientServerTestContextThreaded<> ctx(thread::test::TestOptionsThread0());
  test::TestService service;
  ctx.server().RegisterService(service);

  RpcCaller caller;
  // DataLoss expected on initial response, since pwpb provides no way to
  // populate response callback. We setup callbacks on response packet below.
  EXPECT_EQ(caller.BlockOnResponse<test::GeneratedService::TestAnotherUnaryRpc>(
                0, ctx.client(), ctx.channel().id()),
            Status::DataLoss());

  // To decode a response object that requires to set callbacks, pass it to the
  // response() method as a parameter.
  pw::Vector<uint32_t, 4> values{};

  TestResponse::Message response{};
  response.repeated_field.SetDecoder(
      [&values](TestResponse::StreamDecoder& decoder) {
        return decoder.ReadRepeatedField(values);
      });
  ctx.response<test::GeneratedService::TestAnotherUnaryRpc>(0, response);

  EXPECT_EQ(42, response.value);

  EXPECT_EQ(3u, values.size());
  EXPECT_EQ(7u, values[0]);
  EXPECT_EQ(8u, values[1]);
  EXPECT_EQ(9u, values[2]);
}

}  // namespace
}  // namespace pw::rpc
