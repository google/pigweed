// Copyright 2022 The Pigweed Authors
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

import { Message } from 'google-protobuf';
import {
  RpcPacket,
  PacketType,
} from 'pigweedjs/protos/pw_rpc/internal/packet_pb';
import { Status } from 'pigweedjs/pw_status';
import { MessageSerializer } from './descriptors';

// Channel, Service, Method, CallId
type idSet = [number, number, number, number];

export function decode(data: Uint8Array): RpcPacket {
  return RpcPacket.deserializeBinary(data);
}

function serializeMessage(
  message?: Message,
  serializers?: MessageSerializer,
): Uint8Array {
  let payload: Uint8Array;
  if (typeof message !== 'undefined') {
    if (serializers) {
      payload = serializers.serialize(message);
    } else {
      payload = (message as any)['serializeBinary']();
    }
  } else {
    payload = new Uint8Array(0);
  }
  return payload;
}

export function decodePayload(
  payload: Uint8Array,
  payloadType: any,
  serializer?: MessageSerializer,
): any {
  if (serializer) {
    return serializer.deserialize(payload);
  }
  return payloadType['deserializeBinary'](payload);
}

export function forServer(packet: RpcPacket): boolean {
  return packet.getType() % 2 == 0;
}

export function encodeClientError(
  packet: RpcPacket,
  status: Status,
): Uint8Array {
  const errorPacket = new RpcPacket();
  errorPacket.setType(PacketType.CLIENT_ERROR);
  errorPacket.setChannelId(packet.getChannelId());
  errorPacket.setMethodId(packet.getMethodId());
  errorPacket.setServiceId(packet.getServiceId());
  errorPacket.setCallId(packet.getCallId());
  errorPacket.setStatus(status);
  return errorPacket.serializeBinary();
}

export function encodeClientStream(
  ids: idSet,
  message: Message,
  customSerializer?: MessageSerializer,
): Uint8Array {
  const streamPacket = new RpcPacket();
  streamPacket.setType(PacketType.CLIENT_STREAM);
  streamPacket.setChannelId(ids[0]);
  streamPacket.setServiceId(ids[1]);
  streamPacket.setMethodId(ids[2]);
  streamPacket.setCallId(ids[3]);
  const msgSerialized = serializeMessage(message, customSerializer);
  streamPacket.setPayload(msgSerialized);
  return streamPacket.serializeBinary();
}

export function encodeClientStreamEnd(ids: idSet): Uint8Array {
  const streamEnd = new RpcPacket();
  streamEnd.setType(PacketType.CLIENT_REQUEST_COMPLETION);
  streamEnd.setChannelId(ids[0]);
  streamEnd.setServiceId(ids[1]);
  streamEnd.setMethodId(ids[2]);
  streamEnd.setCallId(ids[3]);
  return streamEnd.serializeBinary();
}

export function encodeRequest(
  ids: idSet,
  request?: Message,
  customSerializer?: MessageSerializer,
): Uint8Array {
  const payload = serializeMessage(request, customSerializer);
  const packet = new RpcPacket();
  packet.setType(PacketType.REQUEST);
  packet.setChannelId(ids[0]);
  packet.setServiceId(ids[1]);
  packet.setMethodId(ids[2]);
  packet.setCallId(ids[3]);
  packet.setPayload(payload);
  return packet.serializeBinary();
}

export function encodeResponse(ids: idSet, response: Message): Uint8Array {
  const packet = new RpcPacket();
  packet.setType(PacketType.RESPONSE);
  packet.setChannelId(ids[0]);
  packet.setServiceId(ids[1]);
  packet.setMethodId(ids[2]);
  packet.setCallId(ids[3]);
  const msgSerialized = (response as any)['serializeBinary']();
  packet.setPayload(msgSerialized);
  return packet.serializeBinary();
}

export function encodeCancel(ids: idSet): Uint8Array {
  const packet = new RpcPacket();
  packet.setType(PacketType.CLIENT_ERROR);
  packet.setStatus(Status.CANCELLED);
  packet.setChannelId(ids[0]);
  packet.setServiceId(ids[1]);
  packet.setMethodId(ids[2]);
  packet.setCallId(ids[3]);
  return packet.serializeBinary();
}
