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

#include "pw_rpc/client.h"

#include "pw_log/log.h"
#include "pw_rpc/internal/packet.h"

namespace pw::rpc {
namespace {

using internal::BaseClientCall;
using internal::Packet;
using internal::PacketType;

}  // namespace

Status Client::ProcessPacket(ConstByteSpan data) {
  Result<Packet> result = Packet::FromBuffer(data);
  if (!result.ok()) {
    PW_LOG_WARN("RPC client failed to decode incoming packet");
    return Status::DataLoss();
  }

  Packet& packet = result.value();

  if (packet.destination() != Packet::kClient) {
    return Status::InvalidArgument();
  }

  if (packet.channel_id() == Channel::kUnassignedChannelId ||
      packet.service_id() == 0 || packet.method_id() == 0) {
    PW_LOG_WARN("RPC client received a malformed packet");
    return Status::DataLoss();
  }

  auto call = std::find_if(calls_.begin(), calls_.end(), [&](auto& c) {
    return c.channel_id() == packet.channel_id() &&
           c.service_id() == packet.service_id() &&
           c.method_id() == packet.method_id();
  });

  auto channel = std::find_if(channels_.begin(), channels_.end(), [&](auto& c) {
    return c.id() == packet.channel_id();
  });

  if (channel == channels_.end()) {
    PW_LOG_WARN("RPC client received a packet for an unregistered channel");
    return Status::NotFound();
  }

  if (call == calls_.end()) {
    PW_LOG_WARN("RPC client received a packet for a request it did not make");
    // Don't send responses to errors to avoid infinite error cycles.
    if (packet.type() != PacketType::SERVER_ERROR) {
      channel->Send(Packet::ClientError(packet, Status::FailedPrecondition()))
          .IgnoreError();
    }
    return Status::NotFound();
  }

  switch (packet.type()) {
    case PacketType::RESPONSE:
    case PacketType::SERVER_ERROR:
      call->HandleResponse(packet);
      call->Unregister();
      break;
    case PacketType::SERVER_STREAM:
      call->HandleResponse(packet);
      break;
    default:
      return Status::Unimplemented();
  }

  return OkStatus();
}

Channel* Client::GetChannel(uint32_t channel_id) const {
  auto channel = std::find_if(channels_.begin(), channels_.end(), [&](auto& c) {
    return c.id() == channel_id;
  });

  if (channel == channels_.end()) {
    return nullptr;
  }

  return channel;
}

void Client::RegisterCall(BaseClientCall& call) {
  auto existing_call = std::find_if(calls_.begin(), calls_.end(), [&](auto& c) {
    return c.channel_id() == call.channel_id() &&
           c.service_id() == call.service_id() &&
           c.method_id() == call.method_id();
  });

  if (existing_call != calls_.end()) {
    PW_LOG_DEBUG(
        "RPC client called same method multiple times; canceling existing "
        "call.");

    // TODO(frolv): Invoke the existing_call's error callback once client calls
    // are refactored as generic Calls.
    existing_call->Unregister();
  }

  calls_.push_front(call);
}

}  // namespace pw::rpc
