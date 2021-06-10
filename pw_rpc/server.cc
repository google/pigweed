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
#include "pw_rpc/internal/packet.h"
#include "pw_rpc/internal/server.h"
#include "pw_rpc/server_context.h"

namespace pw::rpc {
namespace {

using std::byte;

using internal::Packet;
using internal::PacketType;

bool DecodePacket(ChannelOutput& interface,
                  std::span<const byte> data,
                  Packet& packet) {
  Result<Packet> result = Packet::FromBuffer(data);
  if (!result.ok()) {
    PW_LOG_WARN("Failed to decode packet on interface %s", interface.name());
    return false;
  }

  packet = result.value();

  // If the packet is malformed, don't try to process it.
  if (packet.channel_id() == Channel::kUnassignedChannelId ||
      packet.service_id() == 0 || packet.method_id() == 0) {
    PW_LOG_WARN("Received incomplete packet on interface %s", interface.name());

    // Only send an ERROR response if a valid channel ID was provided.
    if (packet.channel_id() != Channel::kUnassignedChannelId) {
      internal::Channel temp_channel(packet.channel_id(), &interface);
      temp_channel.Send(Packet::ServerError(packet, Status::DataLoss()))
          .IgnoreError();  // TODO(pwbug/387): Handle Status properly
    }
    return false;
  }

  return true;
}

}  // namespace

Server::~Server() {
  // Since the responders remove themselves from the server in
  // CloseAndSendResponse(), close responders until no responders remain.
  while (!responders_.empty()) {
    responders_.front()
        .CloseAndSendResponse(OkStatus())
        .IgnoreError();  // TODO(pwbug/387): Handle Status properly
  }
}

Status Server::ProcessPacket(std::span<const byte> data,
                             ChannelOutput& interface) {
  Packet packet;
  if (!DecodePacket(interface, data, packet)) {
    return Status::DataLoss();
  }

  if (packet.destination() != Packet::kServer) {
    return Status::InvalidArgument();
  }

  internal::Channel* channel = FindChannel(packet.channel_id());
  if (channel == nullptr) {
    // If the requested channel doesn't exist, try to dynamically assign one.
    channel = AssignChannel(packet.channel_id(), interface);
    if (channel == nullptr) {
      // If a channel can't be assigned, send a RESOURCE_EXHAUSTED error.
      internal::Channel temp_channel(packet.channel_id(), &interface);
      temp_channel
          .Send(Packet::ServerError(packet, Status::ResourceExhausted()))
          .IgnoreError();  // TODO(pwbug/387): Handle Status properly
      return OkStatus();   // OK since the packet was handled
    }
  }

  const auto [service, method] = FindMethod(packet);

  if (method == nullptr) {
    channel->Send(Packet::ServerError(packet, Status::NotFound()))
        .IgnoreError();  // TODO(pwbug/387): Handle Status properly
    return OkStatus();
  }

  // Find an existing reader/writer for this RPC, if any.
  auto responder =
      std::find_if(responders_.begin(), responders_.end(), [&](auto& w) {
        return packet.channel_id() == w.channel_id() &&
               packet.service_id() == w.service_id() &&
               packet.method_id() == w.method_id();
      });

  switch (packet.type()) {
    case PacketType::REQUEST: {
      // If the REQUEST is for an ongoing RPC, cancel it, then call it again.
      if (responder != responders_.end()) {
        responder->HandleError(Status::Cancelled());
      }

      internal::ServerCall call(
          static_cast<internal::Server&>(*this), *channel, *service, *method);
      method->Invoke(call, packet);
      break;
    }
    case PacketType::CLIENT_STREAM:
      HandleClientStreamPacket(packet, *channel, responder);
      break;
    case PacketType::CLIENT_ERROR:
      if (responder != responders_.end()) {
        responder->HandleError(packet.status());
      }
      break;
    case PacketType::CANCEL:
      HandleCancelPacket(packet, *channel, responder);
      break;
    case PacketType::CLIENT_STREAM_END:
      HandleClientStreamPacket(packet, *channel, responder);
      break;
    default:
      channel->Send(Packet::ServerError(packet, Status::Unimplemented()))
          .IgnoreError();  // TODO(pwbug/387): Handle Status properly
      PW_LOG_WARN("Unable to handle packet of type %u",
                  unsigned(packet.type()));
  }
  return OkStatus();
}

std::tuple<Service*, const internal::Method*> Server::FindMethod(
    const internal::Packet& packet) {
  // Packets always include service and method IDs.
  auto service = std::find_if(services_.begin(), services_.end(), [&](auto& s) {
    return s.id() == packet.service_id();
  });

  if (service == services_.end()) {
    return {};
  }

  return {&(*service), service->FindMethod(packet.method_id())};
}

void Server::HandleClientStreamPacket(const internal::Packet& packet,
                                      internal::Channel& channel,
                                      ResponderIterator responder) const {
  if (responder == responders_.end()) {
    PW_LOG_DEBUG(
        "Received client stream packet for method that is not pending");
    channel.Send(Packet::ServerError(packet, Status::FailedPrecondition()))
        .IgnoreError();  // TODO(pwbug/387): Handle Status properly
    return;
  }

  if (!responder->has_client_stream()) {
    channel.Send(Packet::ServerError(packet, Status::InvalidArgument()))
        .IgnoreError();  // TODO(pwbug/387): Handle Status properly
    return;
  }

  if (!responder->client_stream_open()) {
    channel.Send(Packet::ServerError(packet, Status::FailedPrecondition()))
        .IgnoreError();  // TODO(pwbug/387): Handle Status properly
    return;
  }

  if (packet.type() == PacketType::CLIENT_STREAM) {
    responder->HandleClientStream(packet.payload());
  } else {  // Handle PacketType::CLIENT_STREAM_END.
    responder->EndClientStream();
  }
}

void Server::HandleCancelPacket(const Packet& packet,
                                internal::Channel& channel,
                                ResponderIterator responder) const {
  if (responder == responders_.end()) {
    channel.Send(Packet::ServerError(packet, Status::FailedPrecondition()))
        .IgnoreError();  // TODO(pwbug/387): Handle Status properly
    PW_LOG_DEBUG("Received CANCEL packet for method that is not pending");
  } else {
    responder->HandleError(Status::Cancelled());
  }
}

internal::Channel* Server::FindChannel(uint32_t id) const {
  for (internal::Channel& c : channels_) {
    if (c.id() == id) {
      return &c;
    }
  }
  return nullptr;
}

internal::Channel* Server::AssignChannel(uint32_t id,
                                         ChannelOutput& interface) {
  internal::Channel* channel = FindChannel(Channel::kUnassignedChannelId);
  if (channel == nullptr) {
    return nullptr;
  }

  *channel = internal::Channel(id, &interface);
  return channel;
}

}  // namespace pw::rpc
