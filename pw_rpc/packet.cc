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

Result<Packet> Packet::FromBuffer(ConstByteSpan data) {
  Packet packet;
  Status status;
  protobuf::Decoder decoder(data);

  while ((status = decoder.Next()).ok()) {
    RpcPacket::Fields field =
        static_cast<RpcPacket::Fields>(decoder.FieldNumber());

    switch (field) {
      case RpcPacket::Fields::TYPE: {
        uint32_t value;
        decoder.ReadUint32(&value);
        packet.set_type(static_cast<PacketType>(value));
        break;
      }

      case RpcPacket::Fields::CHANNEL_ID:
        decoder.ReadUint32(&packet.channel_id_);
        break;

      case RpcPacket::Fields::SERVICE_ID:
        decoder.ReadFixed32(&packet.service_id_);
        break;

      case RpcPacket::Fields::METHOD_ID:
        decoder.ReadFixed32(&packet.method_id_);
        break;

      case RpcPacket::Fields::PAYLOAD:
        decoder.ReadBytes(&packet.payload_);
        break;

      case RpcPacket::Fields::STATUS: {
        uint32_t value;
        decoder.ReadUint32(&value);
        packet.set_status(static_cast<Status::Code>(value));
        break;
      }
    }
  }

  if (status == Status::DataLoss()) {
    return status;
  }

  return packet;
}

Result<ConstByteSpan> Packet::Encode(ByteSpan buffer) const {
  pw::protobuf::NestedEncoder encoder(buffer);
  RpcPacket::Encoder rpc_packet(&encoder);

  // The payload is encoded first, as it may share the encode buffer.
  rpc_packet.WritePayload(payload_);

  rpc_packet.WriteType(type_);
  rpc_packet.WriteChannelId(channel_id_);
  rpc_packet.WriteServiceId(service_id_);
  rpc_packet.WriteMethodId(method_id_);
  rpc_packet.WriteStatus(status_.code());

  return encoder.Encode();
}

size_t Packet::MinEncodedSizeBytes() const {
  size_t reserved_size = 0;

  reserved_size += 1;  // channel_id key
  reserved_size += varint::EncodedSize(channel_id());
  reserved_size += 1 + sizeof(uint32_t);  // service_id key and fixed32
  reserved_size += 1 + sizeof(uint32_t);  // method_id key and fixed32

  // Packet type always takes two bytes to encode (varint key + varint enum).
  reserved_size += 2;

  // Status field always takes two bytes to encode (varint key + varint status).
  reserved_size += 2;

  // Payload field takes at least two bytes to encode (varint key + length).
  reserved_size += 2;

  return reserved_size;
}

}  // namespace pw::rpc::internal
