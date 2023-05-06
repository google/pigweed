// Copyright 2023 The Pigweed Authors
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

#include "pw_rpc/client_server.h"
#include "pw_rpc/packet_meta.h"
#include "pw_rpc_transport/rpc_transport.h"
#include "pw_span/span.h"
#include "pw_status/status.h"

namespace pw::rpc {

// An RpcPacketProcessor implementation that uses an incoming RPC packet
// metadata to find its target service and sends the packet to that service for
// processing.
class ServiceRegistry : public RpcPacketProcessor {
 public:
  explicit ServiceRegistry(span<Channel> channels) : client_server_(channels) {}

  ClientServer& client_server() { return client_server_; }

  template <typename Service>
  typename Service::Client CreateClient(uint32_t channel_id) {
    return typename Service::Client(client_server_.client(), channel_id);
  }

  void RegisterService(Service& service) {
    client_server_.server().RegisterService(service);
  }

  Status ProcessRpcPacket(ConstByteSpan rpc_packet) override {
    PW_TRY_ASSIGN(const auto meta, PacketMeta::FromBuffer(rpc_packet));
    if (meta.destination_is_client()) {
      return client_server_.client().ProcessPacket(rpc_packet);
    }
    if (meta.destination_is_server()) {
      return client_server_.server().ProcessPacket(rpc_packet);
    }
    return Status::DataLoss();
  }

 private:
  ClientServer client_server_;
};

}  // namespace pw::rpc
