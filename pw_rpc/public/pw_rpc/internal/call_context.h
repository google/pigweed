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

#include "pw_rpc/internal/channel.h"

namespace pw::rpc {

class Service;

namespace internal {

class Endpoint;
class Method;

// The Server creates a CallContext object to represent a method invocation. The
// CallContext is used to initialize a call object for the RPC.
class CallContext {
 public:
  constexpr CallContext(Endpoint& server,
                        Channel& channel,
                        Service& service,
                        const internal::Method& method)
      : server_(server),
        channel_(channel),
        service_(service),
        method_(method) {}

  constexpr Endpoint& server() const { return server_; }

  constexpr Channel& channel() const { return channel_; }

  constexpr Service& service() const { return service_; }

  constexpr const internal::Method& method() const { return method_; }

 private:
  Endpoint& server_;
  Channel& channel_;
  Service& service_;
  const internal::Method& method_;
};

}  // namespace internal
}  // namespace pw::rpc
