# Copyright 2020 The Pigweed Authors
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.
"""Functions for working with pw_rpc packets."""

import dataclasses
from typing import Optional

from google.protobuf import message
from pw_status import Status

from pw_rpc.internal import packet_pb2


def decode(data: bytes) -> packet_pb2.RpcPacket:
    packet = packet_pb2.RpcPacket()
    packet.MergeFromString(data)
    return packet


def decode_payload(packet, payload_type):
    payload = payload_type()
    payload.MergeFromString(packet.payload)
    return payload


@dataclasses.dataclass(eq=True, frozen=True)
class RpcIds:
    """Integer IDs that uniquely identify a remote procedure call."""

    channel_id: int
    service_id: int
    method_id: int
    call_id: int


def encode_request(rpc: RpcIds, request: Optional[message.Message]) -> bytes:
    payload = request.SerializeToString() if request is not None else bytes()

    return packet_pb2.RpcPacket(
        type=packet_pb2.PacketType.REQUEST,
        channel_id=rpc.channel_id,
        service_id=rpc.service_id,
        method_id=rpc.method_id,
        call_id=rpc.call_id,
        payload=payload,
    ).SerializeToString()


def encode_response(rpc: RpcIds, response: message.Message) -> bytes:
    return packet_pb2.RpcPacket(
        type=packet_pb2.PacketType.RESPONSE,
        channel_id=rpc.channel_id,
        service_id=rpc.service_id,
        method_id=rpc.method_id,
        call_id=rpc.call_id,
        payload=response.SerializeToString(),
    ).SerializeToString()


def encode_client_stream(rpc: RpcIds, request: message.Message) -> bytes:
    return packet_pb2.RpcPacket(
        type=packet_pb2.PacketType.CLIENT_STREAM,
        channel_id=rpc.channel_id,
        service_id=rpc.service_id,
        method_id=rpc.method_id,
        call_id=rpc.call_id,
        payload=request.SerializeToString(),
    ).SerializeToString()


def encode_client_error(packet: packet_pb2.RpcPacket, status: Status) -> bytes:
    return packet_pb2.RpcPacket(
        type=packet_pb2.PacketType.CLIENT_ERROR,
        channel_id=packet.channel_id,
        service_id=packet.service_id,
        method_id=packet.method_id,
        call_id=packet.call_id,
        status=status.value,
    ).SerializeToString()


def encode_cancel(rpc: RpcIds) -> bytes:
    return packet_pb2.RpcPacket(
        type=packet_pb2.PacketType.CLIENT_ERROR,
        status=Status.CANCELLED.value,
        channel_id=rpc.channel_id,
        service_id=rpc.service_id,
        method_id=rpc.method_id,
        call_id=rpc.call_id,
    ).SerializeToString()


def encode_client_stream_end(rpc: RpcIds) -> bytes:
    return packet_pb2.RpcPacket(
        type=packet_pb2.PacketType.CLIENT_REQUEST_COMPLETION,
        channel_id=rpc.channel_id,
        service_id=rpc.service_id,
        method_id=rpc.method_id,
        call_id=rpc.call_id,
    ).SerializeToString()


def for_server(packet: packet_pb2.RpcPacket) -> bool:
    return packet.type % 2 == 0
