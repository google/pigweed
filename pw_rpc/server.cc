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
using internal::PacketType;

void Server::ProcessPacket(span<const std::byte> data,
                           ChannelOutput& interface) {
  Packet packet = Packet::FromBuffer(data);
  if (packet.is_control()) {
    // TODO(frolv): Handle control packets.
    return;
  }

  if (packet.channel_id() == Channel::kUnassignedChannelId ||
      packet.service_id() == 0 || packet.method_id() == 0) {
    // Malformed packet; don't even try to process it.
    PW_LOG_ERROR("Received incomplete RPC packet on interface %u",
                 unsigned(interface.id()));
    return;
  }

  Packet response = Packet::Empty(PacketType::RPC);

  Channel* channel = FindChannel(packet.channel_id());
  if (channel == nullptr) {
    // If the requested channel doesn't exist, try to dynamically assign one.
    channel = AssignChannel(packet.channel_id(), interface);
    if (channel == nullptr) {
      // If a channel can't be assigned, send back a response indicating that
      // the server cannot process the request. The channel_id in the response
      // is not set, to allow clients to detect this error case.
      Channel temp_channel(packet.channel_id(), &interface);
      response.set_status(Status::RESOURCE_EXHAUSTED);
      SendResponse(temp_channel, response, temp_channel.AcquireBuffer());
      return;
    }
  }

  span<std::byte> response_buffer = channel->AcquireBuffer();
  span<std::byte> payload_buffer =
      ResponsePayloadUsableSpace(packet, response_buffer);

  response.set_channel_id(channel->id());

  Service* service = services_.Find(packet.service_id());
  if (service == nullptr) {
    // Couldn't find the requested service. Reply with a NOT_FOUND response
    // without the server_id field set.
    response.set_status(Status::NOT_FOUND);
    SendResponse(*channel, response, response_buffer);
    return;
  }

  service->ProcessPacket(packet, response, payload_buffer);
  SendResponse(*channel, response, response_buffer);
}

Channel* Server::FindChannel(uint32_t id) const {
  for (Channel& c : channels_) {
    if (c.id() == id) {
      return &c;
    }
  }
  return nullptr;
}

Channel* Server::AssignChannel(uint32_t id, ChannelOutput& interface) {
  Channel* channel = FindChannel(Channel::kUnassignedChannelId);
  if (channel == nullptr) {
    return nullptr;
  }

  *channel = Channel(id, &interface);
  return channel;
}

void Server::SendResponse(const Channel& channel,
                          const Packet& response,
                          span<std::byte> response_buffer) const {
  StatusWithSize sws = response.Encode(response_buffer);
  if (!sws.ok()) {
    // TODO(frolv): What should be done here?
    channel.SendAndReleaseBuffer(0);
    PW_LOG_ERROR("Failed to encode response packet to channel buffer");
    return;
  }

  channel.SendAndReleaseBuffer(sws.size());
}

span<std::byte> Server::ResponsePayloadUsableSpace(
    const Packet& request, span<std::byte> buffer) const {
  size_t reserved_size = 0;

  reserved_size += 1;  // channel_id key
  reserved_size += varint::EncodedSize(request.channel_id());
  reserved_size += 1;  // service_id key
  reserved_size += varint::EncodedSize(request.service_id());
  reserved_size += 1;  // method_id key
  reserved_size += varint::EncodedSize(request.method_id());

  // Packet type always takes two bytes to encode (varint key + varint enum).
  reserved_size += 2;

  // Status field always takes two bytes to encode (varint key + varint status).
  reserved_size += 2;

  return buffer.subspan(reserved_size);
}

}  // namespace pw::rpc
