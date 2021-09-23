// Copyright 2021 The Pigweed Authors
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

#include "pw_rpc/nanopb/client_reader_writer.h"

#include <optional>

#include "gtest/gtest.h"
#include "pw_rpc/nanopb/client_testing.h"
#include "pw_rpc_test_protos/test.rpc.pb.h"

namespace pw::rpc {
namespace {

TEST(NanopbUnaryReceiver, DefaultConstructed) {
  NanopbUnaryReceiver<pw_rpc_test_TestResponse> call;

  ASSERT_FALSE(call.active());
  EXPECT_EQ(call.channel_id(), Channel::kUnassignedChannelId);

  EXPECT_EQ(Status::FailedPrecondition(), call.Cancel());

  call.set_on_completed([](const pw_rpc_test_TestResponse&, Status) {});
  call.set_on_error([](Status) {});
}

TEST(NanopbClientWriter, DefaultConstructed) {
  NanopbClientWriter<pw_rpc_test_TestRequest, pw_rpc_test_TestStreamResponse>
      call;

  ASSERT_FALSE(call.active());
  EXPECT_EQ(call.channel_id(), Channel::kUnassignedChannelId);

  EXPECT_EQ(Status::FailedPrecondition(),
            call.Write(pw_rpc_test_TestRequest{}));
  EXPECT_EQ(Status::FailedPrecondition(), call.Cancel());

  call.set_on_completed([](const pw_rpc_test_TestStreamResponse&, Status) {});
  call.set_on_error([](Status) {});
}

TEST(NanopbClientReader, DefaultConstructed) {
  NanopbClientReader<pw_rpc_test_TestStreamResponse> call;

  ASSERT_FALSE(call.active());
  EXPECT_EQ(call.channel_id(), Channel::kUnassignedChannelId);

  EXPECT_EQ(Status::FailedPrecondition(), call.Cancel());

  call.set_on_completed([](Status) {});
  call.set_on_next([](const pw_rpc_test_TestStreamResponse&) {});
  call.set_on_error([](Status) {});
}

TEST(NanopbClientReaderWriter, DefaultConstructed) {
  NanopbClientReaderWriter<pw_rpc_test_TestRequest,
                           pw_rpc_test_TestStreamResponse>
      call;

  ASSERT_FALSE(call.active());
  EXPECT_EQ(call.channel_id(), Channel::kUnassignedChannelId);

  EXPECT_EQ(Status::FailedPrecondition(),
            call.Write(pw_rpc_test_TestRequest{}));
  EXPECT_EQ(Status::FailedPrecondition(), call.Cancel());

  call.set_on_completed([](Status) {});
  call.set_on_next([](const pw_rpc_test_TestStreamResponse&) {});
  call.set_on_error([](Status) {});
}

using test::pw_rpc::nanopb::TestService;

TEST(NanopbUnaryReceiver, CallbacksMoveCorrectly) {
  NanopbClientTestContext ctx;

  struct {
    pw_rpc_test_TestResponse payload = {.value = 12345678};
    std::optional<Status> status;
  } reply;

  NanopbUnaryReceiver<pw_rpc_test_TestResponse> call_2;
  {
    NanopbUnaryReceiver call_1 = TestService::TestUnaryRpc(
        ctx.client(),
        ctx.channel().id(),
        {},
        [&reply](const pw_rpc_test_TestResponse& response, Status status) {
          reply.payload = response;
          reply.status = status;
        });

    call_2 = std::move(call_1);
  }

  ctx.server().SendResponse<TestService::TestUnaryRpc>({.value = 9000},
                                                       Status::NotFound());
  EXPECT_EQ(reply.payload.value, 9000);
  EXPECT_EQ(reply.status, Status::NotFound());
}

TEST(NanopbClientReaderWriter, CallbacksMoveCorrectly) {
  NanopbClientTestContext ctx;

  pw_rpc_test_TestStreamResponse payload = {.chunk = {}, .number = 13579};

  NanopbClientReaderWriter<pw_rpc_test_TestRequest,
                           pw_rpc_test_TestStreamResponse>
      call_2;
  {
    NanopbClientReaderWriter call_1 = TestService::TestBidirectionalStreamRpc(
        ctx.client(),
        ctx.channel().id(),
        [&payload](const pw_rpc_test_TestStreamResponse& response) {
          payload = response;
        });

    call_2 = std::move(call_1);
  }

  ctx.server().SendServerStream<TestService::TestBidirectionalStreamRpc>(
      {.chunk = {}, .number = 5050});
  EXPECT_EQ(payload.number, 5050u);
}

}  // namespace
}  // namespace pw::rpc
