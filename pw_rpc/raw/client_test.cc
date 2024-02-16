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

#include "pw_rpc/client.h"

#include <optional>

#include "pw_rpc/internal/client_call.h"
#include "pw_rpc/internal/packet.h"
#include "pw_rpc/raw/client_reader_writer.h"
#include "pw_rpc/raw/client_testing.h"
#include "pw_unit_test/framework.h"

namespace pw::rpc {

void UnaryMethod();
void BidirectionalStreamMethod();

template <>
struct internal::MethodInfo<UnaryMethod> {
  static constexpr uint32_t kServiceId = 100;
  static constexpr uint32_t kMethodId = 200;
  static constexpr MethodType kType = MethodType::kUnary;
};

template <>
struct internal::MethodInfo<BidirectionalStreamMethod> {
  static constexpr uint32_t kServiceId = 100;
  static constexpr uint32_t kMethodId = 300;
  static constexpr MethodType kType = MethodType::kBidirectionalStreaming;
};

namespace {

// Captures payload from on_next and statuses from on_error and on_completed.
// Payloads are assumed to be null-terminated strings.
template <typename CallType>
struct CallContext {
  auto OnNext() {
    return [this](ConstByteSpan string) {
      payload = reinterpret_cast<const char*>(string.data());
    };
  }

  auto UnaryOnCompleted() {
    return [this](ConstByteSpan string, Status status) {
      payload = reinterpret_cast<const char*>(string.data());
      completed = status;
    };
  }

  auto StreamOnCompleted() {
    return [this](Status status) { completed = status; };
  }

  auto OnError() {
    return [this](Status status) { error = status; };
  }

  CallType call;

  const char* payload;
  std::optional<Status> completed;
  std::optional<Status> error;
};

template <auto kMethod, typename Context>
CallContext<RawUnaryReceiver> StartUnaryCall(
    Context& context, std::optional<uint32_t> channel_id = std::nullopt)
    PW_LOCKS_EXCLUDED(internal::rpc_lock()) {
  CallContext<RawUnaryReceiver> call_context;
  call_context.call =
      internal::UnaryResponseClientCall::Start<RawUnaryReceiver>(
          context.client(),
          channel_id.value_or(context.channel().id()),
          internal::MethodInfo<kMethod>::kServiceId,
          internal::MethodInfo<kMethod>::kMethodId,
          call_context.UnaryOnCompleted(),
          call_context.OnError(),
          {});
  return call_context;
}

template <auto kMethod, typename Context>
CallContext<RawClientReaderWriter> StartStreamCall(
    Context& context, std::optional<uint32_t> channel_id = std::nullopt)
    PW_LOCKS_EXCLUDED(internal::rpc_lock()) {
  CallContext<RawClientReaderWriter> call_context;
  call_context.call =
      internal::StreamResponseClientCall::Start<RawClientReaderWriter>(
          context.client(),
          channel_id.value_or(context.channel().id()),
          internal::MethodInfo<kMethod>::kServiceId,
          internal::MethodInfo<kMethod>::kMethodId,
          call_context.OnNext(),
          call_context.StreamOnCompleted(),
          call_context.OnError(),
          {});
  return call_context;
}

TEST(Client, ProcessPacket_InvokesUnaryCallbacks) {
  RawClientTestContext context;
  CallContext call_context = StartUnaryCall<UnaryMethod>(context);

  ASSERT_NE(call_context.completed, OkStatus());

  context.server().SendResponse<UnaryMethod>(as_bytes(span("you nary?!?")),
                                             OkStatus());

  ASSERT_NE(call_context.payload, nullptr);
  EXPECT_STREQ(call_context.payload, "you nary?!?");
  EXPECT_EQ(call_context.completed, OkStatus());
  EXPECT_FALSE(call_context.call.active());
}

TEST(Client, ProcessPacket_NoCallbackSet) {
  RawClientTestContext context;
  CallContext call_context = StartUnaryCall<UnaryMethod>(context);
  call_context.call.set_on_completed(nullptr);

  ASSERT_NE(call_context.completed, OkStatus());

  context.server().SendResponse<UnaryMethod>(as_bytes(span("you nary?!?")),
                                             OkStatus());
  EXPECT_FALSE(call_context.call.active());
}

TEST(Client, ProcessPacket_InvokesStreamCallbacks) {
  RawClientTestContext context;
  auto call = StartStreamCall<BidirectionalStreamMethod>(context);

  context.server().SendServerStream<BidirectionalStreamMethod>(
      as_bytes(span("<=>")));

  ASSERT_NE(call.payload, nullptr);
  EXPECT_STREQ(call.payload, "<=>");

  context.server().SendResponse<BidirectionalStreamMethod>(Status::NotFound());

  EXPECT_EQ(call.completed, Status::NotFound());
}

TEST(Client, ProcessPacket_UnassignedChannelId_ReturnsDataLoss) {
  RawClientTestContext context;
  auto call_cts = StartStreamCall<BidirectionalStreamMethod>(context);

  std::byte encoded[64];
  uint32_t arbitrary_call_id = 24602;
  Result<span<const std::byte>> result =
      internal::Packet(
          internal::pwpb::PacketType::kResponse,
          Channel::kUnassignedChannelId,
          internal::MethodInfo<BidirectionalStreamMethod>::kServiceId,
          internal::MethodInfo<BidirectionalStreamMethod>::kMethodId,
          arbitrary_call_id)
          .Encode(encoded);
  ASSERT_TRUE(result.ok());

  EXPECT_EQ(context.client().ProcessPacket(*result), Status::DataLoss());
}

TEST(Client, ProcessPacket_InvokesErrorCallback) {
  RawClientTestContext context;
  auto call = StartStreamCall<BidirectionalStreamMethod>(context);

  context.server().SendServerError<BidirectionalStreamMethod>(
      Status::Aborted());

  EXPECT_EQ(call.error, Status::Aborted());
}

TEST(Client, ProcessPacket_SendsClientErrorOnUnregisteredServerStream) {
  RawClientTestContext context;
  context.server().SendServerStream<BidirectionalStreamMethod>({});

  StatusView errors = context.output().errors<BidirectionalStreamMethod>();
  ASSERT_EQ(errors.size(), 1u);
  EXPECT_EQ(errors.front(), Status::FailedPrecondition());
}

TEST(Client, ProcessPacket_NonServerStreamOnUnregisteredCall_SendsNothing) {
  RawClientTestContext context;
  context.server().SendServerError<UnaryMethod>(Status::NotFound());
  EXPECT_EQ(context.output().total_packets(), 0u);

  context.server().SendResponse<UnaryMethod>({}, Status::Unavailable());
  EXPECT_EQ(context.output().total_packets(), 0u);
}

TEST(Client, ProcessPacket_ReturnsDataLossOnBadPacket) {
  RawClientTestContext context;

  constexpr std::byte bad_packet[]{
      std::byte{0xab}, std::byte{0xcd}, std::byte{0xef}};
  EXPECT_EQ(context.client().ProcessPacket(bad_packet), Status::DataLoss());
}

TEST(Client, ProcessPacket_ReturnsInvalidArgumentOnServerPacket) {
  RawClientTestContext context;

  std::byte encoded[64];
  Result<span<const std::byte>> result =
      internal::Packet(internal::pwpb::PacketType::REQUEST, 1, 2, 3, 4)
          .Encode(encoded);
  ASSERT_TRUE(result.ok());

  EXPECT_EQ(context.client().ProcessPacket(*result), Status::InvalidArgument());
}

const Channel* GetChannel(internal::Endpoint& endpoint, uint32_t id) {
  internal::RpcLockGuard lock;
  return endpoint.GetInternalChannel(id);
}

TEST(Client, CloseChannel_NoCalls) {
  RawClientTestContext ctx;
  ASSERT_NE(nullptr, GetChannel(ctx.client(), ctx.kDefaultChannelId));
  EXPECT_EQ(OkStatus(), ctx.client().CloseChannel(ctx.kDefaultChannelId));
  EXPECT_EQ(nullptr, GetChannel(ctx.client(), ctx.kDefaultChannelId));
  EXPECT_EQ(ctx.output().total_packets(), 0u);
}

TEST(Client, CloseChannel_UnknownChannel) {
  RawClientTestContext ctx;
  ASSERT_EQ(nullptr, GetChannel(ctx.client(), 13579));
  EXPECT_EQ(Status::NotFound(), ctx.client().CloseChannel(13579));
}

TEST(Client, CloseChannel_CallsErrorCallback) {
  RawClientTestContext ctx;
  CallContext call_ctx = StartUnaryCall<UnaryMethod>(ctx);

  ASSERT_NE(call_ctx.completed, OkStatus());
  ASSERT_EQ(1u,
            static_cast<internal::Endpoint&>(ctx.client()).active_call_count());

  EXPECT_EQ(OkStatus(), ctx.client().CloseChannel(1));

  EXPECT_EQ(0u,
            static_cast<internal::Endpoint&>(ctx.client()).active_call_count());
  ASSERT_EQ(call_ctx.error, Status::Aborted());  // set by the on_error callback
}

TEST(Client, CloseChannel_ErrorCallbackReusesCallObjectForCallOnClosedChannel) {
  struct {
    RawClientTestContext<> ctx;
    CallContext<RawUnaryReceiver> call_ctx;
  } context;

  context.call_ctx = StartUnaryCall<UnaryMethod>(context.ctx);
  context.call_ctx.call.set_on_error([&context](Status error) {
    context.call_ctx = StartUnaryCall<UnaryMethod>(context.ctx, 1);
    context.call_ctx.error = error;
  });

  EXPECT_EQ(OkStatus(), context.ctx.client().CloseChannel(1));
  EXPECT_EQ(context.call_ctx.error, Status::Aborted());

  EXPECT_FALSE(context.call_ctx.call.active());
  EXPECT_EQ(0u,
            static_cast<internal::Endpoint&>(context.ctx.client())
                .active_call_count());
}

TEST(Client, CloseChannel_ErrorCallbackReusesCallObjectForActiveCall) {
  class ContextWithTwoChannels {
   public:
    ContextWithTwoChannels()
        : channels_{Channel::Create<1>(&channel_output_),
                    Channel::Create<2>(&channel_output_)},
          client_(channels_),
          packet_buffer{},
          fake_server_(channel_output_, client_, 1, packet_buffer) {}

    Channel& channel() { return channels_[0]; }
    Client& client() { return client_; }
    CallContext<RawUnaryReceiver>& call_ctx() { return call_context_; }
    RawUnaryReceiver& call() { return call_context_.call; }

    void StartCall(uint32_t channel_id) {
      call_context_ = StartUnaryCall<UnaryMethod>(*this, channel_id);
    }

   private:
    RawFakeChannelOutput<10, 256> channel_output_;
    Channel channels_[2];
    Client client_;
    std::byte packet_buffer[64];
    FakeServer fake_server_;

    CallContext<RawUnaryReceiver> call_context_;
  } context;

  context.StartCall(1);
  context.call().set_on_error([&context](Status error) {
    context.StartCall(2);
    context.call_ctx().error = error;
  });

  EXPECT_EQ(OkStatus(), context.client().CloseChannel(1));
  EXPECT_EQ(context.call_ctx().error, Status::Aborted());

  EXPECT_TRUE(context.call().active());
  EXPECT_EQ(
      1u,
      static_cast<internal::Endpoint&>(context.client()).active_call_count());
}

TEST(Client, OpenChannel_UnusedSlot) {
  RawClientTestContext ctx;
  ASSERT_EQ(OkStatus(), ctx.client().CloseChannel(1));
  ASSERT_EQ(nullptr, GetChannel(ctx.client(), 9));

  EXPECT_EQ(OkStatus(), ctx.client().OpenChannel(9, ctx.output()));

  EXPECT_NE(nullptr, GetChannel(ctx.client(), 9));
}

TEST(Client, OpenChannel_AlreadyExists) {
  RawClientTestContext ctx;
  ASSERT_NE(nullptr, GetChannel(ctx.client(), 1));
  EXPECT_EQ(Status::AlreadyExists(), ctx.client().OpenChannel(1, ctx.output()));
}

TEST(Client, OpenChannel_AdditionalSlot) {
  RawClientTestContext ctx;

  constexpr Status kExpected =
      PW_RPC_DYNAMIC_ALLOCATION == 0 ? Status::ResourceExhausted() : OkStatus();
  EXPECT_EQ(kExpected, ctx.client().OpenChannel(19823, ctx.output()));
}

}  // namespace
}  // namespace pw::rpc
