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

#include "pw_assert/assert.h"
#include "pw_rpc/internal/channel.h"

namespace pw::rpc {

class ServerContext;
class Service;

namespace internal {

class Method;
class Server;

// Collects information for an ongoing RPC being processed by the server.
// The Server creates a ServerCall object to represent a method invocation. The
// ServerCall is copied into a ServerWriter or ServerReader for streaming RPCs.
//
// ServerCall is a strictly internal class. ServerContext is the public
// interface to the internal::ServerCall.
class ServerCall {
 public:
  constexpr ServerCall()
      : server_(nullptr),
        channel_(nullptr),
        service_(nullptr),
        method_(nullptr) {}

  constexpr ServerCall(Server& server,
                       Channel& channel,
                       Service& service,
                       const internal::Method& method)
      : server_(&server),
        channel_(&channel),
        service_(&service),
        method_(&method) {}

  constexpr ServerCall(const ServerCall&) = default;
  constexpr ServerCall& operator=(const ServerCall&) = default;

  // Access the ServerContext for this call. Defined in pw_rpc/server_context.h.
  ServerContext& context();

  Server& server() const {
    PW_DCHECK_NOTNULL(server_);
    return *server_;
  }

  Channel& channel() const {
    PW_DCHECK_NOTNULL(channel_);
    return *channel_;
  }

  Service& service() const {
    PW_DCHECK_NOTNULL(service_);
    return *service_;
  }

  const internal::Method& method() const {
    PW_DCHECK_NOTNULL(method_);
    return *method_;
  }

 private:
  Server* server_;
  Channel* channel_;
  Service* service_;
  const internal::Method* method_;
};

}  // namespace internal
}  // namespace pw::rpc
