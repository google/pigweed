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
      temp_channel.Send(Packet::ServerError(packet, Status::DataLoss()));
    }
    return false;
  }

  return true;
}

}  // namespace

Server::~Server() {
  // Since the writers remove themselves from the server in Finish(), remove the
  // first writer until no writers remain.
  while (!writers_.empty()) {
    writers_.front().Finish();
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
      temp_channel.Send(
          Packet::ServerError(packet, Status::ResourceExhausted()));
      return Status::Ok();  // OK since the packet was handled
    }
  }

  const auto [service, method] = FindMethod(packet);

  if (method == nullptr) {
    channel->Send(Packet::ServerError(packet, Status::NotFound()));
    return Status::Ok();
  }

  switch (packet.type()) {
    case PacketType::REQUEST: {
      internal::ServerCall call(
          static_cast<internal::Server&>(*this), *channel, *service, *method);
      method->Invoke(call, packet);
      break;
    }
    case PacketType::CLIENT_STREAM_END:
      // TODO(hepler): Support client streaming RPCs.
      break;
    case PacketType::CLIENT_ERROR:
      HandleClientError(packet);
      break;
    case PacketType::CANCEL_SERVER_STREAM:
      HandleCancelPacket(packet, *channel);
      break;
    default:
      channel->Send(Packet::ServerError(packet, Status::Unimplemented()));
      PW_LOG_WARN("Unable to handle packet of type %u",
                  unsigned(packet.type()));
  }
  return Status::Ok();
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

void Server::HandleCancelPacket(const Packet& packet,
                                internal::Channel& channel) {
  auto writer = std::find_if(writers_.begin(), writers_.end(), [&](auto& w) {
    return w.channel_id() == packet.channel_id() &&
           w.service_id() == packet.service_id() &&
           w.method_id() == packet.method_id();
  });

  if (writer == writers_.end()) {
    channel.Send(Packet::ServerError(packet, Status::FailedPrecondition()));
    PW_LOG_WARN("Received CANCEL packet for method that is not pending");
  } else {
    writer->Finish(Status::Cancelled());
  }
}

void Server::HandleClientError(const Packet& packet) {
  // A client error indicates that the client received a packet that it did not
  // expect. If the packet belongs to a streaming RPC, cancel the stream without
  // sending a final SERVER_STREAM_END packet.
  auto writer = std::find_if(writers_.begin(), writers_.end(), [&](auto& w) {
    return w.channel_id() == packet.channel_id() &&
           w.service_id() == packet.service_id() &&
           w.method_id() == packet.method_id();
  });

  if (writer != writers_.end()) {
    writer->Close();
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
