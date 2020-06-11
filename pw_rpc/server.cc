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

#include <algorithm>

#include "pw_log/log.h"
#include "pw_rpc/internal/method.h"
#include "pw_rpc/internal/packet.h"
#include "pw_rpc/server_context.h"

namespace pw::rpc {

using std::byte;

using internal::Method;
using internal::Packet;
using internal::PacketType;

void Server::ProcessPacket(span<const byte> data, ChannelOutput& interface) {
  Packet packet = Packet::FromBuffer(data);
  if (packet.is_control()) {
    // TODO(frolv): Handle control packets.
    return;
  }

  if (packet.channel_id() == Channel::kUnassignedChannelId ||
      packet.service_id() == 0 || packet.method_id() == 0) {
    // Malformed packet; don't even try to process it.
    PW_LOG_ERROR("Received incomplete RPC packet on interface %s",
                 interface.name());
    return;
  }

  Packet response(PacketType::RPC);

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

  response.set_channel_id(channel->id());
  const span<byte> response_buffer = channel->AcquireBuffer();

  // Invoke the method with matching service and method IDs, if any.
  InvokeMethod(packet, *channel, response, response_buffer);
  SendResponse(*channel, response, response_buffer);
}

void Server::InvokeMethod(const Packet& request,
                          Channel& channel,
                          internal::Packet& response,
                          span<std::byte> buffer) {
  auto service = std::find_if(services_.begin(), services_.end(), [&](auto& s) {
    return s.id() == request.service_id();
  });

  if (service == services_.end()) {
    // Couldn't find the requested service. Reply with a NOT_FOUND response
    // without the service_id field set.
    response.set_status(Status::NOT_FOUND);
    return;
  }

  response.set_service_id(service->id());

  const internal::Method* method = service->FindMethod(request.method_id());

  if (method == nullptr) {
    // Couldn't find the requested method. Reply with a NOT_FOUND response
    // without the method_id field set.
    response.set_status(Status::NOT_FOUND);
    return;
  }

  response.set_method_id(method->id());

  span<byte> response_buffer = request.PayloadUsableSpace(buffer);

  internal::ServerCall call(*this, channel, *service, *method);
  StatusWithSize result =
      method->Invoke(call, request.payload(), response_buffer);

  response.set_status(result.status());
  response.set_payload(response_buffer.first(result.size()));
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
                          span<byte> response_buffer) const {
  StatusWithSize sws = response.Encode(response_buffer);
  if (!sws.ok()) {
    // TODO(frolv): What should be done here?
    channel.SendAndReleaseBuffer(0);
    PW_LOG_ERROR("Failed to encode response packet to channel buffer");
    return;
  }

  channel.SendAndReleaseBuffer(sws.size());
}

static_assert(std::is_base_of<internal::BaseMethod, internal::Method>(),
              "The Method implementation must be derived from "
              "pw::rpc::internal::BaseMethod");

}  // namespace pw::rpc
