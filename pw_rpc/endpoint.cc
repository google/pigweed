// Copyright 2021 The Pigweed Authors
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

#include "pw_rpc/internal/endpoint.h"

#include "pw_log/log.h"

namespace pw::rpc::internal {

Endpoint::~Endpoint() {
  // Since the calls remove themselves from the Endpoint in
  // CloseAndSendResponse(), close responders until no responders remain.
  while (!calls_.empty()) {
    calls_.front().CloseAndSendResponse(OkStatus()).IgnoreError();
  }
}

Result<Packet> Endpoint::ProcessPacket(std::span<const std::byte> data,
                                       Packet::Destination destination,
                                       Call*& ongoing_call) {
  Result<Packet> result = Packet::FromBuffer(data);

  if (!result.ok()) {
    PW_LOG_WARN("Failed to decode pw_rpc packet");
    return Status::DataLoss();
  }

  Packet& packet = *result;

  if (packet.channel_id() == Channel::kUnassignedChannelId ||
      packet.service_id() == 0 || packet.method_id() == 0) {
    PW_LOG_WARN("Received malformed pw_rpc packet");
    return Status::DataLoss();
  }

  if (packet.destination() != destination) {
    return Status::InvalidArgument();
  }

  // Find an existing reader/writer for this RPC, if any.
  ongoing_call = FindCall(packet);

  return result;
}

Call* Endpoint::FindCall(const Packet& packet) {
  for (Call& call : calls_) {
    if (packet.channel_id() == call.channel_id() &&
        packet.service_id() == call.service_id() &&
        packet.method_id() == call.method_id()) {
      return &call;
    }
  }
  return nullptr;
}

Channel* Endpoint::GetInternalChannel(uint32_t id) const {
  for (Channel& c : channels_) {
    if (c.id() == id) {
      return &c;
    }
  }
  return nullptr;
}

Channel* Endpoint::AssignChannel(uint32_t id, ChannelOutput& interface) {
  internal::Channel* channel =
      GetInternalChannel(Channel::kUnassignedChannelId);
  if (channel == nullptr) {
    return nullptr;
  }

  *channel = Channel(id, &interface);
  return channel;
}

}  // namespace pw::rpc::internal
