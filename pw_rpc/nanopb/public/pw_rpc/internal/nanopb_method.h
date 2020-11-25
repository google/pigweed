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
#include <span>
#include <type_traits>

#include "pw_rpc/internal/base_server_writer.h"
#include "pw_rpc/internal/config.h"
#include "pw_rpc/internal/method.h"
#include "pw_rpc/internal/method_type.h"
#include "pw_rpc/internal/nanopb_common.h"
#include "pw_rpc/server_context.h"
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

  // Writes a response struct. Returns the following Status codes:
  //
  //   OK - the response was successfully sent
  //   FAILED_PRECONDITION - the writer is closed
  //   INTERNAL - pw_rpc was unable to encode the Nanopb protobuf
  //   other errors - the ChannelOutput failed to send the packet; the error
  //       codes are determined by the ChannelOutput implementation
  //
  Status Write(const T& response);
};

namespace internal {

class NanopbMethod;
class Packet;

// MethodTraits specialization for a static unary method.
template <typename RequestType, typename ResponseType>
struct MethodTraits<Status (*)(
    ServerContext&, const RequestType&, ResponseType&)> {
  using Implementation = NanopbMethod;
  using Request = RequestType;
  using Response = ResponseType;

  static constexpr MethodType kType = MethodType::kUnary;
  static constexpr bool kServerStreaming = false;
  static constexpr bool kClientStreaming = false;
};

// MethodTraits specialization for a unary method.
template <typename T, typename RequestType, typename ResponseType>
struct MethodTraits<Status (T::*)(
    ServerContext&, const RequestType&, ResponseType&)>
    : public MethodTraits<Status (*)(
          ServerContext&, const RequestType&, ResponseType&)> {
  using Service = T;
};

// MethodTraits specialization for a static server streaming method.
template <typename RequestType, typename ResponseType>
struct MethodTraits<void (*)(
    ServerContext&, const RequestType&, ServerWriter<ResponseType>&)> {
  using Implementation = NanopbMethod;
  using Request = RequestType;
  using Response = ResponseType;

  static constexpr MethodType kType = MethodType::kServerStreaming;
  static constexpr bool kServerStreaming = true;
  static constexpr bool kClientStreaming = false;
};

// MethodTraits specialization for a server streaming method.
template <typename T, typename RequestType, typename ResponseType>
struct MethodTraits<void (T::*)(
    ServerContext&, const RequestType&, ServerWriter<ResponseType>&)>
    : public MethodTraits<void (*)(
          ServerContext&, const RequestType&, ServerWriter<ResponseType>&)> {
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
  template <auto method>
  static constexpr bool matches() {
    return std::is_same_v<MethodImplementation<method>, NanopbMethod>;
  }

  // Creates a NanopbMethod for a unary RPC.
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
    constexpr UnaryFunction wrapper =
        [](ServerCall& call, const void* req, void* resp) {
          return CallMethodImplFunction<method>(
              call,
              *static_cast<const Request<method>*>(req),
              *static_cast<Response<method>*>(resp));
        };
    return NanopbMethod(id,
                        UnaryInvoker<AllocateSpaceFor<Request<method>>(),
                                     AllocateSpaceFor<Response<method>>()>,
                        Function{.unary = wrapper},
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
    // struct as void* and a BaseServerWriter instead of the templated
    // ServerWriter class. This wrapper is stored generically in the Function
    // union, defined below.
    constexpr ServerStreamingFunction wrapper =
        [](ServerCall& call, const void* req, BaseServerWriter& writer) {
          return CallMethodImplFunction<method>(
              call,
              *static_cast<const Request<method>*>(req),
              static_cast<ServerWriter<Response<method>>&>(writer));
        };
    return NanopbMethod(
        id,
        ServerStreamingInvoker<AllocateSpaceFor<Request<method>>()>,
        Function{.server_streaming = wrapper},
        request,
        response);
  }

  // Represents an invalid method. Used to reduce error message verbosity.
  static constexpr NanopbMethod Invalid() {
    return {0, InvalidInvoker, {}, nullptr, nullptr};
  }

  // Encodes a response protobuf with Nanopb to the provided buffer.
  StatusWithSize EncodeResponse(const void* proto_struct,
                                std::span<std::byte> buffer) const {
    return serde_.EncodeResponse(buffer, proto_struct);
  }

  // Decodes a response protobuf with Nanopb to the provided buffer. For testing
  // use.
  bool DecodeResponse(std::span<const std::byte> response,
                      void* proto_struct) const {
    return serde_.DecodeResponse(proto_struct, response);
  }

 private:
  // Generic version of the unary RPC function signature:
  //
  //   Status(ServerCall&, const Request&, Response&)
  //
  using UnaryFunction = Status (*)(ServerCall&,
                                   const void* request,
                                   void* response);

  // Generic version of the server streaming RPC function signature:
  //
  //   Status(ServerCall&, const Request&, ServerWriter<Response>&)
  //
  using ServerStreamingFunction = void (*)(ServerCall&,
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
    return std::max(sizeof(T), cfg::kNanopbStructMinBufferSize);
  }

  constexpr NanopbMethod(uint32_t id,
                         Invoker invoker,
                         Function function,
                         NanopbMessageDescriptor request,
                         NanopbMessageDescriptor response)
      : Method(id, invoker), function_(function), serde_(request, response) {}

  void CallUnary(ServerCall& call,
                 const Packet& request,
                 void* request_struct,
                 void* response_struct) const;

  void CallServerStreaming(ServerCall& call,
                           const Packet& request,
                           void* request_struct) const;

  // TODO(hepler): Add CallClientStreaming and CallBidiStreaming

  // Invoker function for unary RPCs. Allocates request and response structs by
  // size, with maximum alignment, to avoid generating unnecessary copies of
  // this function for each request/response type.
  template <size_t request_size, size_t response_size>
  static void UnaryInvoker(const Method& method,
                           ServerCall& call,
                           const Packet& request) {
    _PW_RPC_NANOPB_STRUCT_STORAGE_CLASS
    std::aligned_storage_t<request_size, alignof(std::max_align_t)>
        request_struct{};
    _PW_RPC_NANOPB_STRUCT_STORAGE_CLASS
    std::aligned_storage_t<response_size, alignof(std::max_align_t)>
        response_struct{};

    static_cast<const NanopbMethod&>(method).CallUnary(
        call, request, &request_struct, &response_struct);
  }

  // Invoker function for server streaming RPCs. Allocates space for a request
  // struct. Ignores the payload buffer since resposnes are sent through the
  // ServerWriter.
  template <size_t request_size>
  static void ServerStreamingInvoker(const Method& method,
                                     ServerCall& call,
                                     const Packet& request) {
    _PW_RPC_NANOPB_STRUCT_STORAGE_CLASS
    std::aligned_storage_t<request_size, alignof(std::max_align_t)>
        request_struct{};

    static_cast<const NanopbMethod&>(method).CallServerStreaming(
        call, request, &request_struct);
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

}  // namespace internal

template <typename T>
Status ServerWriter<T>::Write(const T& response) {
  if (!open()) {
    return Status::FailedPrecondition();
  }

  std::span<std::byte> buffer = AcquirePayloadBuffer();

  if (auto result =
          static_cast<const internal::NanopbMethod&>(method()).EncodeResponse(
              &response, buffer);
      result.ok()) {
    return ReleasePayloadBuffer(buffer.first(result.size()));
  }

  ReleasePayloadBuffer();
  return Status::Internal();
}

}  // namespace pw::rpc
