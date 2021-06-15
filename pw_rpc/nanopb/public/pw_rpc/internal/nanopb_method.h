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
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <type_traits>

#include "pw_function/function.h"
#include "pw_rpc/internal/config.h"
#include "pw_rpc/internal/method.h"
#include "pw_rpc/internal/method_type.h"
#include "pw_rpc/internal/nanopb_common.h"
#include "pw_rpc/internal/responder.h"
#include "pw_rpc/nanopb/server_reader_writer.h"
#include "pw_rpc/server_context.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"

namespace pw::rpc::internal {

class NanopbMethod;
class Packet;

// Expected function signatures for user-implemented RPC functions.
template <typename Request, typename Response>
using NanopbSynchronousUnary = Status(ServerContext&,
                                      const Request&,
                                      Response&);

template <typename Request, typename Response>
using NanopbServerStreaming = void(ServerContext&,
                                   const Request&,
                                   NanopbServerWriter<Response>&);

template <typename Request, typename Response>
using NanopbClientStreaming = void(ServerContext&,
                                   NanopbServerReader<Request, Response>&);

template <typename Request, typename Response>
using NanopbBidirectionalStreaming =
    void(ServerContext&, NanopbServerReaderWriter<Request, Response>&);

// MethodTraits specialization for a static unary method.
template <typename Req, typename Resp>
struct MethodTraits<NanopbSynchronousUnary<Req, Resp>*> {
  using Implementation = NanopbMethod;
  using Request = Req;
  using Response = Resp;

  static constexpr MethodType kType = MethodType::kUnary;
  static constexpr bool kServerStreaming = false;
  static constexpr bool kClientStreaming = false;
};

// MethodTraits specialization for a unary method.
template <typename T, typename Req, typename Resp>
struct MethodTraits<NanopbSynchronousUnary<Req, Resp>(T::*)>
    : MethodTraits<NanopbSynchronousUnary<Req, Resp>*> {
  using Service = T;
};

// MethodTraits specialization for a static server streaming method.
template <typename Req, typename Resp>
struct MethodTraits<NanopbServerStreaming<Req, Resp>*> {
  using Implementation = NanopbMethod;
  using Request = Req;
  using Response = Resp;

  static constexpr MethodType kType = MethodType::kServerStreaming;
  static constexpr bool kServerStreaming = true;
  static constexpr bool kClientStreaming = false;
};

// MethodTraits specialization for a server streaming method.
template <typename T, typename Req, typename Resp>
struct MethodTraits<NanopbServerStreaming<Req, Resp>(T::*)>
    : MethodTraits<NanopbServerStreaming<Req, Resp>*> {
  using Service = T;
};

// MethodTraits specialization for a static server streaming method.
template <typename Req, typename Resp>
struct MethodTraits<NanopbClientStreaming<Req, Resp>*> {
  using Implementation = NanopbMethod;
  using Request = Req;
  using Response = Resp;

  static constexpr MethodType kType = MethodType::kClientStreaming;
  static constexpr bool kServerStreaming = false;
  static constexpr bool kClientStreaming = true;
};

// MethodTraits specialization for a server streaming method.
template <typename T, typename Req, typename Resp>
struct MethodTraits<NanopbClientStreaming<Req, Resp>(T::*)>
    : MethodTraits<NanopbClientStreaming<Req, Resp>*> {
  using Service = T;
};

// MethodTraits specialization for a static server streaming method.
template <typename Req, typename Resp>
struct MethodTraits<NanopbBidirectionalStreaming<Req, Resp>*> {
  using Implementation = NanopbMethod;
  using Request = Req;
  using Response = Resp;

  static constexpr MethodType kType = MethodType::kBidirectionalStreaming;
  static constexpr bool kServerStreaming = true;
  static constexpr bool kClientStreaming = true;
};

// MethodTraits specialization for a server streaming method.
template <typename T, typename Req, typename Resp>
struct MethodTraits<NanopbBidirectionalStreaming<Req, Resp>(T::*)>
    : MethodTraits<NanopbBidirectionalStreaming<Req, Resp>*> {
  using Service = T;
};

template <auto method>
using Request = typename MethodTraits<decltype(method)>::Request;

template <auto method>
using Response = typename MethodTraits<decltype(method)>::Response;

// The NanopbMethod class invokes user-defined service methods. When a
// pw::rpc::Server receives an RPC request packet, it looks up the matching
// NanopbMethod instance and calls its Invoke method, which eventually calls
// into the user-defined RPC function.
//
// A NanopbMethod instance is created for each user-defined RPC in the pw_rpc
// generated code. The NanopbMethod stores a pointer to the RPC function, a
// pointer to an "invoker" function that calls that function, and pointers to
// the Nanopb descriptors used to encode and decode request and response
// structs.
class NanopbMethod : public Method {
 public:
  template <auto method, typename RequestType, typename ResponseType>
  static constexpr bool matches() {
    return std::is_same_v<MethodImplementation<method>, NanopbMethod> &&
           std::is_same_v<RequestType, Request<method>> &&
           std::is_same_v<ResponseType, Response<method>>;
  }

  // Creates a NanopbMethod for a synchronous unary RPC.
  template <auto method>
  static constexpr NanopbMethod Unary(uint32_t id,
                                      NanopbMessageDescriptor request,
                                      NanopbMessageDescriptor response) {
    // Define a wrapper around the user-defined function that takes the
    // request and response protobuf structs as void*. This wrapper is stored
    // generically in the Function union, defined below.
    //
    // In optimized builds, the compiler inlines the user-defined function into
    // this wrapper, elminating any overhead.
    constexpr SynchronousUnaryFunction wrapper =
        [](ServerCall& call, const void* req, void* resp) {
          return CallMethodImplFunction<method>(
              call,
              *static_cast<const Request<method>*>(req),
              *static_cast<Response<method>*>(resp));
        };
    return NanopbMethod(
        id,
        SynchronousUnaryInvoker<AllocateSpaceFor<Request<method>>(),
                                AllocateSpaceFor<Response<method>>()>,
        Function{.synchronous_unary = wrapper},
        request,
        response);
  }

  // Creates a NanopbMethod for a server-streaming RPC.
  template <auto method>
  static constexpr NanopbMethod ServerStreaming(
      uint32_t id,
      NanopbMessageDescriptor request,
      NanopbMessageDescriptor response) {
    // Define a wrapper around the user-defined function that takes the request
    // struct as void* and a GenericNanopbResponder instead of the
    // templated NanopbServerWriter class. This wrapper is stored generically in
    // the Function union, defined below.
    constexpr UnaryRequestFunction wrapper =
        [](ServerCall& call, const void* req, GenericNanopbResponder& writer) {
          return CallMethodImplFunction<method>(
              call,
              *static_cast<const Request<method>*>(req),
              static_cast<NanopbServerWriter<Response<method>>&>(writer));
        };
    return NanopbMethod(
        id,
        UnaryRequestInvoker<AllocateSpaceFor<Request<method>>()>,
        Function{.unary_request = wrapper},
        request,
        response);
  }

  // Creates a NanopbMethod for a client-streaming RPC.
  template <auto method>
  static constexpr NanopbMethod ClientStreaming(
      uint32_t id,
      NanopbMessageDescriptor request,
      NanopbMessageDescriptor response) {
    constexpr StreamRequestFunction wrapper = [](ServerCall& call,
                                                 GenericNanopbResponder&
                                                     reader) {
      return CallMethodImplFunction<method>(
          call,
          static_cast<NanopbServerReader<Request<method>, Response<method>>&>(
              reader));
    };
    return NanopbMethod(id,
                        StreamRequestInvoker<Request<method>>,
                        Function{.stream_request = wrapper},
                        request,
                        response);
  }

  // Creates a NanopbMethod for a bidirectional-streaming RPC.
  template <auto method>
  static constexpr NanopbMethod BidirectionalStreaming(
      uint32_t id,
      NanopbMessageDescriptor request,
      NanopbMessageDescriptor response) {
    constexpr StreamRequestFunction wrapper =
        [](ServerCall& call, GenericNanopbResponder& reader_writer) {
          return CallMethodImplFunction<method>(
              call,
              static_cast<
                  NanopbServerReaderWriter<Request<method>, Response<method>>&>(
                  reader_writer));
        };
    return NanopbMethod(id,
                        StreamRequestInvoker<Request<method>>,
                        Function{.stream_request = wrapper},
                        request,
                        response);
  }

  // Represents an invalid method. Used to reduce error message verbosity.
  static constexpr NanopbMethod Invalid() {
    return {0, InvalidInvoker, {}, nullptr, nullptr};
  }

  // Give access to the serializer/deserializer object for converting requests
  // and responses between the wire format and Nanopb structs.
  const NanopbMethodSerde& serde() const { return serde_; }

 private:
  // Generic function signature for synchronous unary RPCs.
  using SynchronousUnaryFunction = Status (*)(ServerCall&,
                                              const void* request,
                                              void* response);

  // Generic function signature for asynchronous unary and server streaming
  // RPCs.
  using UnaryRequestFunction = void (*)(ServerCall&,
                                        const void* request,
                                        GenericNanopbResponder& writer);

  // Generic function signature for client and bidirectional streaming RPCs.
  using StreamRequestFunction = void (*)(ServerCall&,
                                         GenericNanopbResponder& reader_writer);

  // The Function union stores a pointer to a generic version of the
  // user-defined RPC function. Using a union instead of void* avoids
  // reinterpret_cast, which keeps this class fully constexpr.
  union Function {
    SynchronousUnaryFunction synchronous_unary;
    UnaryRequestFunction unary_request;
    StreamRequestFunction stream_request;
  };

  // Allocates space for a struct. Rounds up to a reasonable minimum size to
  // avoid generating unnecessary copies of the invoker functions.
  template <typename T>
  static constexpr size_t AllocateSpaceFor() {
    return std::max(sizeof(T), cfg::kNanopbStructMinBufferSize);
  }

  constexpr NanopbMethod(uint32_t id,
                         Invoker invoker,
                         Function function,
                         NanopbMessageDescriptor request,
                         NanopbMessageDescriptor response)
      : Method(id, invoker), function_(function), serde_(request, response) {}

  void CallSynchronousUnary(ServerCall& call,
                            const Packet& request,
                            void* request_struct,
                            void* response_struct) const;

  void CallUnaryRequest(ServerCall& call,
                        const Packet& request,
                        void* request_struct) const;

  // Invoker function for synchronous unary RPCs. Allocates request and response
  // structs by size, with maximum alignment, to avoid generating unnecessary
  // copies of this function for each request/response type.
  template <size_t kRequestSize, size_t kResponseSize>
  static void SynchronousUnaryInvoker(const Method& method,
                                      ServerCall& call,
                                      const Packet& request) {
    _PW_RPC_NANOPB_STRUCT_STORAGE_CLASS
    std::aligned_storage_t<kRequestSize, alignof(std::max_align_t)>
        request_struct{};
    _PW_RPC_NANOPB_STRUCT_STORAGE_CLASS
    std::aligned_storage_t<kResponseSize, alignof(std::max_align_t)>
        response_struct{};

    static_cast<const NanopbMethod&>(method).CallSynchronousUnary(
        call, request, &request_struct, &response_struct);
  }

  // Invoker function for server streaming RPCs. Allocates space for a request
  // struct. Ignores the payload buffer since resposnes are sent through the
  // NanopbServerWriter.
  template <size_t kRequestSize>
  static void UnaryRequestInvoker(const Method& method,
                                  ServerCall& call,
                                  const Packet& request) {
    _PW_RPC_NANOPB_STRUCT_STORAGE_CLASS
    std::aligned_storage_t<kRequestSize, alignof(std::max_align_t)>
        request_struct{};

    static_cast<const NanopbMethod&>(method).CallUnaryRequest(
        call, request, &request_struct);
  }

  // Invoker function for client or bidirectional streaming RPCs.
  template <typename Request>
  static void StreamRequestInvoker(const Method& method,
                                   ServerCall& call,
                                   const Packet&) {
    BaseNanopbServerReader<Request> reader_writer(call);
    static_cast<const NanopbMethod&>(method).function_.stream_request(
        call, reader_writer);
  }

  // Decodes a request protobuf with Nanopb to the provided buffer. Sends an
  // error packet if the request failed to decode.
  bool DecodeRequest(Channel& channel,
                     const Packet& request,
                     void* proto_struct) const;

  // Encodes a response and sends it over the provided channel.
  void SendResponse(Channel& channel,
                    const Packet& request,
                    const void* response_struct,
                    Status status) const;

  // Stores the user-defined RPC in a generic wrapper.
  Function function_;

  // Serde used to encode and decode Nanopb structs.
  NanopbMethodSerde serde_;
};

}  // namespace pw::rpc::internal
