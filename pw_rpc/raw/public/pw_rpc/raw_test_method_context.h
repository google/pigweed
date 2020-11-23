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

#include <type_traits>

#include "pw_assert/light.h"
#include "pw_bytes/span.h"
#include "pw_containers/vector.h"
#include "pw_rpc/channel.h"
#include "pw_rpc/internal/hash.h"
#include "pw_rpc/internal/method_lookup.h"
#include "pw_rpc/internal/packet.h"
#include "pw_rpc/internal/raw_method.h"
#include "pw_rpc/internal/server.h"

namespace pw::rpc {

// Declares a context object that may be used to invoke an RPC. The context is
// declared with the name of the implemented service and the method to invoke.
// The RPC can then be invoked with the call method.
//
// For a unary RPC, context.call(request) returns the status, and the response
// struct can be accessed via context.response().
//
//   PW_RAW_TEST_METHOD_CONTEXT(my::CoolService, TheMethod) context;
//   EXPECT_EQ(Status::Ok(), context.call(encoded_request).status());
//   EXPECT_EQ(0,
//             std::memcmp(encoded_response,
//                         context.response().data(),
//                         sizeof(encoded_response)));
//
// For a server streaming RPC, context.call(request) invokes the method. As in a
// normal RPC, the method completes when the ServerWriter's Finish method is
// called (or it goes out of scope).
//
//   PW_RAW_TEST_METHOD_CONTEXT(my::CoolService, TheStreamingMethod) context;
//   context.call(encoded_response);
//
//   EXPECT_TRUE(context.done());  // Check that the RPC completed
//   EXPECT_EQ(Status::Ok(), context.status());  // Check the status
//
//   EXPECT_EQ(3u, context.responses().size());
//   ByteSpan& response = context.responses()[0];  // check individual responses
//
//   for (ByteSpan& response : context.responses()) {
//     // iterate over the responses
//   }
//
// PW_RAW_TEST_METHOD_CONTEXT forwards its constructor arguments to the
// underlying service. For example:
//
//   PW_RAW_TEST_METHOD_CONTEXT(MyService, Go) context(service, args);
//
// PW_RAW_TEST_METHOD_CONTEXT takes two optional arguments:
//
//   size_t max_responses: maximum responses to store; ignored unless streaming
//   size_t output_size_bytes: buffer size; must be large enough for a packet
//
// Example:
//
//   PW_RAW_TEST_METHOD_CONTEXT(MyService, BestMethod, 3, 256) context;
//   ASSERT_EQ(3u, context.responses().max_size());
//
#define PW_RAW_TEST_METHOD_CONTEXT(service, method, ...)              \
  ::pw::rpc::RawTestMethodContext<service,                            \
                                  &service::method,                   \
                                  ::pw::rpc::internal::Hash(#method), \
                                  ##__VA_ARGS__>
template <typename Service,
          auto method,
          uint32_t method_id,
          size_t max_responses = 4,
          size_t output_size_bytes = 128>
class RawTestMethodContext;

// Internal classes that implement RawTestMethodContext.
namespace internal::test::raw {

// A ChannelOutput implementation that stores the outgoing payloads and status.
template <size_t output_size>
class MessageOutput final : public ChannelOutput {
 public:
  using ResponseBuffer = std::array<std::byte, output_size>;

  MessageOutput(Vector<ByteSpan>& responses,
                Vector<ResponseBuffer>& buffers,
                ByteSpan packet_buffer)
      : ChannelOutput("internal::test::raw::MessageOutput"),
        responses_(responses),
        buffers_(buffers),
        packet_buffer_(packet_buffer) {
    clear();
  }

  Status last_status() const { return last_status_; }
  void set_last_status(Status status) { last_status_ = status; }

  size_t total_responses() const { return total_responses_; }

  bool stream_ended() const { return stream_ended_; }

  void clear() {
    responses_.clear();
    buffers_.clear();
    total_responses_ = 0;
    stream_ended_ = false;
    last_status_ = Status::Unknown();
  }

 private:
  ByteSpan AcquireBuffer() override { return packet_buffer_; }

  Status SendAndReleaseBuffer(std::span<const std::byte> buffer) override;

  Vector<ByteSpan>& responses_;
  Vector<ResponseBuffer>& buffers_;
  ByteSpan packet_buffer_;
  size_t total_responses_;
  bool stream_ended_;
  Status last_status_;
};

// Collects everything needed to invoke a particular RPC.
template <typename Service,
          uint32_t method_id,
          size_t max_responses,
          size_t output_size>
struct InvocationContext {
  template <typename... Args>
  InvocationContext(Args&&... args)
      : output(responses, buffers, packet_buffer),
        channel(Channel::Create<123>(&output)),
        server(std::span(&channel, 1)),
        service(std::forward<Args>(args)...),
        call(static_cast<internal::Server&>(server),
             static_cast<internal::Channel&>(channel),
             service,
             MethodLookup::GetRawMethod<Service, method_id>()) {}

  using ResponseBuffer = std::array<std::byte, output_size>;

  MessageOutput<output_size> output;
  rpc::Channel channel;
  rpc::Server server;
  Service service;
  Vector<ByteSpan, max_responses> responses;
  Vector<ResponseBuffer, max_responses> buffers;
  std::array<std::byte, output_size> packet_buffer = {};
  internal::ServerCall call;
};

// Method invocation context for a unary RPC. Returns the status in call() and
// provides the response through the response() method.
template <typename Service, auto method, uint32_t method_id, size_t output_size>
class UnaryContext {
 private:
  using Context = InvocationContext<Service, method_id, 1, output_size>;
  Context ctx_;

 public:
  template <typename... Args>
  UnaryContext(Args&&... args) : ctx_(std::forward<Args>(args)...) {}

  Service& service() { return ctx_.service; }

  // Invokes the RPC with the provided request. Returns RPC's StatusWithSize.
  StatusWithSize call(ConstByteSpan request) {
    ctx_.output.clear();
    ctx_.buffers.emplace_back();
    ctx_.buffers.back() = {};
    ctx_.responses.emplace_back();
    auto& response = ctx_.responses.back();
    response = {ctx_.buffers.back().data(), ctx_.buffers.back().size()};
    auto sws = CallMethodImplFunction<method>(ctx_.call, request, response);
    response = response.first(sws.size());
    return sws;
  }

  // Gives access to the RPC's response.
  ConstByteSpan response() const {
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
  using Context =
      InvocationContext<Service, method_id, max_responses, output_size>;
  Context ctx_;

 public:
  template <typename... Args>
  ServerStreamingContext(Args&&... args) : ctx_(std::forward<Args>(args)...) {}

  Service& service() { return ctx_.service; }

  // Invokes the RPC with the provided request.
  void call(ConstByteSpan request) {
    ctx_.output.clear();
    BaseServerWriter server_writer(ctx_.call);
    return CallMethodImplFunction<method>(
        ctx_.call, request, static_cast<RawServerWriter&>(server_writer));
  }

  // Returns a server writer which writes responses into the context's buffer.
  // This should not be called alongside call(); use one or the other.
  RawServerWriter writer() {
    ctx_.output.clear();
    BaseServerWriter server_writer(ctx_.call);
    return std::move(static_cast<RawServerWriter&>(server_writer));
  }

  // Returns the responses that have been recorded. The maximum number of
  // responses is responses().max_size(). responses().back() is always the most
  // recent response, even if total_responses() > responses().max_size().
  const Vector<ByteSpan>& responses() const { return ctx_.responses; }

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
    static_cast<size_t>(MethodTraits<decltype(method)>::kType),
    std::tuple<UnaryContext<Service, method, method_id, output_size>,
               ServerStreamingContext<Service,
                                      method,
                                      method_id,
                                      responses,
                                      output_size>
               // TODO(hepler): Support client and bidi streaming
               >>;

template <size_t output_size>
Status MessageOutput<output_size>::SendAndReleaseBuffer(
    std::span<const std::byte> buffer) {
  PW_ASSERT(!stream_ended_);
  PW_ASSERT(buffer.data() == packet_buffer_.data());

  if (buffer.empty()) {
    return Status::Ok();
  }

  Result<internal::Packet> result = internal::Packet::FromBuffer(buffer);
  PW_ASSERT(result.ok());

  last_status_ = result.value().status();

  switch (result.value().type()) {
    case internal::PacketType::RESPONSE: {
      // If we run out of space, the back message is always the most recent.
      buffers_.emplace_back();
      buffers_.back() = {};
      auto response = result.value().payload();
      std::memcpy(&buffers_.back(), response.data(), response.size());
      responses_.emplace_back();
      responses_.back() = {buffers_.back().data(), response.size()};
      total_responses_ += 1;
      break;
    }
    case internal::PacketType::SERVER_STREAM_END:
      stream_ended_ = true;
      break;
    default:
      PW_CRASH("Unhandled PacketType");
  }
  return Status::Ok();
}

}  // namespace internal::test::raw

template <typename Service,
          auto method,
          uint32_t method_id,
          size_t max_responses,
          size_t output_size_bytes>
class RawTestMethodContext
    : public internal::test::raw::Context<Service,
                                          method,
                                          method_id,
                                          max_responses,
                                          output_size_bytes> {
 public:
  // Forwards constructor arguments to the service class.
  template <typename... ServiceArgs>
  RawTestMethodContext(ServiceArgs&&... service_args)
      : internal::test::raw::Context<Service,
                                     method,
                                     method_id,
                                     max_responses,
                                     output_size_bytes>(
            std::forward<ServiceArgs>(service_args)...) {}
};

}  // namespace pw::rpc
