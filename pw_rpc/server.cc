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

#include "pw_rpc/server.h"

#include "pw_log/log.h"
#include "pw_rpc/internal/packet.h"

namespace pw::rpc {

using internal::Packet;

void Server::ProcessPacket(span<const std::byte> data,
                           ChannelOutput& interface) {
  Packet packet = Packet::FromBuffer(data);
  if (packet.is_control()) {
    // TODO(frolv): Handle control packets.
    return;
  }

  if (packet.service_id() == 0 || packet.method_id() == 0) {
    // Malformed packet; don't even try to process it.
    PW_LOG_ERROR("Received incomplete RPC packet on interface %u",
                 unsigned(interface.id()));
    return;
  }

  Channel* channel = FindChannel(packet.channel_id());
  if (channel == nullptr) {
    // TODO(frolv): Dynamically assign channel.
    return;
  }

  span<std::byte> response_buffer = channel->AcquireBuffer();

  Service* service = services_.Find(packet.service_id());
  if (service == nullptr) {
    // TODO(frolv): Send back a NOT_FOUND response.
    channel->SendAndReleaseBuffer(0);
    return;
  }

  Packet response = Packet::Empty();
  response.set_channel_id(channel->id());

  service->ProcessPacket(packet, response);

  StatusWithSize sws = response.Encode(response_buffer);
  if (!sws.ok()) {
    // TODO(frolv): What should be done here?
    channel->SendAndReleaseBuffer(0);
    return;
  }

  channel->SendAndReleaseBuffer(sws.size());
}

Channel* Server::FindChannel(uint32_t id) {
  for (Channel& c : channels_) {
    if (c.id() == id) {
      return &c;
    }
  }
  return nullptr;
}

}  // namespace pw::rpc
