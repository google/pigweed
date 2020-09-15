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

#include "pw_rpc/internal/call.h"

namespace pw::rpc::internal {

class Packet;

// RPC server implementations provide a class that dervies from Method.
class Method {
 public:
  constexpr uint32_t id() const { return id_; }

  // The pw::rpc::Server calls method.Invoke to call a user-defined RPC. Invoke
  // calls the invoker function, which handles the RPC request and response
  // according to the RPC type and protobuf implementation and calls the
  // user-defined RPC function.
  void Invoke(ServerCall& call, const Packet& request) const {
    return invoker_(*this, call, request);
  }

 protected:
  using Invoker = void (&)(const Method&, ServerCall&, const Packet&);

  constexpr Method(uint32_t id, Invoker invoker) : id_(id), invoker_(invoker) {}

 private:
  uint32_t id_;
  Invoker invoker_;
};

}  // namespace pw::rpc::internal
