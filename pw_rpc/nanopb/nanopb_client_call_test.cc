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

#include "pw_rpc/nanopb_client_call.h"

#include "gtest/gtest.h"
#include "pw_rpc_nanopb_private/internal_test_utils.h"
#include "pw_rpc_private/internal_test_utils.h"
#include "pw_rpc_test_protos/test.pb.h"

namespace pw::rpc {
namespace {

constexpr uint32_t kServiceId = 16;
constexpr uint32_t kUnaryMethodId = 111;
constexpr uint32_t kServerStreamingMethodId = 112;

class FakeGeneratedServiceClient {
 public:
  static NanopbClientCall<UnaryResponseHandler<pw_rpc_test_TestResponse>>
  TestRpc(Channel& channel,
          const pw_rpc_test_TestRequest& request,
          UnaryResponseHandler<pw_rpc_test_TestResponse>& callback) {
    auto call = NanopbClientCall(&channel,
                                 kServiceId,
                                 kUnaryMethodId,
                                 callback,
                                 pw_rpc_test_TestRequest_fields,
                                 pw_rpc_test_TestResponse_fields);
    call.SendRequest(&request);
    return call;
  }

  static NanopbClientCall<
      ServerStreamingResponseHandler<pw_rpc_test_TestStreamResponse>>
  TestStreamRpc(Channel& channel,
                const pw_rpc_test_TestRequest& request,
                ServerStreamingResponseHandler<pw_rpc_test_TestStreamResponse>&
                    callback) {
    auto call = NanopbClientCall(&channel,
                                 kServiceId,
                                 kServerStreamingMethodId,
                                 callback,
                                 pw_rpc_test_TestRequest_fields,
                                 pw_rpc_test_TestStreamResponse_fields);
    call.SendRequest(&request);
    return call;
  }
};

using internal::TestServerStreamingResponseHandler;
using internal::TestUnaryResponseHandler;

TEST(NanopbClientCall, Unary_SendsRequestPacket) {
  ClientContextForTest context;
  TestUnaryResponseHandler<pw_rpc_test_TestResponse> handler;

  auto call = FakeGeneratedServiceClient::TestRpc(
      context.channel(), {.integer = 123, .status_code = 0}, handler);

  EXPECT_EQ(context.output().packet_count(), 1u);
  auto packet = context.output().sent_packet();
  EXPECT_EQ(packet.channel_id(), context.channel().id());
  EXPECT_EQ(packet.service_id(), kServiceId);
  EXPECT_EQ(packet.method_id(), kUnaryMethodId);

  PW_DECODE_PB(pw_rpc_test_TestRequest, sent_proto, packet.payload());
  EXPECT_EQ(sent_proto.integer, 123);
}

TEST(NanopbClientCall, Unary_InvokesCallbackOnValidResponse) {
  ClientContextForTest context;
  TestUnaryResponseHandler<pw_rpc_test_TestResponse> handler;

  auto call = FakeGeneratedServiceClient::TestRpc(
      context.channel(), {.integer = 123, .status_code = 0}, handler);

  PW_ENCODE_PB(pw_rpc_test_TestResponse, response, .value = 42);
  context.SendResponse(Status::Ok(), response);

  ASSERT_EQ(handler.responses_received(), 1u);
  EXPECT_EQ(handler.last_status(), Status::Ok());
  EXPECT_EQ(handler.last_response().value, 42);
}

TEST(NanopbClientCall, Unary_InvokesErrorCallbackOnInvalidResponse) {
  ClientContextForTest context;
  TestUnaryResponseHandler<pw_rpc_test_TestResponse> handler;

  auto call = FakeGeneratedServiceClient::TestRpc(
      context.channel(), {.integer = 123, .status_code = 0}, handler);

  constexpr std::byte bad_payload[]{
      std::byte{0xab}, std::byte{0xcd}, std::byte{0xef}};
  context.SendResponse(Status::Ok(), bad_payload);

  EXPECT_EQ(handler.responses_received(), 0u);
  EXPECT_EQ(handler.rpc_error(), Status::DataLoss());
}

TEST(NanopbClientCall, Unary_InvokesErrorCallbackOnServerError) {
  ClientContextForTest context;
  TestUnaryResponseHandler<pw_rpc_test_TestResponse> handler;

  auto call = FakeGeneratedServiceClient::TestRpc(
      context.channel(), {.integer = 123, .status_code = 0}, handler);

  context.SendPacket(internal::PacketType::SERVER_ERROR, Status::NotFound());

  EXPECT_EQ(handler.responses_received(), 0u);
  EXPECT_EQ(handler.rpc_error(), Status::NotFound());
}

TEST(NanopbClientCall, Unary_OnlyReceivesOneResponse) {
  ClientContextForTest context;
  TestUnaryResponseHandler<pw_rpc_test_TestResponse> handler;

  auto call = FakeGeneratedServiceClient::TestRpc(
      context.channel(), {.integer = 123, .status_code = 0}, handler);

  PW_ENCODE_PB(pw_rpc_test_TestResponse, r1, .value = 42);
  context.SendResponse(Status::Unimplemented(), r1);
  PW_ENCODE_PB(pw_rpc_test_TestResponse, r2, .value = 44);
  context.SendResponse(Status::OutOfRange(), r2);
  PW_ENCODE_PB(pw_rpc_test_TestResponse, r3, .value = 46);
  context.SendResponse(Status::Internal(), r3);

  EXPECT_EQ(handler.responses_received(), 1u);
  EXPECT_EQ(handler.last_status(), Status::Unimplemented());
  EXPECT_EQ(handler.last_response().value, 42);
}

TEST(NanopbClientCall, ServerStreaming_SendsRequestPacket) {
  ClientContextForTest<128, 128, 99, kServiceId, kServerStreamingMethodId>
      context;
  TestServerStreamingResponseHandler<pw_rpc_test_TestStreamResponse> handler;

  auto call = FakeGeneratedServiceClient::TestStreamRpc(
      context.channel(), {.integer = 71, .status_code = 0}, handler);

  EXPECT_EQ(context.output().packet_count(), 1u);
  auto packet = context.output().sent_packet();
  EXPECT_EQ(packet.channel_id(), context.channel().id());
  EXPECT_EQ(packet.service_id(), kServiceId);
  EXPECT_EQ(packet.method_id(), kServerStreamingMethodId);

  PW_DECODE_PB(pw_rpc_test_TestRequest, sent_proto, packet.payload());
  EXPECT_EQ(sent_proto.integer, 71);
}

TEST(NanopbClientCall, ServerStreaming_InvokesCallbackOnValidResponse) {
  ClientContextForTest<128, 128, 99, kServiceId, kServerStreamingMethodId>
      context;
  TestServerStreamingResponseHandler<pw_rpc_test_TestStreamResponse> handler;

  auto call = FakeGeneratedServiceClient::TestStreamRpc(
      context.channel(), {.integer = 71, .status_code = 0}, handler);

  PW_ENCODE_PB(pw_rpc_test_TestStreamResponse, r1, .chunk = {}, .number = 11u);
  context.SendResponse(Status::Ok(), r1);
  EXPECT_TRUE(handler.active());
  EXPECT_EQ(handler.responses_received(), 1u);
  EXPECT_EQ(handler.last_response().number, 11u);

  PW_ENCODE_PB(pw_rpc_test_TestStreamResponse, r2, .chunk = {}, .number = 22u);
  context.SendResponse(Status::Ok(), r2);
  EXPECT_TRUE(handler.active());
  EXPECT_EQ(handler.responses_received(), 2u);
  EXPECT_EQ(handler.last_response().number, 22u);

  PW_ENCODE_PB(pw_rpc_test_TestStreamResponse, r3, .chunk = {}, .number = 33u);
  context.SendResponse(Status::Ok(), r3);
  EXPECT_TRUE(handler.active());
  EXPECT_EQ(handler.responses_received(), 3u);
  EXPECT_EQ(handler.last_response().number, 33u);
}

TEST(NanopbClientCall, ServerStreaming_ClosesOnFinish) {
  ClientContextForTest<128, 128, 99, kServiceId, kServerStreamingMethodId>
      context;
  TestServerStreamingResponseHandler<pw_rpc_test_TestStreamResponse> handler;

  auto call = FakeGeneratedServiceClient::TestStreamRpc(
      context.channel(), {.integer = 71, .status_code = 0}, handler);

  PW_ENCODE_PB(pw_rpc_test_TestStreamResponse, r1, .chunk = {}, .number = 11u);
  context.SendResponse(Status::Ok(), r1);
  EXPECT_TRUE(handler.active());

  PW_ENCODE_PB(pw_rpc_test_TestStreamResponse, r2, .chunk = {}, .number = 22u);
  context.SendResponse(Status::Ok(), r2);
  EXPECT_TRUE(handler.active());

  // Close the stream.
  context.SendPacket(internal::PacketType::SERVER_STREAM_END,
                     Status::NotFound());

  PW_ENCODE_PB(pw_rpc_test_TestStreamResponse, r3, .chunk = {}, .number = 33u);
  context.SendResponse(Status::Ok(), r3);
  EXPECT_FALSE(handler.active());

  EXPECT_EQ(handler.responses_received(), 2u);
}

TEST(NanopbClientCall, ServerStreaming_InvokesErrorCallbackOnInvalidResponses) {
  ClientContextForTest<128, 128, 99, kServiceId, kServerStreamingMethodId>
      context;
  TestServerStreamingResponseHandler<pw_rpc_test_TestStreamResponse> handler;

  auto call = FakeGeneratedServiceClient::TestStreamRpc(
      context.channel(), {.integer = 71, .status_code = 0}, handler);

  PW_ENCODE_PB(pw_rpc_test_TestStreamResponse, r1, .chunk = {}, .number = 11u);
  context.SendResponse(Status::Ok(), r1);
  EXPECT_TRUE(handler.active());
  EXPECT_EQ(handler.responses_received(), 1u);
  EXPECT_EQ(handler.last_response().number, 11u);

  constexpr std::byte bad_payload[]{
      std::byte{0xab}, std::byte{0xcd}, std::byte{0xef}};
  context.SendResponse(Status::Ok(), bad_payload);
  EXPECT_EQ(handler.responses_received(), 1u);
  EXPECT_EQ(handler.rpc_error(), Status::DataLoss());

  PW_ENCODE_PB(pw_rpc_test_TestStreamResponse, r2, .chunk = {}, .number = 22u);
  context.SendResponse(Status::Ok(), r2);
  EXPECT_TRUE(handler.active());
  EXPECT_EQ(handler.responses_received(), 2u);
  EXPECT_EQ(handler.last_response().number, 22u);

  context.SendPacket(internal::PacketType::SERVER_ERROR, Status::NotFound());
  EXPECT_EQ(handler.responses_received(), 2u);
  EXPECT_EQ(handler.rpc_error(), Status::NotFound());
}

}  // namespace
}  // namespace pw::rpc
