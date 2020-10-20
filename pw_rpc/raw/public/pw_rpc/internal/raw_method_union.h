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
#include "pw_rpc/internal/method_union.h"
#include "pw_rpc/internal/raw_method.h"

namespace pw::rpc::internal {

// MethodUnion which stores only a raw method. For use in fully raw RPC
// services, without any additional memory overhead.
class RawMethodUnion : public MethodUnion {
 public:
  constexpr RawMethodUnion(RawMethod&& method)
      : impl_({.raw = std::move(method)}) {}

  constexpr const Method& method() const { return impl_.method; }
  constexpr const RawMethod& raw_method() const { return impl_.raw; }

 private:
  union {
    Method method;
    RawMethod raw;
  } impl_;
};

// MethodTraits specialization for a unary method.
template <typename T>
struct MethodTraits<StatusWithSize (T::*)(
    ServerContext&, ConstByteSpan, ByteSpan)> {
  static constexpr MethodType kType = MethodType::kUnary;
  using Service = T;
  using Implementation = RawMethod;
};

// MethodTraits specialization for a raw server streaming method.
template <typename T>
struct MethodTraits<void (T::*)(
    ServerContext&, ConstByteSpan, RawServerWriter&)> {
  static constexpr MethodType kType = MethodType::kServerStreaming;
  using Service = T;
  using Implementation = RawMethod;
};

template <auto method>
constexpr bool kIsRaw = std::is_same_v<MethodImplementation<method>, RawMethod>;

// Deduces the type of an implemented service method from its signature, and
// returns the appropriate MethodUnion object to invoke it.
template <auto method>
constexpr RawMethod GetRawMethodFor(uint32_t id) {
  static_assert(kIsRaw<method>,
                "GetRawMethodFor should only be called on raw RPC methods");

  using Traits = MethodTraits<decltype(method)>;
  using ServiceImpl = typename Traits::Service;

  if constexpr (Traits::kType == MethodType::kUnary) {
    constexpr auto invoker =
        +[](ServerCall& call, ConstByteSpan request, ByteSpan response) {
          return (static_cast<ServiceImpl&>(call.service()).*method)(
              call.context(), request, response);
        };
    return RawMethod::Unary<invoker>(id);
  }

  if constexpr (Traits::kType == MethodType::kServerStreaming) {
    constexpr auto invoker =
        +[](ServerCall& call, ConstByteSpan request, RawServerWriter& writer) {
          (static_cast<ServiceImpl&>(call.service()).*method)(
              call.context(), request, writer);
        };
    return RawMethod::ServerStreaming<invoker>(id);
  }

  constexpr auto fake_invoker =
      +[](ServerCall&, ConstByteSpan, RawServerWriter&) {};
  return RawMethod::ServerStreaming<fake_invoker>(0);
};

}  // namespace pw::rpc::internal
