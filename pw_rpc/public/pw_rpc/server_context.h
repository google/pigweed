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

namespace pw::rpc {

class ServerContext;

namespace internal {

ServerContext& GlobalServerContextStub();

}  // namespace internal

// The ServerContext class is DEPRECATED and will be removed from pw_rpc. All
// information formerly in the ServerContext is accessible through the
// ServerReader/Writer object.
//
// The only case where the information in a ServerContext is not available is
// synchronous unary RPCs. If information like channel_id() is needed in a unary
// RPC, just use an asynchronous unary RPC.
class ServerContext {
 public:
  constexpr ServerContext(const ServerContext&) = delete;
  constexpr ServerContext& operator=(const ServerContext&) = delete;

  constexpr ServerContext(ServerContext&&) = delete;
  constexpr ServerContext& operator=(ServerContext&&) = delete;

 private:
  constexpr ServerContext() = default;

  // Allow GlobalServerContextStub() to create a global instance.
  friend ServerContext& internal::GlobalServerContextStub();
};

namespace internal {

inline ServerContext& GlobalServerContextStub() {
  static ServerContext global_server_context_stub;
  return global_server_context_stub;
}

}  // namespace internal
}  // namespace pw::rpc
