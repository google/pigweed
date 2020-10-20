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
#include "pw_rpc/internal/nanopb_method.h"
#include "pw_rpc/internal/raw_method_union.h"

namespace pw::rpc::internal {

// Method union which holds either a nanopb or a raw method.
class NanopbMethodUnion : public MethodUnion {
 public:
  constexpr NanopbMethodUnion(RawMethod&& method)
      : impl_({.raw = std::move(method)}) {}
  constexpr NanopbMethodUnion(NanopbMethod&& method)
      : impl_({.nanopb = std::move(method)}) {}

  constexpr const Method& method() const { return impl_.method; }
  constexpr const RawMethod& raw_method() const { return impl_.raw; }
  constexpr const NanopbMethod& nanopb_method() const { return impl_.nanopb; }

 private:
  union {
    Method method;
    RawMethod raw;
    NanopbMethod nanopb;
  } impl_;
};

// Specialization for a nanopb unary method.
template <typename T, typename RequestType, typename ResponseType>
struct MethodTraits<Status (T::*)(
    ServerContext&, const RequestType&, ResponseType&)> {
  static constexpr MethodType kType = MethodType::kUnary;

  using Service = T;
  using Implementation = NanopbMethod;
  using Request = RequestType;
  using Response = ResponseType;
};

// Specialization for a nanopb server streaming method.
template <typename T, typename RequestType, typename ResponseType>
struct MethodTraits<void (T::*)(
    ServerContext&, const RequestType&, ServerWriter<ResponseType>&)> {
  static constexpr MethodType kType = MethodType::kServerStreaming;

  using Service = T;
  using Implementation = NanopbMethod;
  using Request = RequestType;
  using Response = ResponseType;
};

template <auto method>
constexpr bool kIsNanopb =
    std::is_same_v<MethodImplementation<method>, NanopbMethod>;

// Deduces the type of an implemented nanopb service method from its signature,
// and returns the appropriate Method object to invoke it.
template <auto method>
constexpr NanopbMethod GetNanopbMethodFor(
    uint32_t id,
    NanopbMessageDescriptor request_fields,
    NanopbMessageDescriptor response_fields) {
  static_assert(
      kIsNanopb<method>,
      "GetNanopbMethodFor should only be called on nanopb RPC methods");

  using Traits = MethodTraits<decltype(method)>;
  using ServiceImpl = typename Traits::Service;

  if constexpr (Traits::kType == MethodType::kUnary) {
    constexpr auto invoker = +[](ServerCall& call,
                                 const typename Traits::Request& request,
                                 typename Traits::Response& response) {
      return (static_cast<ServiceImpl&>(call.service()).*method)(
          call.context(), request, response);
    };
    return NanopbMethod::Unary<invoker>(id, request_fields, response_fields);
  }

  if constexpr (Traits::kType == MethodType::kServerStreaming) {
    constexpr auto invoker =
        +[](ServerCall& call,
            const typename Traits::Request& request,
            ServerWriter<typename Traits::Response>& writer) {
          (static_cast<ServiceImpl&>(call.service()).*method)(
              call.context(), request, writer);
        };
    return NanopbMethod::ServerStreaming<invoker>(
        id, request_fields, response_fields);
  }

  constexpr auto fake_invoker =
      +[](ServerCall&, const int&, ServerWriter<int>&) {};
  return NanopbMethod::ServerStreaming<fake_invoker>(0, nullptr, nullptr);
}

// Returns either a raw or nanopb method object, depending on an implemented
// function's signature.
template <auto method>
constexpr auto GetNanopbOrRawMethodFor(
    uint32_t id,
    [[maybe_unused]] NanopbMessageDescriptor request_fields,
    [[maybe_unused]] NanopbMessageDescriptor response_fields) {
  if constexpr (kIsRaw<method>) {
    return GetRawMethodFor<method>(id);
  } else {
    return GetNanopbMethodFor<method>(id, request_fields, response_fields);
  }
};

}  // namespace pw::rpc::internal
