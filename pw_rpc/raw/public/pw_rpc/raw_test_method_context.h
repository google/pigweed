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

#include "pw_assert/assert.h"
#include "pw_containers/vector.h"
#include "pw_rpc/channel.h"
#include "pw_rpc/internal/hash.h"
#include "pw_rpc/internal/method_lookup.h"
#include "pw_rpc/internal/packet.h"
#include "pw_rpc/internal/raw_method.h"
#include "pw_rpc/internal/server.h"
#include "pw_rpc/internal/test_method_context.h"
#include "pw_rpc_private/fake_channel_output.h"

namespace pw::rpc {

// Declares a context object that may be used to invoke an RPC. The context is
// declared with the name of the implemented service and the method to invoke.
// The RPC can then be invoked with the call method.
//
// For a unary RPC, context.call(request) returns the status, and the response
// struct can be accessed via context.response().
//
//   PW_RAW_TEST_METHOD_CONTEXT(my::CoolService, TheMethod) context;
//   EXPECT_EQ(OkStatus(), context.call(encoded_request).status());
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
//   EXPECT_EQ(OkStatus(), context.status());  // Check the status
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
//   size_t kMaxResponses: maximum responses to store; ignored unless streaming
//   size_t kOutputSizeBytes: buffer size; must be large enough for a packet
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
          auto kMethod,
          uint32_t kMethodId,
          size_t kMaxResponses = 4,
          size_t kOutputSizeBytes = 128>
class RawTestMethodContext;

// Internal classes that implement RawTestMethodContext.
namespace internal::test::raw {

// A ChannelOutput implementation that stores the outgoing payloads and status.
template <size_t kOutputSize, size_t kMaxResponses>
class MessageOutput final : public FakeChannelOutput {
 public:
  MessageOutput(bool server_streaming)
      : FakeChannelOutput(packet_buffer_, server_streaming) {}

  const Vector<ByteSpan>& responses() const { return responses_; }

  // Allocates a response buffer and returns a reference to the response span
  // for it.
  ByteSpan& AllocateResponse() {
    // If we run out of space, the back message is always the most recent.
    response_buffers_.emplace_back();
    response_buffers_.back() = {};

    responses_.emplace_back();
    responses_.back() = {response_buffers_.back().data(),
                         response_buffers_.back().size()};
    return responses_.back();
  }

 private:
  void AppendResponse(ConstByteSpan response) override {
    ByteSpan& response_span = AllocateResponse();
    PW_ASSERT(response.size() <= response_span.size());

    std::memcpy(response_span.data(), response.data(), response.size());
    response_span = response_span.first(response.size());
  }

  void ClearResponses() override {
    responses_.clear();
    response_buffers_.clear();
  }

  std::array<std::byte, kOutputSize> packet_buffer_;
  Vector<ByteSpan, kMaxResponses> responses_;
  Vector<std::array<std::byte, kOutputSize>, kMaxResponses> response_buffers_;
};

// Collects everything needed to invoke a particular RPC.
template <typename Service,
          auto kMethod,
          uint32_t kMethodId,
          size_t kMaxResponses,
          size_t kOutputSize>
class RawInvocationContext : public InvocationContext<Service, kMethodId> {
 public:
  // Returns the responses that have been recorded. The maximum number of
  // responses is responses().max_size(). responses().back() is always the most
  // recent response, even if total_responses() > responses().max_size().
  const Vector<ByteSpan>& responses() const { return output_.responses(); }

  // Gives access to the RPC's most recent response.
  ConstByteSpan response() const {
    PW_ASSERT(!responses().empty());
    return responses().back();
  }

 protected:
  template <typename... Args>
  RawInvocationContext(Args&&... args)
      : InvocationContext<Service, kMethodId>(
            MethodLookup::GetRawMethod<Service, kMethodId>(),
            output_,
            std::forward<Args>(args)...),
        output_(MethodTraits<decltype(kMethod)>::kServerStreaming) {}

  MessageOutput<kOutputSize, kMaxResponses>& output() { return output_; }

 private:
  MessageOutput<kOutputSize, kMaxResponses> output_;
};

// Method invocation context for a unary RPC. Returns the status in call() and
// provides the response through the response() method.
template <typename Service,
          auto kMethod,
          uint32_t kMethodId,
          size_t kOutputSize>
class UnaryContext
    : public RawInvocationContext<Service, kMethod, kMethodId, 1, kOutputSize> {
  using Base =
      RawInvocationContext<Service, kMethod, kMethodId, 1, kOutputSize>;

 public:
  template <typename... Args>
  UnaryContext(Args&&... args) : Base(std::forward<Args>(args)...) {}

  // Invokes the RPC with the provided request. Returns RPC's StatusWithSize.
  StatusWithSize call(ConstByteSpan request) {
    Base::output().clear();
    ByteSpan& response = Base::output().AllocateResponse();
    auto sws =
        CallMethodImplFunction<kMethod>(Base::server_call(), request, response);
    response = response.first(sws.size());
    return sws;
  }
};

// Method invocation context for a server streaming RPC.
template <typename Service,
          auto kMethod,
          uint32_t kMethodId,
          size_t kMaxResponses,
          size_t kOutputSize>
class ServerStreamingContext : public RawInvocationContext<Service,
                                                           kMethod,
                                                           kMethodId,
                                                           kMaxResponses,
                                                           kOutputSize> {
  using Base = RawInvocationContext<Service,
                                    kMethod,
                                    kMethodId,
                                    kMaxResponses,
                                    kOutputSize>;

 public:
  template <typename... Args>
  ServerStreamingContext(Args&&... args) : Base(std::forward<Args>(args)...) {}

  // Invokes the RPC with the provided request.
  void call(ConstByteSpan request) {
    Base::output().clear();
    RawServerWriter writer(Base::server_call());
    return CallMethodImplFunction<kMethod>(
        Base::server_call(), request, writer);
  }

  // Returns a server writer which writes responses into the context's buffer.
  // This should not be called alongside call(); use one or the other.
  RawServerWriter writer() {
    Base::output().clear();
    return RawServerWriter(Base::server_call());
  }
};

// Method invocation context for a client streaming RPC.
template <typename Service,
          auto kMethod,
          uint32_t kMethodId,
          size_t kMaxResponses,
          size_t kOutputSize>
class ClientStreamingContext : public RawInvocationContext<Service,
                                                           kMethod,
                                                           kMethodId,
                                                           kMaxResponses,
                                                           kOutputSize> {
  using Base = RawInvocationContext<Service,
                                    kMethod,
                                    kMethodId,
                                    kMaxResponses,
                                    kOutputSize>;

 public:
  template <typename... Args>
  ClientStreamingContext(Args&&... args) : Base(std::forward<Args>(args)...) {}

  // Invokes the RPC.
  void call() {
    Base::output().clear();
    RawServerReader reader_writer(Base::server_call());
    return CallMethodImplFunction<kMethod>(Base::server_call(), reader_writer);
  }

  // Returns a reader/writer which writes responses into the context's buffer.
  // This should not be called alongside call(); use one or the other.
  RawServerReader reader() {
    Base::output().clear();
    return RawServerReader(Base::server_call());
  }

  // Allow sending client streaming packets.
  using Base::SendClientStream;
  using Base::SendClientStreamEnd;
};

// Method invocation context for a bidirectional streaming RPC.
template <typename Service,
          auto kMethod,
          uint32_t kMethodId,
          size_t kMaxResponses,
          size_t kOutputSize>
class BidirectionalStreamingContext : public RawInvocationContext<Service,
                                                                  kMethod,
                                                                  kMethodId,
                                                                  kMaxResponses,
                                                                  kOutputSize> {
  using Base = RawInvocationContext<Service,
                                    kMethod,
                                    kMethodId,
                                    kMaxResponses,
                                    kOutputSize>;

 public:
  template <typename... Args>
  BidirectionalStreamingContext(Args&&... args)
      : Base(std::forward<Args>(args)...) {}

  // Invokes the RPC.
  void call() {
    Base::output().clear();
    RawServerReaderWriter reader_writer(Base::server_call());
    return CallMethodImplFunction<kMethod>(Base::server_call(), reader_writer);
  }

  // Returns a reader/writer which writes responses into the context's buffer.
  // This should not be called alongside call(); use one or the other.
  RawServerReaderWriter reader_writer() {
    Base::output().clear();
    return RawServerReaderWriter(Base::server_call());
  }

  // Allow sending client streaming packets.
  using Base::SendClientStream;
  using Base::SendClientStreamEnd;
};

// Alias to select the type of the context object to use based on which type of
// RPC it is for.
template <typename Service,
          auto kMethod,
          uint32_t kMethodId,
          size_t kMaxResponses,
          size_t kOutputSize>
using Context = std::tuple_element_t<
    static_cast<size_t>(MethodTraits<decltype(kMethod)>::kType),
    std::tuple<UnaryContext<Service, kMethod, kMethodId, kOutputSize>,
               ServerStreamingContext<Service,
                                      kMethod,
                                      kMethodId,
                                      kMaxResponses,
                                      kOutputSize>,
               ClientStreamingContext<Service,
                                      kMethod,
                                      kMethodId,
                                      kMaxResponses,
                                      kOutputSize>,
               BidirectionalStreamingContext<Service,
                                             kMethod,
                                             kMethodId,
                                             kMaxResponses,
                                             kOutputSize>>>;

}  // namespace internal::test::raw

template <typename Service,
          auto kMethod,
          uint32_t kMethodId,
          size_t kMaxResponses,
          size_t kOutputSizeBytes>
class RawTestMethodContext
    : public internal::test::raw::Context<Service,
                                          kMethod,
                                          kMethodId,
                                          kMaxResponses,
                                          kOutputSizeBytes> {
 public:
  // Forwards constructor arguments to the service class.
  template <typename... ServiceArgs>
  RawTestMethodContext(ServiceArgs&&... service_args)
      : internal::test::raw::Context<Service,
                                     kMethod,
                                     kMethodId,
                                     kMaxResponses,
                                     kOutputSizeBytes>(
            std::forward<ServiceArgs>(service_args)...) {}
};

}  // namespace pw::rpc
