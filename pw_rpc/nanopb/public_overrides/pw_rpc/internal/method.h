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

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "pw_rpc/internal/base_method.h"
#include "pw_rpc/internal/base_server_writer.h"
#include "pw_rpc/server_context.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"

namespace pw::rpc {

// Define the Nanopb version of the the ServerWriter class.
template <typename T>
class ServerWriter : public internal::BaseServerWriter {
 public:
  // Allow default construction so that users can declare a variable into which
  // to move ServerWriters from RPC calls.
  constexpr ServerWriter() = default;

  ServerWriter(ServerWriter&&) = default;
  ServerWriter& operator=(ServerWriter&&) = default;

  // Writes a response struct. Returns Status::OK on success, or
  // Status::FAILED_PRECONDITION if the writer is closed.
  Status Write(const T& response);
};

namespace internal {

// Use a void* to cover both Nanopb 3's pb_field_s and Nanopb 4's pb_msgdesc_s.
using NanopbMessageDescriptor = const void*;

// Extracts the request and response proto types from a method.
template <typename Method>
struct RpcTraits;

// Specialization for unary RPCs.
template <typename RequestType, typename ResponseType>
struct RpcTraits<Status (*)(
    ServerContext&, const RequestType&, ResponseType&)> {
  using Request = RequestType;
  using Response = ResponseType;
};

// Specialization for server streaming RPCs.
template <typename RequestType, typename ResponseType>
struct RpcTraits<Status (*)(
    ServerContext&, const RequestType&, ServerWriter<ResponseType>&)> {
  using Request = RequestType;
  using Response = ResponseType;
};

template <auto method>
using Request = typename RpcTraits<decltype(method)>::Request;

template <auto method>
using Response = typename RpcTraits<decltype(method)>::Response;

// The Method class invokes user-defined service methods. When a pw::rpc::Server
// receives an RPC request packet, it looks up the matching Method instance and
// calls its Invoke method, which eventually calls into the user-defined RPC
// function.
//
// A Method instance is created for each user-defined RPC in the pw_rpc
// generated code. The Nanopb Method stores a pointer to the RPC function, a
// pointer to an "invoker" function that calls that function, and pointers to
// the Nanopb descriptors used to encode and decode request and response
// structs.
class Method : public BaseMethod {
 public:
  // Creates a Method for a unary RPC.
  template <auto method>
  static constexpr Method Unary(uint32_t id,
                                NanopbMessageDescriptor request,
                                NanopbMessageDescriptor response) {
    // Define a wrapper around the user-defined function that takes the
    // request and response protobuf structs as void*. This wrapper is stored
    // generically in the Function union, defined below.
    //
    // In optimized builds, the compiler inlines the user-defined function into
    // this wrapper, elminating any overhead.
    return Method({.unary =
                       [](ServerContext& ctx, const void* req, void* resp) {
                         return method(
                             ctx,
                             *static_cast<const Request<method>*>(req),
                             *static_cast<Response<method>*>(resp));
                       }},
                  UnaryInvoker<AllocateSpaceFor<Request<method>>(),
                               AllocateSpaceFor<Response<method>>()>,
                  id,
                  request,
                  response);
  }

  // Creates a Method for a server-streaming RPC.
  template <auto method>
  static constexpr Method ServerStreaming(uint32_t id,
                                          NanopbMessageDescriptor request,
                                          NanopbMessageDescriptor response) {
    // Define a wrapper around the user-defined function that takes the request
    // struct as void* and a BaseServerWriter instead of the templated
    // ServerWriter class. This wrapper is stored generically in the Function
    // union, defined below.
    return Method(
        {.server_streaming =
             [](ServerContext& ctx, const void* req, BaseServerWriter& resp) {
               return method(
                   ctx,
                   *static_cast<const Request<method>*>(req),
                   static_cast<ServerWriter<Response<method>>&>(resp));
             }},
        ServerStreamingInvoker<AllocateSpaceFor<Request<method>>()>,
        id,
        request,
        response);
  }

  // The pw::rpc::Server calls method.Invoke to call a user-defined RPC. Invoke
  // calls the invoker function, which encodes and decodes the request and
  // response (if any) and calls the user-defined RPC function.
  StatusWithSize Invoke(ServerCall& call,
                        span<const std::byte> request,
                        span<std::byte> payload_buffer) const {
    return invoker_(*this, call, request, payload_buffer);
  }

  // Decodes a request protobuf with Nanopb to the provided buffer.
  Status DecodeRequest(span<const std::byte> buffer, void* proto_struct) const;

  // Encodes a response protobuf with Nanopb to the provided buffer.
  StatusWithSize EncodeResponse(const void* proto_struct,
                                span<std::byte> buffer) const;

 private:
  // Generic version of the unary RPC function signature:
  //
  //   Status(ServerContext&, const Request&, Response&)
  //
  using UnaryFunction = Status (*)(ServerContext&,
                                   const void* request,
                                   void* response);

  // Generic version of the server streaming RPC function signature:
  //
  //   Status(ServerContext&, const Request&, ServerWriter<Response>&)
  //
  using ServerStreamingFunction = Status (*)(ServerContext&,
                                             const void* request,
                                             BaseServerWriter& writer);

  // The Function union stores a pointer to a generic version of the
  // user-defined RPC function. Using a union instead of void* avoids
  // reinterpret_cast, which keeps this class fully constexpr.
  union Function {
    UnaryFunction unary;
    ServerStreamingFunction server_streaming;
    // TODO(hepler): Add client_streaming and bidi_streaming
  };

  // Allocates space for a struct. Rounds up to a reasonable minimum size to
  // avoid generating unnecessary copies of the invoker functions.
  template <typename T>
  static constexpr size_t AllocateSpaceFor() {
    return std::max(sizeof(T), size_t(64));
  }

  // The Invoker allocates request/response structs on the stack and calls the
  // RPC according to its type (unary, server streaming, etc.). The Invoker
  // returns the number of bytes written to the response buffer, if any.
  using Invoker = StatusWithSize (&)(const Method&,
                                     ServerCall&,
                                     span<const std::byte>,
                                     span<std::byte>);

  constexpr Method(Function function,
                   Invoker invoker,
                   uint32_t id,
                   NanopbMessageDescriptor request,
                   NanopbMessageDescriptor response)
      : BaseMethod(id),
        invoker_(invoker),
        function_(function),
        request_fields_(request),
        response_fields_(response) {}

  StatusWithSize CallUnary(ServerCall& call,
                           span<const std::byte> request_buffer,
                           span<std::byte> response_buffer,
                           void* request_struct,
                           void* response_struct) const;

  StatusWithSize CallServerStreaming(ServerCall& call,
                                     span<const std::byte> request_buffer,
                                     void* request_struct) const;

  // TODO(hepler): Add CallClientStreaming and CallBidiStreaming

  // Invoker function for unary RPCs. Allocates request and response structs by
  // size, with maximum alignment, to avoid generating unnecessary copies of
  // this function for each request/response type.
  template <size_t request_size, size_t response_size>
  static StatusWithSize UnaryInvoker(const Method& method,
                                     ServerCall& call,
                                     span<const std::byte> request_buffer,
                                     span<std::byte> response_buffer) {
    std::aligned_storage_t<request_size, alignof(std::max_align_t)>
        request_struct{};
    std::aligned_storage_t<response_size, alignof(std::max_align_t)>
        response_struct{};

    return method.CallUnary(call,
                            request_buffer,
                            response_buffer,
                            &request_struct,
                            &response_struct);
  }

  // Invoker function for server streaming RPCs. Allocates space for a request
  // struct. Ignores the payload buffer since resposnes are sent through the
  // ServerWriter.
  template <size_t request_size>
  static StatusWithSize ServerStreamingInvoker(
      const Method& method,
      ServerCall& call,
      span<const std::byte> request_buffer,
      span<std::byte> /* payload not used */) {
    std::aligned_storage_t<request_size, alignof(std::max_align_t)>
        request_struct{};

    return method.CallServerStreaming(call, request_buffer, &request_struct);
  }

  // Allocates memory for the request/response structs and invokes the
  // user-defined RPC based on its type (unary, server streaming, etc.).
  Invoker invoker_;

  // Stores the user-defined RPC in a generic wrapper.
  Function function_;

  // Pointers to the descriptors used to encode and decode Nanopb structs.
  NanopbMessageDescriptor request_fields_;
  NanopbMessageDescriptor response_fields_;
};

}  // namespace internal

template <typename T>
Status ServerWriter<T>::Write(const T& response) {
  // TODO(hepler): Need to think about what Status this returns. Should channels
  // return a status for their SendAndReleaseBuffer?
  span<std::byte> buffer = AcquireBuffer();

  if (auto result = method().EncodeResponse(&response, buffer); result.ok()) {
    return SendAndReleaseBuffer(buffer.first(result.size()));
  }

  return Status::INTERNAL;
}

}  // namespace pw::rpc
