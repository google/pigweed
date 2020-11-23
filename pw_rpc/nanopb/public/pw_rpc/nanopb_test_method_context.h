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
#pragma once

#include <tuple>
#include <utility>

#include "pw_assert/light.h"
#include "pw_containers/vector.h"
#include "pw_preprocessor/arguments.h"
#include "pw_rpc/channel.h"
#include "pw_rpc/internal/hash.h"
#include "pw_rpc/internal/method_lookup.h"
#include "pw_rpc/internal/nanopb_method.h"
#include "pw_rpc/internal/packet.h"
#include "pw_rpc/internal/server.h"

namespace pw::rpc {

// Declares a context object that may be used to invoke an RPC. The context is
// declared with the name of the implemented service and the method to invoke.
// The RPC can then be invoked with the call method.
//
// For a unary RPC, context.call(request) returns the status, and the response
// struct can be accessed via context.response().
//
//   PW_NANOPB_TEST_METHOD_CONTEXT(my::CoolService, TheMethod) context;
//   EXPECT_EQ(Status::Ok(), context.call({.some_arg = 123}));
//   EXPECT_EQ(500, context.response().some_response_value);
//
// For a server streaming RPC, context.call(request) invokes the method. As in a
// normal RPC, the method completes when the ServerWriter's Finish method is
// called (or it goes out of scope).
//
//   PW_NANOPB_TEST_METHOD_CONTEXT(my::CoolService, TheStreamingMethod) context;
//   context.call({.some_arg = 123});
//
//   EXPECT_TRUE(context.done());  // Check that the RPC completed
//   EXPECT_EQ(Status::Ok(), context.status());  // Check the status
//
//   EXPECT_EQ(3u, context.responses().size());
//   EXPECT_EQ(123, context.responses()[0].value); // check individual responses
//
//   for (const MyResponse& response : context.responses()) {
//     // iterate over the responses
//   }
//
// PW_NANOPB_TEST_METHOD_CONTEXT forwards its constructor arguments to the
// underlying serivce. For example:
//
//   PW_NANOPB_TEST_METHOD_CONTEXT(MyService, Go) context(service, args);
//
// PW_NANOPB_TEST_METHOD_CONTEXT takes two optional arguments:
//
//   size_t max_responses: maximum responses to store; ignored unless streaming
//   size_t output_size_bytes: buffer size; must be large enough for a packet
//
// Example:
//
//   PW_NANOPB_TEST_METHOD_CONTEXT(MyService, BestMethod, 3, 256) context;
//   ASSERT_EQ(3u, context.responses().max_size());
//
#define PW_NANOPB_TEST_METHOD_CONTEXT(service, method, ...)              \
  ::pw::rpc::NanopbTestMethodContext<service,                            \
                                     &service::method,                   \
                                     ::pw::rpc::internal::Hash(#method), \
                                     ##__VA_ARGS__>
template <typename Service,
          auto method,
          uint32_t method_id,
          size_t max_responses = 4,
          size_t output_size_bytes = 128>
class NanopbTestMethodContext;

// Internal classes that implement NanopbTestMethodContext.
namespace internal::test::nanopb {

// A ChannelOutput implementation that stores the outgoing payloads and status.
template <typename Response>
class MessageOutput final : public ChannelOutput {
 public:
  MessageOutput(const internal::NanopbMethod& method,
                Vector<Response>& responses,
                std::span<std::byte> buffer)
      : ChannelOutput("internal::test::nanopb::MessageOutput"),
        method_(method),
        responses_(responses),
        buffer_(buffer) {
    clear();
  }

  Status last_status() const { return last_status_; }
  void set_last_status(Status status) { last_status_ = status; }

  size_t total_responses() const { return total_responses_; }

  bool stream_ended() const { return stream_ended_; }

  void clear();

 private:
  std::span<std::byte> AcquireBuffer() override { return buffer_; }

  Status SendAndReleaseBuffer(std::span<const std::byte> buffer) override;

  const internal::NanopbMethod& method_;
  Vector<Response>& responses_;
  std::span<std::byte> buffer_;
  size_t total_responses_;
  bool stream_ended_;
  Status last_status_;
};

// Collects everything needed to invoke a particular RPC.
template <typename Service,
          auto method,
          uint32_t method_id,
          size_t max_responses,
          size_t output_size>
struct InvocationContext {
  using Request = internal::Request<method>;
  using Response = internal::Response<method>;

  template <typename... Args>
  InvocationContext(Args&&... args)
      : output(MethodLookup::GetNanopbMethod<Service, method_id>(),
               responses,
               buffer),
        channel(Channel::Create<123>(&output)),
        server(std::span(&channel, 1)),
        service(std::forward<Args>(args)...),
        call(static_cast<internal::Server&>(server),
             static_cast<internal::Channel&>(channel),
             service,
             MethodLookup::GetNanopbMethod<Service, method_id>()) {}

  MessageOutput<Response> output;

  rpc::Channel channel;
  rpc::Server server;
  Service service;
  Vector<Response, max_responses> responses;
  std::array<std::byte, output_size> buffer = {};

  internal::ServerCall call;
};

// Method invocation context for a unary RPC. Returns the status in call() and
// provides the response through the response() method.
template <typename Service, auto method, uint32_t method_id, size_t output_size>
class UnaryContext {
 private:
  InvocationContext<Service, method, method_id, 1, output_size> ctx_;

 public:
  using Request = typename decltype(ctx_)::Request;
  using Response = typename decltype(ctx_)::Response;

  template <typename... Args>
  UnaryContext(Args&&... args) : ctx_(std::forward<Args>(args)...) {}

  Service& service() { return ctx_.service; }

  // Invokes the RPC with the provided request. Returns the status.
  Status call(const Request& request) {
    ctx_.output.clear();
    ctx_.responses.emplace_back();
    ctx_.responses.back() = {};
    return CallMethodImplFunction<method>(
        ctx_.call, request, ctx_.responses.back());
  }

  // Gives access to the RPC's response.
  const Response& response() const {
    PW_ASSERT(ctx_.responses.size() > 0u);
    return ctx_.responses.back();
  }
};

// Method invocation context for a server streaming RPC.
template <typename Service,
          auto method,
          uint32_t method_id,
          size_t max_responses,
          size_t output_size>
class ServerStreamingContext {
 private:
  InvocationContext<Service, method, method_id, max_responses, output_size>
      ctx_;

 public:
  using Request = typename decltype(ctx_)::Request;
  using Response = typename decltype(ctx_)::Response;

  template <typename... Args>
  ServerStreamingContext(Args&&... args) : ctx_(std::forward<Args>(args)...) {}

  Service& service() { return ctx_.service; }

  // Invokes the RPC with the provided request.
  void call(const Request& request) {
    ctx_.output.clear();
    internal::BaseServerWriter server_writer(ctx_.call);
    return CallMethodImplFunction<method>(
        ctx_.call,
        request,
        static_cast<ServerWriter<Response>&>(server_writer));
  }

  // Returns a server writer which writes responses into the context's buffer.
  // This should not be called alongside call(); use one or the other.
  ServerWriter<Response> writer() {
    ctx_.output.clear();
    internal::BaseServerWriter server_writer(ctx_.call);
    return std::move(static_cast<ServerWriter<Response>&>(server_writer));
  }

  // Returns the responses that have been recorded. The maximum number of
  // responses is responses().max_size(). responses().back() is always the most
  // recent response, even if total_responses() > responses().max_size().
  const Vector<Response>& responses() const { return ctx_.responses; }

  // The total number of responses sent, which may be larger than
  // responses.max_size().
  size_t total_responses() const { return ctx_.output.total_responses(); }

  // True if the stream has terminated.
  bool done() const { return ctx_.output.stream_ended(); }

  // The status of the stream. Only valid if done() is true.
  Status status() const {
    PW_ASSERT(done());
    return ctx_.output.last_status();
  }
};

// Alias to select the type of the context object to use based on which type of
// RPC it is for.
template <typename Service,
          auto method,
          uint32_t method_id,
          size_t responses,
          size_t output_size>
using Context = std::tuple_element_t<
    static_cast<size_t>(internal::MethodTraits<decltype(method)>::kType),
    std::tuple<UnaryContext<Service, method, method_id, output_size>,
               ServerStreamingContext<Service,
                                      method,
                                      method_id,
                                      responses,
                                      output_size>
               // TODO(hepler): Support client and bidi streaming
               >>;

template <typename Response>
void MessageOutput<Response>::clear() {
  responses_.clear();
  total_responses_ = 0;
  stream_ended_ = false;
  last_status_ = Status::Unknown();
}

template <typename Response>
Status MessageOutput<Response>::SendAndReleaseBuffer(
    std::span<const std::byte> buffer) {
  PW_ASSERT(!stream_ended_);
  PW_ASSERT(buffer.data() == buffer_.data());

  if (buffer.empty()) {
    return Status::Ok();
  }

  Result<internal::Packet> result = internal::Packet::FromBuffer(buffer);
  PW_ASSERT(result.ok());

  last_status_ = result.value().status();

  switch (result.value().type()) {
    case internal::PacketType::RESPONSE:
      // If we run out of space, the back message is always the most recent.
      responses_.emplace_back();
      responses_.back() = {};
      PW_ASSERT(
          method_.DecodeResponse(result.value().payload(), &responses_.back()));
      total_responses_ += 1;
      break;
    case internal::PacketType::SERVER_STREAM_END:
      stream_ended_ = true;
      break;
    default:
      PW_CRASH("Unhandled PacketType");
  }
  return Status::Ok();
}

}  // namespace internal::test::nanopb

template <typename Service,
          auto method,
          uint32_t method_id,
          size_t max_responses,
          size_t output_size_bytes>
class NanopbTestMethodContext
    : public internal::test::nanopb::Context<Service,
                                             method,
                                             method_id,
                                             max_responses,
                                             output_size_bytes> {
 public:
  // Forwards constructor arguments to the service class.
  template <typename... ServiceArgs>
  NanopbTestMethodContext(ServiceArgs&&... service_args)
      : internal::test::nanopb::Context<Service,
                                        method,
                                        method_id,
                                        max_responses,
                                        output_size_bytes>(
            std::forward<ServiceArgs>(service_args)...) {}
};

}  // namespace pw::rpc
