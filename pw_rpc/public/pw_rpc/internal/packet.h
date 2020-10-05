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
#include <cstdint>
#include <span>

#include "pw_bytes/span.h"
#include "pw_rpc_protos/packet.pwpb.h"
#include "pw_status/status_with_size.h"

namespace pw::rpc::internal {

class Packet {
 public:
  static constexpr uint32_t kUnassignedId = 0;

  // Parses a packet from a protobuf message. Missing or malformed fields take
  // their default values.
  static Result<Packet> FromBuffer(ConstByteSpan data);

  // Creates an RPC packet with the channel, service, and method ID of the
  // provided packet.
  static constexpr Packet Response(const Packet& request,
                                   Status status = Status::Ok()) {
    return Packet(PacketType::RESPONSE,
                  request.channel_id(),
                  request.service_id(),
                  request.method_id(),
                  {},
                  status);
  }

  // Creates a SERVER_ERROR packet with the channel, service, and method ID of
  // the provided packet.
  static constexpr Packet ServerError(const Packet& packet, Status status) {
    return Packet(PacketType::SERVER_ERROR,
                  packet.channel_id(),
                  packet.service_id(),
                  packet.method_id(),
                  {},
                  status);
  }

  // Creates a CLIENT_ERROR packet with the channel, service, and method ID of
  // the provided packet.
  static constexpr Packet ClientError(const Packet& packet, Status status) {
    return Packet(PacketType::CLIENT_ERROR,
                  packet.channel_id(),
                  packet.service_id(),
                  packet.method_id(),
                  {},
                  status);
  }

  // Creates an empty packet.
  constexpr Packet()
      : Packet(PacketType{}, kUnassignedId, kUnassignedId, kUnassignedId) {}

  constexpr Packet(PacketType type,
                   uint32_t channel_id,
                   uint32_t service_id,
                   uint32_t method_id,
                   ConstByteSpan payload = {},
                   Status status = Status::Ok())
      : type_(type),
        channel_id_(channel_id),
        service_id_(service_id),
        method_id_(method_id),
        payload_(payload),
        status_(status) {}

  // Encodes the packet into its wire format. Returns the encoded size.
  Result<ConstByteSpan> Encode(ByteSpan buffer) const;

  // Determines the space required to encode the packet proto fields for a
  // response, excluding the payload. This may be used to split the buffer into
  // reserved space and available space for the payload.
  size_t MinEncodedSizeBytes() const;

  enum Destination : bool { kServer, kClient };

  constexpr Destination destination() const {
    return static_cast<int>(type_) % 2 == 0 ? kServer : kClient;
  }

  constexpr PacketType type() const { return type_; }
  constexpr uint32_t channel_id() const { return channel_id_; }
  constexpr uint32_t service_id() const { return service_id_; }
  constexpr uint32_t method_id() const { return method_id_; }
  constexpr const ConstByteSpan& payload() const { return payload_; }
  constexpr Status status() const { return status_; }

  constexpr void set_type(PacketType type) { type_ = type; }
  constexpr void set_channel_id(uint32_t channel_id) {
    channel_id_ = channel_id;
  }
  constexpr void set_service_id(uint32_t service_id) {
    service_id_ = service_id;
  }
  constexpr void set_method_id(uint32_t method_id) { method_id_ = method_id; }
  constexpr void set_payload(ConstByteSpan payload) { payload_ = payload; }
  constexpr void set_status(Status status) { status_ = status; }

 private:
  PacketType type_;
  uint32_t channel_id_;
  uint32_t service_id_;
  uint32_t method_id_;
  ConstByteSpan payload_;
  Status status_;
};

}  // namespace pw::rpc::internal
