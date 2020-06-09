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

namespace pw::rpc {

// The ServerContext collects context for an RPC being invoked on a server. The
// ServerContext is passed into RPC functions and is user-facing.
//
// The ServerContext is a public-facing view of the internal::ServerCall class.
// It uses inheritance to avoid copying or creating an extra reference to the
// underlying ServerCall. Private inheritance prevents exposing the
// internal-facing ServerCall interface.
class ServerContext : private internal::ServerCall {
 public:
  // Returns the ID for the channel this RPC is using.
  uint32_t channel_id() const { return channel().id(); }

  constexpr ServerContext() = delete;

  constexpr ServerContext(const ServerContext&) = delete;
  constexpr ServerContext& operator=(const ServerContext&) = delete;

  constexpr ServerContext(ServerContext&&) = delete;
  constexpr ServerContext& operator=(ServerContext&&) = delete;

  friend class internal::ServerCall;  // Allow down-casting from ServerCall.
};

namespace internal {

inline ServerContext& ServerCall::context() {
  return static_cast<ServerContext&>(*this);
}

}  // namespace internal
}  // namespace pw::rpc
