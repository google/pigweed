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

#include <cstdint>
#include <limits>
#include <span>

#include "pw_containers/intrusive_list.h"
#include "pw_preprocessor/compiler.h"
#include "pw_rpc/internal/hash.h"
#include "pw_rpc/internal/method.h"
#include "pw_rpc/internal/method_union.h"

namespace pw::rpc {

// Base class for all RPC services. This cannot be instantiated directly; use a
// generated subclass instead.
//
// Services store a span of concrete method implementation classes. To support
// different Method implementations, Service stores a base MethodUnion* and the
// size of the concrete MethodUnion object.
class Service : public IntrusiveList<Service>::Item {
 public:
  uint32_t id() const { return id_; }

 protected:
  template <typename T, size_t kMethodCount>
  constexpr Service(uint32_t id, const std::array<T, kMethodCount>& methods)
      : id_(id),
        methods_(methods.data()),
        method_size_(sizeof(T)),
        method_count_(static_cast<uint16_t>(kMethodCount)) {
    PW_MODIFY_DIAGNOSTICS_PUSH();
    // GCC 10 emits spurious -Wtype-limits warnings for the static_assert.
    PW_MODIFY_DIAGNOSTIC_GCC(ignored, "-Wtype-limits");
    static_assert(kMethodCount <= std::numeric_limits<uint16_t>::max());
    PW_MODIFY_DIAGNOSTICS_POP();
  }

  // For use by tests with only one method.
  template <typename T>
  constexpr Service(uint32_t id, const T& method)
      : id_(id), methods_(&method), method_size_(sizeof(T)), method_count_(1) {}

 private:
  friend class Server;
  friend class ServiceTestHelper;

  // Finds the method with the provided method_id. Returns nullptr if no match.
  const internal::Method* FindMethod(uint32_t method_id) const;

  const uint32_t id_;
  const internal::MethodUnion* const methods_;
  const uint16_t method_size_;
  const uint16_t method_count_;
};

// Calculates the method ID of the method with the given name. Services track
// methods by this ID.
constexpr uint32_t CalculateMethodId(std::string_view method_name) {
  return internal::Hash(method_name);
}

}  // namespace pw::rpc
