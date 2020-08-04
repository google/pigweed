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

#include "pw_bytes/span.h"
#include "pw_rpc/internal/base_client_call.h"
#include "pw_rpc/internal/channel.h"

namespace pw::rpc {

class Client {
 public:
  // Creates a client that uses a set of RPC channels. Channels can be shared
  // between a client and a server, but not between multiple clients.
  constexpr Client(std::span<Channel> channels)
      : channels_(static_cast<internal::Channel*>(channels.data()),
                  channels.size()) {
    for (Channel& channel : channels_) {
      channel.set_client(this);
    };
  }

  // Processes an incoming RPC packet. The packet may be an RPC response or a
  // control packet, the result of which is processed in this function. Returns
  // whether the packet was able to be processed:
  //
  //   OK - The packet was processed by the client.
  //   DATA_LOSS - Failed to decode the packet.
  //   INVALID_ARGUMENT - The packet is intended for a server, not a client.
  //   NOT_FOUND - The packet belongs to an unknown RPC call.
  //   UNIMPLEMENTED - Received a type of packet that the client doesn't know
  //       how to handle.
  //
  Status ProcessPacket(ConstByteSpan data);

  size_t active_calls() const { return calls_.size(); }

 private:
  friend class internal::BaseClientCall;

  Status RegisterCall(internal::BaseClientCall& call);
  void RemoveCall(const internal::BaseClientCall& call) { calls_.remove(call); }

  std::span<internal::Channel> channels_;
  IntrusiveList<internal::BaseClientCall> calls_;
};

}  // namespace pw::rpc
