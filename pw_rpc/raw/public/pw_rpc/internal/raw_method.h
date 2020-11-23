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

#include "pw_bytes/span.h"
#include "pw_rpc/internal/base_server_writer.h"
#include "pw_rpc/internal/method.h"
#include "pw_rpc/internal/method_type.h"
#include "pw_status/status_with_size.h"

namespace pw::rpc {

class RawServerWriter : public internal::BaseServerWriter {
 public:
  RawServerWriter() = default;
  RawServerWriter(RawServerWriter&&) = default;
  RawServerWriter& operator=(RawServerWriter&&) = default;

  ~RawServerWriter();

  // Returns a buffer in which a response payload can be built.
  ByteSpan PayloadBuffer() { return AcquirePayloadBuffer(); }

  // Sends a response packet with the given raw payload. The payload can either
  // be in the buffer previously acquired from PayloadBuffer(), or an arbitrary
  // external buffer.
  Status Write(ConstByteSpan response);
};

namespace internal {

// A RawMethod is a method invoker which does not perform any automatic protobuf
// serialization or deserialization. The implementer is given the raw binary
// payload of incoming requests, and is responsible for encoding responses to a
// provided buffer. This is intended for use in methods which would have large
// protobuf data structure overhead to lower stack usage, or in methods packing
// responses up to a channel's MTU.
class RawMethod : public Method {
 public:
  template <auto method>
  static constexpr bool matches() {
    return std::is_same_v<MethodImplementation<method>, RawMethod>;
  }

  template <auto method>
  static constexpr RawMethod Unary(uint32_t id) {
    constexpr UnaryFunction wrapper =
        [](ServerCall& call, ConstByteSpan req, ByteSpan res) {
          return CallMethodImplFunction<method>(call, req, res);
        };
    return RawMethod(id, UnaryInvoker, Function{.unary = wrapper});
  }

  template <auto method>
  static constexpr RawMethod ServerStreaming(uint32_t id) {
    constexpr ServerStreamingFunction wrapper =
        [](ServerCall& call, ConstByteSpan request, BaseServerWriter& writer) {
          CallMethodImplFunction<method>(
              call, request, static_cast<RawServerWriter&>(writer));
        };
    return RawMethod(
        id, ServerStreamingInvoker, Function{.server_streaming = wrapper});
  }

  // Represents an invalid method. Used to reduce error message verbosity.
  static constexpr RawMethod Invalid() { return {0, InvalidInvoker, {}}; }

 private:
  using UnaryFunction = StatusWithSize (*)(ServerCall&,
                                           ConstByteSpan,
                                           ByteSpan);

  using ServerStreamingFunction = void (*)(ServerCall&,
                                           ConstByteSpan,
                                           BaseServerWriter&);
  union Function {
    UnaryFunction unary;
    ServerStreamingFunction server_streaming;
    // TODO(frolv): Support client and bidirectional streaming.
  };

  constexpr RawMethod(uint32_t id, Invoker invoker, Function function)
      : Method(id, invoker), function_(function) {}

  static void UnaryInvoker(const Method& method,
                           ServerCall& call,
                           const Packet& request) {
    static_cast<const RawMethod&>(method).CallUnary(call, request);
  }

  static void ServerStreamingInvoker(const Method& method,
                                     ServerCall& call,
                                     const Packet& request) {
    static_cast<const RawMethod&>(method).CallServerStreaming(call, request);
  }

  void CallUnary(ServerCall& call, const Packet& request) const;
  void CallServerStreaming(ServerCall& call, const Packet& request) const;

  // Stores the user-defined RPC in a generic wrapper.
  Function function_;
};

// MethodTraits specialization for a static raw unary method.
template <>
struct MethodTraits<StatusWithSize (*)(
    ServerContext&, ConstByteSpan, ByteSpan)> {
  using Implementation = RawMethod;
  static constexpr MethodType kType = MethodType::kUnary;
};

// MethodTraits specialization for a raw unary method.
template <typename T>
struct MethodTraits<StatusWithSize (T::*)(
    ServerContext&, ConstByteSpan, ByteSpan)> {
  using Implementation = RawMethod;
  static constexpr MethodType kType = MethodType::kUnary;
  using Service = T;
};

// MethodTraits specialization for a static raw server streaming method.
template <>
struct MethodTraits<void (*)(ServerContext&, ConstByteSpan, RawServerWriter&)> {
  using Implementation = RawMethod;
  static constexpr MethodType kType = MethodType::kServerStreaming;
};

// MethodTraits specialization for a raw server streaming method.
template <typename T>
struct MethodTraits<void (T::*)(
    ServerContext&, ConstByteSpan, RawServerWriter&)> {
  using Implementation = RawMethod;
  static constexpr MethodType kType = MethodType::kServerStreaming;
  using Service = T;
};

}  // namespace internal
}  // namespace pw::rpc
