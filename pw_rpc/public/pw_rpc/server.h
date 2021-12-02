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
#include <span>
#include <tuple>

#include "pw_containers/intrusive_list.h"
#include "pw_rpc/channel.h"
#include "pw_rpc/internal/channel.h"
#include "pw_rpc/internal/endpoint.h"
#include "pw_rpc/internal/method.h"
#include "pw_rpc/internal/server_call.h"
#include "pw_rpc/service.h"
#include "pw_status/status.h"

namespace pw::rpc {

class Server : public internal::Endpoint {
 public:
  constexpr Server(std::span<Channel> channels) : Endpoint(channels) {}

  // Registers a service with the server. This should not be called directly
  // with a Service; instead, use a generated class which inherits from it.
  void RegisterService(Service& service) { services_.push_front(service); }

  // Processes an RPC packet. The packet may contain an RPC request or a control
  // packet, the result of which is processed in this function. Returns whether
  // the packet was able to be processed:
  //
  //   OK - The packet was processed by the server.
  //   DATA_LOSS - Failed to decode the packet.
  //   INVALID_ARGUMENT - The packet is intended for a client, not a server.
  //
  // ProcessPacket optionally accepts a ChannelOutput as a second argument. If
  // provided, the server will be able to dynamically assign channels as
  // requests come in instead of requiring channels to be known at compile time.
  Status ProcessPacket(ConstByteSpan packet_data) {
    return ProcessPacket(packet_data, nullptr);
  }
  Status ProcessPacket(ConstByteSpan packet_data, ChannelOutput& interface) {
    return ProcessPacket(packet_data, &interface);
  }

 private:
  friend class internal::Call;
  friend class ClientServer;

  Status ProcessPacket(ConstByteSpan packet_data, ChannelOutput* interface)
      PW_LOCKS_EXCLUDED(internal::rpc_lock());

  std::tuple<Service*, const internal::Method*> FindMethod(
      const internal::Packet& packet);

  void HandleClientStreamPacket(const internal::Packet& packet,
                                internal::Channel& channel,
                                internal::ServerCall* call) const
      PW_UNLOCK_FUNCTION(internal::rpc_lock());

  IntrusiveList<Service> services_;
};

}  // namespace pw::rpc
