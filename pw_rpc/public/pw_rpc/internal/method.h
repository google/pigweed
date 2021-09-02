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

#include <cstddef>
#include <cstdint>
#include <utility>

#include "pw_rpc/internal/call.h"

namespace pw::rpc::internal {

class Packet;

// Each supported protobuf implementation provides a class that derives from
// Method. The implementation classes provide the following public interface:
/*
class MethodImpl : public Method {
  // True if the provided function signature is valid for this method impl.
  template <auto kMethod>
  static constexpr bool matches();

  // Creates a unary method instance.
  template <auto kMethod>
  static constexpr MethodImpl Unary(uint32_t id, [optional args]);

  // Creates a server streaming method instance.
  template <auto kMethod>
  static constexpr MethodImpl ServerStreaming(uint32_t id, [optional args]);

  // Creates a client streaming method instance.
  static constexpr MethodImpl ClientStreaming(uint32_t id, [optional args]);

  // Creates a bidirectional streaming method instance.
  static constexpr MethodImpl BidirectionalStreaming(uint32_t id,
                                                     [optional args]);

  // Creates a method instance for when the method implementation function has
  // an incorrect signature. Having this helps reduce error message verbosity.
  static constexpr MethodImpl Invalid();
};
*/
// Method implementations must pass a test that uses the MethodImplTester class
// in pw_rpc/internal/method_impl_tester.h.
class Method {
 public:
  constexpr uint32_t id() const { return id_; }

  // The pw::rpc::Server calls method.Invoke to call a user-defined RPC. Invoke
  // calls the invoker function, which handles the RPC request and response
  // according to the RPC type and protobuf implementation and calls the
  // user-defined RPC function.
  void Invoke(CallContext& call, const Packet& request) const {
    return invoker_(*this, call, request);
  }

 protected:
  using Invoker = void (&)(const Method&, CallContext&, const Packet&);

  static constexpr void InvalidInvoker(const Method&,
                                       CallContext&,
                                       const Packet&) {}

  constexpr Method(uint32_t id, Invoker invoker) : id_(id), invoker_(invoker) {}

 private:
  uint32_t id_;
  Invoker invoker_;
};

// Traits struct that determines the type of an RPC service method from its
// signature. Derived Methods should provide specializations for their method
// types.
template <typename Method>
struct MethodTraits {
  // Specializations must set Implementation as an alias for their method
  // implementation class.
  using Implementation = Method;

  // Specializations must set kType to the MethodType.
  // static constexpr MethodType kType = (method type);

  // Specializations for member function types must set Service to an alias to
  // for the implemented service class.
  // using Service = (derived service class);

  // Specializations may provide the C++ types of the requests and responses if
  // relevant.
  using Request = void;
  using Response = void;
};

template <auto kMethod>
using MethodImplementation =
    typename MethodTraits<decltype(kMethod)>::Implementation;

template <auto kMethod>
using Request = typename MethodTraits<decltype(kMethod)>::Request;

template <auto kMethod>
using Response = typename MethodTraits<decltype(kMethod)>::Response;

// Function that calls a user-defined method implementation function from a
// CallContext object.
template <auto kMethod, typename... Args>
constexpr auto CallMethodImplFunction(CallContext& call, Args&&... args) {
  // If the method impl is a member function, deduce the type of the
  // user-defined service from it, then call the method on the service.
  if constexpr (std::is_member_function_pointer_v<decltype(kMethod)>) {
    return (static_cast<typename MethodTraits<decltype(kMethod)>::Service&>(
                call.service()).*
            kMethod)(call.context(), std::forward<Args>(args)...);
  } else {
    return kMethod(call.context(), std::forward<Args>(args)...);
  }
}

// Identifies a base class from a member function it defines. This should be
// used with decltype to retrieve the base service class.
template <typename T, typename U>
T BaseFromMember(U T::*);

// The base generated service of an RPC service class.
template <typename Service>
using GeneratedService =
    decltype(BaseFromMember(&Service::_PwRpcInternalGeneratedBase));

}  // namespace pw::rpc::internal
