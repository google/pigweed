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

#include "pw_rpc/internal/packet.h"

#include "pw_protobuf/decoder.h"

namespace pw::rpc::internal {

using std::byte;

Packet Packet::FromBuffer(span<const byte> data) {
  PacketType type = PacketType::RPC;
  uint32_t channel_id = 0;
  uint32_t service_id = 0;
  uint32_t method_id = 0;
  span<const byte> payload;
  Status status;

  uint32_t value;
  protobuf::Decoder decoder(data);

  while (decoder.Next().ok()) {
    RpcPacket::Fields field =
        static_cast<RpcPacket::Fields>(decoder.FieldNumber());
    uint32_t proto_value = 0;

    switch (field) {
      case RpcPacket::Fields::TYPE:
        decoder.ReadUint32(&proto_value);
        type = static_cast<PacketType>(proto_value);
        break;

      case RpcPacket::Fields::CHANNEL_ID:
        decoder.ReadUint32(&channel_id);
        break;

      case RpcPacket::Fields::SERVICE_ID:
        decoder.ReadUint32(&service_id);
        break;

      case RpcPacket::Fields::METHOD_ID:
        decoder.ReadUint32(&method_id);
        break;

      case RpcPacket::Fields::PAYLOAD:
        decoder.ReadBytes(&payload);
        break;

      case RpcPacket::Fields::STATUS:
        decoder.ReadUint32(&value);
        status = static_cast<Status::Code>(value);
        break;
    }
  }

  return Packet(type, channel_id, service_id, method_id, payload, status);
}

StatusWithSize Packet::Encode(span<byte> buffer) const {
  pw::protobuf::NestedEncoder encoder(buffer);
  RpcPacket::Encoder rpc_packet(&encoder);

  // The payload is encoded first, as it may share the encode buffer.
  rpc_packet.WritePayload(payload_);

  rpc_packet.WriteType(type_);
  rpc_packet.WriteChannelId(channel_id_);
  rpc_packet.WriteServiceId(service_id_);
  rpc_packet.WriteMethodId(method_id_);
  rpc_packet.WriteStatus(status_);

  span<const byte> proto;
  if (Status status = encoder.Encode(&proto); !status.ok()) {
    return StatusWithSize(status, 0);
  }

  return StatusWithSize(proto.size());
}

span<byte> Packet::PayloadUsableSpace(span<byte> buffer) const {
  size_t reserved_size = 0;

  reserved_size += 1;  // channel_id key
  reserved_size += varint::EncodedSize(channel_id());
  reserved_size += 1;  // service_id key
  reserved_size += varint::EncodedSize(service_id());
  reserved_size += 1;  // method_id key
  reserved_size += varint::EncodedSize(method_id());

  // Packet type always takes two bytes to encode (varint key + varint enum).
  reserved_size += 2;

  // Status field always takes two bytes to encode (varint key + varint status).
  reserved_size += 2;

  // Payload field takes at least two bytes to encode (varint key + length).
  reserved_size += 2;

  return reserved_size <= buffer.size() ? buffer.subspan(reserved_size)
                                        : span<byte>();
}

}  // namespace pw::rpc::internal
