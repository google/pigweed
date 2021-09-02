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

#include "pw_assert/assert.h"
#include "pw_bytes/span.h"
#include "pw_containers/vector.h"
#include "pw_preprocessor/arguments.h"
#include "pw_rpc/internal/hash.h"
#include "pw_rpc/internal/method_lookup.h"
#include "pw_rpc/internal/test_method_context.h"
#include "pw_rpc/nanopb/fake_channel_output.h"
#include "pw_rpc/nanopb/internal/method.h"

namespace pw::rpc {

// Declares a context object that may be used to invoke an RPC. The context is
// declared with the name of the implemented service and the method to invoke.
// The RPC can then be invoked with the call method.
//
// For a unary RPC, context.call(request) returns the status, and the response
// struct can be accessed via context.response().
//
//   PW_NANOPB_TEST_METHOD_CONTEXT(my::CoolService, TheMethod) context;
//   EXPECT_EQ(OkStatus(), context.call({.some_arg = 123}));
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
//   EXPECT_EQ(OkStatus(), context.status());  // Check the status
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
//   size_t kMaxResponses: maximum responses to store; ignored unless streaming
//   size_t kOutputSizeBytes: buffer size; must be large enough for a packet
//
// Example:
//
//   PW_NANOPB_TEST_METHOD_CONTEXT(MyService, BestMethod, 3, 256) context;
//   ASSERT_EQ(3u, context.responses().max_size());
//
#define PW_NANOPB_TEST_METHOD_CONTEXT(service, method, ...)                \
  ::pw::rpc::NanopbTestMethodContext<service,                              \
                                     &service::method,                     \
                                     ::pw::rpc::CalculateMethodId(#method) \
                                         PW_COMMA_ARGS(__VA_ARGS__)>
template <typename Service,
          auto kMethod,
          uint32_t kMethodId,
          size_t kMaxResponses = 4,
          size_t kOutputSizeBytes = 128>
class NanopbTestMethodContext;

// Internal classes that implement NanopbTestMethodContext.
namespace internal::test::nanopb {

// Collects everything needed to invoke a particular RPC.
template <typename Service,
          auto kMethod,
          uint32_t kMethodId,
          size_t kMaxResponses,
          size_t kOutputSize>
class NanopbInvocationContext
    : public InvocationContext<
          NanopbFakeChannelOutput<internal::Response<kMethod>,
                                  kMaxResponses,
                                  kOutputSize>,
          Service,
          kMethodId> {
 public:
  using Request = internal::Request<kMethod>;
  using Response = internal::Response<kMethod>;

  // Gives access to the RPC's most recent response.
  const Response& response() const { return Base::output().last_response(); }

 protected:
  template <typename... Args>
  NanopbInvocationContext(Args&&... args)
      : Base(kMethodInfo,
             std::forward_as_tuple(MethodTraits<decltype(kMethod)>::kType,
                                   kMethodInfo),
             std::forward<Args>(args)...) {}

  void SendClientStream(const Request& request) {
    // Borrow a buffer from the ChannelOutput for sending the request.
    ChannelOutput& channel_output = Base::output();
    std::span buffer = channel_output.AcquireBuffer();

    Base::SendClientStream(buffer.first(
        kMethodInfo.serde().EncodeRequest(&request, buffer).size()));

    channel_output.DiscardBuffer(buffer);
  }

 private:
  using Base = InvocationContext<
      NanopbFakeChannelOutput<Response, kMaxResponses, kOutputSize>,
      Service,
      kMethodId>;

  static constexpr NanopbMethod kMethodInfo =
      MethodLookup::GetNanopbMethod<Service, kMethodId>();
};

// Method invocation context for a unary RPC. Returns the status in
// call_context() and provides the response through the response() method.
template <typename Service,
          auto kMethod,
          uint32_t kMethodId,
          size_t kOutputSize>
class UnaryContext : public NanopbInvocationContext<Service,
                                                    kMethod,
                                                    kMethodId,
                                                    1,
                                                    kOutputSize> {
 private:
  using Base =
      NanopbInvocationContext<Service, kMethod, kMethodId, 1, kOutputSize>;

 public:
  using Request = typename Base::Request;
  using Response = typename Base::Response;

  template <typename... Args>
  UnaryContext(Args&&... args) : Base(std::forward<Args>(args)...) {}

  // Invokes the RPC with the provided request. Returns the status.
  Status call(const Request& request) {
    Base::output().clear();
    Response& response = Base::output().AllocateResponse();
    return CallMethodImplFunction<kMethod>(
        Base::call_context(), request, response);
  }
};

// Method invocation context for a server streaming RPC.
template <typename Service,
          auto kMethod,
          uint32_t kMethodId,
          size_t kMaxResponses,
          size_t kOutputSize>
class ServerStreamingContext : public NanopbInvocationContext<Service,
                                                              kMethod,
                                                              kMethodId,
                                                              kMaxResponses,
                                                              kOutputSize> {
 private:
  using Base = NanopbInvocationContext<Service,
                                       kMethod,
                                       kMethodId,
                                       kMaxResponses,
                                       kOutputSize>;

 public:
  using Request = typename Base::Request;
  using Response = typename Base::Response;

  template <typename... Args>
  ServerStreamingContext(Args&&... args) : Base(std::forward<Args>(args)...) {}

  // Invokes the RPC with the provided request.
  void call(const Request& request) {
    Base::template call<kMethod, NanopbServerWriter<Response>>(request);
  }

  // Returns a server writer which writes responses into the context's buffer.
  // This should not be called alongside call(); use one or the other.
  NanopbServerWriter<Response> writer() {
    return Base::template GetResponder<NanopbServerWriter<Response>>();
  }
};

// Method invocation context for a client streaming RPC.
template <typename Service,
          auto kMethod,
          uint32_t kMethodId,
          size_t kMaxResponses,
          size_t kOutputSize>
class ClientStreamingContext : public NanopbInvocationContext<Service,
                                                              kMethod,
                                                              kMethodId,
                                                              kMaxResponses,
                                                              kOutputSize> {
 private:
  using Base = NanopbInvocationContext<Service,
                                       kMethod,
                                       kMethodId,
                                       kMaxResponses,
                                       kOutputSize>;

 public:
  using Request = typename Base::Request;
  using Response = typename Base::Response;

  template <typename... Args>
  ClientStreamingContext(Args&&... args) : Base(std::forward<Args>(args)...) {}

  // Invokes the RPC.
  void call() {
    Base::template call<kMethod, NanopbServerReader<Request, Response>>();
  }

  // Returns a server reader which writes responses into the context's buffer.
  // This should not be called alongside call(); use one or the other.
  NanopbServerReader<Request, Response> reader() {
    return Base::template GetResponder<NanopbServerReader<Request, Response>>();
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
class BidirectionalStreamingContext
    : public NanopbInvocationContext<Service,
                                     kMethod,
                                     kMethodId,
                                     kMaxResponses,
                                     kOutputSize> {
 private:
  using Base = NanopbInvocationContext<Service,
                                       kMethod,
                                       kMethodId,
                                       kMaxResponses,
                                       kOutputSize>;

 public:
  using Request = typename Base::Request;
  using Response = typename Base::Response;

  template <typename... Args>
  BidirectionalStreamingContext(Args&&... args)
      : Base(std::forward<Args>(args)...) {}

  // Invokes the RPC.
  void call() {
    Base::template call<kMethod, NanopbServerReaderWriter<Request, Response>>();
  }

  // Returns a server reader which writes responses into the context's buffer.
  // This should not be called alongside call(); use one or the other.
  NanopbServerReader<Request, Response> reader() {
    return Base::template GetResponder<
        NanopbServerReaderWriter<Request, Response>>();
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
    static_cast<size_t>(internal::MethodTraits<decltype(kMethod)>::kType),
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

}  // namespace internal::test::nanopb

template <typename Service,
          auto kMethod,
          uint32_t kMethodId,
          size_t kMaxResponses,
          size_t kOutputSizeBytes>
class NanopbTestMethodContext
    : public internal::test::nanopb::Context<Service,
                                             kMethod,
                                             kMethodId,
                                             kMaxResponses,
                                             kOutputSizeBytes> {
 public:
  // Forwards constructor arguments to the service class.
  template <typename... ServiceArgs>
  NanopbTestMethodContext(ServiceArgs&&... service_args)
      : internal::test::nanopb::Context<Service,
                                        kMethod,
                                        kMethodId,
                                        kMaxResponses,
                                        kOutputSizeBytes>(
            std::forward<ServiceArgs>(service_args)...) {}
};

}  // namespace pw::rpc
