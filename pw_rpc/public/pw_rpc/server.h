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

#include "pw_rpc/channel.h"
#include "pw_rpc/internal/service_registry.h"

namespace pw::rpc {

class Server {
 public:
  constexpr Server(span<Channel> channels) : channels_(channels) {}

  Server(const Server& other) = delete;
  Server& operator=(const Server& other) = delete;

  // Registers a service with the server. This should not be called directly
  // with an internal::Service; instead, use a generated class which inherits
  // from it.
  void RegisterService(internal::Service& service) {
    services_.Register(service);
  }

  void ProcessPacket(span<const std::byte> packet, ChannelOutput& interface);

  constexpr size_t channel_count() const { return channels_.size(); }

 private:
  using Service = internal::Service;
  using ServiceRegistry = internal::ServiceRegistry;

  void SendResponse(const Channel& channel,
                    const internal::Packet& response,
                    span<std::byte> response_buffer) const;

  // Determines the space required to encode the packet proto fields for a
  // response, and splits the buffer into reserved space and available space for
  // the payload. Returns a subspan of the payload space.
  span<std::byte> ResponsePayloadUsableSpace(const internal::Packet& request,
                                             span<std::byte> buffer) const;

  Channel* FindChannel(uint32_t id) const;
  Channel* AssignChannel(uint32_t id, ChannelOutput& interface);

  span<Channel> channels_;
  ServiceRegistry services_;
};

}  // namespace pw::rpc
