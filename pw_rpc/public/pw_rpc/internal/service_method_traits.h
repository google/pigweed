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

#include "pw_rpc/internal/method_union.h"

namespace pw::rpc::internal {

// Gets information about a service and method at compile-time. Uses a pointer
// to a member function of the service implementation to identify the service
// class, generated service class, and Method object.
template <auto lookup_function, auto impl_method, uint32_t method_id>
class ServiceMethodTraits {
 public:
  ServiceMethodTraits() = delete;

  // Type of the service implementation derived class.
  using Service = MethodService<impl_method>;

  using MethodImpl = MethodImplementation<impl_method>;

  // Reference to the Method object corresponding to this method.
  static constexpr const MethodImpl& method() {
    return *lookup_function(method_id);
  }

  static_assert(lookup_function(method_id) != nullptr,
                "The selected function is not an RPC service method");
};

}  // namespace pw::rpc::internal
