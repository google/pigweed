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

import os

from google.protobuf import message
from pw_protobuf_compiler import python_protos

packet_pb2 = python_protos.compile_and_import_file(
    os.path.join(__file__, '..', '..', '..', 'pw_rpc_protos', 'packet.proto'))

PacketType = packet_pb2.PacketType
RpcPacket = packet_pb2.RpcPacket

DecodeError = message.DecodeError
Message = message.Message


def decode(data: bytes):
    packet = RpcPacket()
    packet.MergeFromString(data)
    return packet


def decode_payload(packet, payload_type):
    payload = payload_type()
    payload.MergeFromString(packet.payload)
    return payload


def _ids(rpc: tuple) -> tuple:
    return tuple(item if isinstance(item, int) else item.id for item in rpc)


def encode_request(rpc: tuple, request: message.Message) -> bytes:
    channel, service, method = _ids(rpc)

    return RpcPacket(type=PacketType.RPC,
                     channel_id=channel,
                     service_id=service,
                     method_id=method,
                     payload=request.SerializeToString()).SerializeToString()


def encode_cancel(rpc: tuple) -> bytes:
    channel, service, method = _ids(rpc)
    return RpcPacket(type=PacketType.CANCEL,
                     channel_id=channel,
                     service_id=service,
                     method_id=method).SerializeToString()
