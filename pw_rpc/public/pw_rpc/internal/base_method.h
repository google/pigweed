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

namespace pw::rpc::internal {

// RPC server implementations provide a Method class in the
// pw_rpc/internal/method.h header that is derived from BaseMethod.
class BaseMethod {
 public:
  constexpr uint32_t id() const { return id_; }

  // Implementations must provide the Invoke method, which the Server calls:
  //
  // StatusWithSize Invoke(ServerCall& call,
  //                       span<const std::byte> request,
  //                       span<std::byte> payload_buffer) const;

 protected:
  constexpr BaseMethod(uint32_t id) : id_(id) {}

 private:
  uint32_t id_;
};

}  // namespace pw::rpc::internal
