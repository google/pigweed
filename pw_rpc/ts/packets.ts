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

/** Functions for working with pw_rpc packets. */

import {Message} from 'google-protobuf';
import * as packetPb from 'packet_proto_tspb/packet_proto_tspb_pb/pw_rpc/internal/packet_pb'
import {Status} from 'pigweed/pw_status/ts/status';

export function decode(data: Uint8Array): packetPb.RpcPacket {
  return packetPb.RpcPacket.deserializeBinary(data);
}

export function decodePayload(packet: any, payloadType: any) {
  const payload = new payloadType();
  payload.deserializeBinary(packet);
  return payload;
}

export function forServer(packet: packetPb.RpcPacket): boolean {
  return packet.getType() % 2 == 0;
}

export function encodeClientError(
    packet: packetPb.RpcPacket, status: Status): Uint8Array {
  const errorPacket = new packetPb.RpcPacket();
  errorPacket.setType(packetPb.PacketType.CLIENT_ERROR);
  errorPacket.setChannelId(packet.getChannelId());
  errorPacket.setMethodId(packet.getMethodId());
  errorPacket.setServiceId(packet.getServiceId());
  errorPacket.setStatus(status);
  return errorPacket.serializeBinary();
}

export function encodeRequest(
    channelId: number, serviceId: number, methodId: number, request?: Message):
    Uint8Array {
  const payload: Uint8Array = (typeof request !== 'undefined') ?
      request.serializeBinary() :
      new Uint8Array();

  const packet = new packetPb.RpcPacket();
  packet.setType(packetPb.PacketType.REQUEST);
  packet.setChannelId(channelId);
  packet.setServiceId(serviceId);
  packet.setMethodId(methodId);
  packet.setPayload(payload);
  return packet.serializeBinary();
}

export function encodeResponse(
    channelId: number, serviceId: number, methodId: number, response: Message):
    Uint8Array {
  const packet = new packetPb.RpcPacket();
  packet.setType(packetPb.PacketType.RESPONSE);
  packet.setChannelId(channelId);
  packet.setServiceId(serviceId);
  packet.setMethodId(methodId);
  packet.setPayload(response.serializeBinary());
  return packet.serializeBinary();
}
