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

#include "pw_rpc/internal/method.h"

namespace pw::rpc::internal {

// Base class for different combinations of possible service methods. Derived
// classes should contain a union of different method types, one of which is a
// base Method.
class MethodUnion {
 public:
  constexpr const Method& method() const;
};

// Templated false value for use in static_assert(false) statements.
template <typename...>
constexpr std::false_type kFalse{};

// Traits struct that determines the type of an RPC service method from its
// signature. Derived MethodUnions should provide specializations for their
// method types.
template <typename Method>
struct MethodTraits {
  static_assert(kFalse<Method>,
                "The selected function is not an RPC service method");

  // Specializations must set Implementation as an alias for their method
  // implementation class.
  using Implementation = Method;
};

template <auto method>
using MethodImplementation =
    typename MethodTraits<decltype(method)>::Implementation;

class CoreMethodUnion : public MethodUnion {
 public:
  constexpr const Method& method() const { return impl_.method; }

 private:
  // All derived MethodUnions must contain a union of different method
  // implementations as their only member.
  union {
    Method method;
  } impl_;
};

constexpr const Method& MethodUnion::method() const {
  // This is an ugly hack. As all MethodUnion classes contain a union of Method
  // derivatives, CoreMethodUnion is used to extract a generic Method from the
  // specific implementation.
  return static_cast<const CoreMethodUnion*>(this)->method();
}

}  // namespace pw::rpc::internal
