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

#include "pw_span/span.h"
#include "pw_status/status.h"

namespace pw::rpc::internal {

// Forward-declare Packet instead of including it to avoid exposing the internal
// dependency on pw_protobuf.
class Packet;

class ServiceRegistry;

// Base class for all RPC services. This is not meant to be instantiated
// directly; use a generated subclass instead.
class Service {
 public:
  struct Method {
    uint32_t id;
  };

  Service(uint32_t id, span<const Method> methods)
      : id_(id), methods_(methods), next_(nullptr) {}

  uint32_t id() const { return id_; }

  // Handles an incoming packet and populates a response. Errors that occur
  // should be set within the response packet.
  void ProcessPacket(const internal::Packet& request,
                     internal::Packet& response,
                     span<std::byte> payload_buffer);

 private:
  friend class internal::ServiceRegistry;

  uint32_t id_;
  span<const Method> methods_;
  Service* next_;
};

}  // namespace pw::rpc::internal
