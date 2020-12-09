#!/usr/bin/env python3
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
"""Tests creating pw_rpc client."""

import unittest

from pw_protobuf_compiler import python_protos
from pw_status import Status

from pw_rpc import callback_client, client, packets
from pw_rpc.packet_pb2 import RpcPacket
import pw_rpc.ids

TEST_PROTO_1 = """\
syntax = "proto3";

package pw.test1;

message SomeMessage {
  uint32 magic_number = 1;
}

message AnotherMessage {
  enum Result {
    FAILED = 0;
    FAILED_MISERABLY = 1;
    I_DONT_WANT_TO_TALK_ABOUT_IT = 2;
  }

  Result result = 1;
  string payload = 2;
}

service PublicService {
  rpc SomeUnary(SomeMessage) returns (AnotherMessage) {}
  rpc SomeServerStreaming(SomeMessage) returns (stream AnotherMessage) {}
  rpc SomeClientStreaming(stream SomeMessage) returns (AnotherMessage) {}
  rpc SomeBidiStreaming(stream SomeMessage) returns (stream AnotherMessage) {}
}
"""

TEST_PROTO_2 = """\
syntax = "proto2";

package pw.test2;

message Request {
  optional float magic_number = 1;
}

message Response {
}

service Alpha {
  rpc Unary(Request) returns (Response) {}
}

service Bravo {
  rpc BidiStreaming(stream Request) returns (stream Response) {}
}
"""


def _test_setup(output=None):
    protos = python_protos.Library.from_strings([TEST_PROTO_1, TEST_PROTO_2])
    return protos, client.Client.from_modules(callback_client.Impl(),
                                              [client.Channel(1, output)],
                                              protos.modules())


class ChannelClientTest(unittest.TestCase):
    """Tests the ChannelClient."""
    def setUp(self):
        self._channel_client = _test_setup()[1].channel(1)

    def test_access_service_client_as_attribute_or_index(self):
        self.assertIs(self._channel_client.rpcs.pw.test1.PublicService,
                      self._channel_client.rpcs['pw.test1.PublicService'])
        self.assertIs(
            self._channel_client.rpcs.pw.test1.PublicService,
            self._channel_client.rpcs[pw_rpc.ids.calculate(
                'pw.test1.PublicService')])

    def test_access_method_client_as_attribute_or_index(self):
        self.assertIs(self._channel_client.rpcs.pw.test2.Alpha.Unary,
                      self._channel_client.rpcs['pw.test2.Alpha']['Unary'])
        self.assertIs(
            self._channel_client.rpcs.pw.test2.Alpha.Unary,
            self._channel_client.rpcs['pw.test2.Alpha'][pw_rpc.ids.calculate(
                'Unary')])

    def test_service_name(self):
        self.assertEqual(
            self._channel_client.rpcs.pw.test2.Alpha.Unary.service.name,
            'Alpha')
        self.assertEqual(
            self._channel_client.rpcs.pw.test2.Alpha.Unary.service.full_name,
            'pw.test2.Alpha')

    def test_method_name(self):
        self.assertEqual(
            self._channel_client.rpcs.pw.test2.Alpha.Unary.method.name,
            'Unary')
        self.assertEqual(
            self._channel_client.rpcs.pw.test2.Alpha.Unary.method.full_name,
            'pw.test2.Alpha.Unary')

    def test_iterate_over_all_methods(self):
        channel_client = self._channel_client
        all_methods = {
            channel_client.rpcs.pw.test1.PublicService.SomeUnary,
            channel_client.rpcs.pw.test1.PublicService.SomeServerStreaming,
            channel_client.rpcs.pw.test1.PublicService.SomeClientStreaming,
            channel_client.rpcs.pw.test1.PublicService.SomeBidiStreaming,
            channel_client.rpcs.pw.test2.Alpha.Unary,
            channel_client.rpcs.pw.test2.Bravo.BidiStreaming,
        }
        self.assertEqual(set(channel_client.methods()), all_methods)

    def test_check_for_presence_of_services(self):
        self.assertIn('pw.test1.PublicService', self._channel_client.rpcs)
        self.assertIn(pw_rpc.ids.calculate('pw.test1.PublicService'),
                      self._channel_client.rpcs)

    def test_check_for_presence_of_missing_services(self):
        self.assertNotIn('PublicService', self._channel_client.rpcs)
        self.assertNotIn('NotAService', self._channel_client.rpcs)
        self.assertNotIn(-1213, self._channel_client.rpcs)

    def test_check_for_presence_of_methods(self):
        service = self._channel_client.rpcs.pw.test1.PublicService
        self.assertIn('SomeUnary', service)
        self.assertIn(pw_rpc.ids.calculate('SomeUnary'), service)

    def test_check_for_presence_of_missing_methods(self):
        service = self._channel_client.rpcs.pw.test1.PublicService
        self.assertNotIn('Some', service)
        self.assertNotIn('Unary', service)
        self.assertNotIn(12345, service)

    def test_method_fully_qualified_name(self):
        self.assertIs(self._channel_client.method('pw.test2.Alpha/Unary'),
                      self._channel_client.rpcs.pw.test2.Alpha.Unary)
        self.assertIs(self._channel_client.method('pw.test2.Alpha.Unary'),
                      self._channel_client.rpcs.pw.test2.Alpha.Unary)


class ClientTest(unittest.TestCase):
    """Tests the pw_rpc Client independently of the ClientImpl."""
    def setUp(self):
        self._last_packet_sent_bytes = None
        self._protos, self._client = _test_setup(self._save_packet)

    def _save_packet(self, packet):
        self._last_packet_sent_bytes = packet

    def _last_packet_sent(self):
        packet = RpcPacket()
        self.assertIsNotNone(self._last_packet_sent_bytes)
        packet.MergeFromString(self._last_packet_sent_bytes)
        return packet

    def test_all_methods(self):
        services = self._client.services

        all_methods = {
            services['pw.test1.PublicService'].methods['SomeUnary'],
            services['pw.test1.PublicService'].methods['SomeServerStreaming'],
            services['pw.test1.PublicService'].methods['SomeClientStreaming'],
            services['pw.test1.PublicService'].methods['SomeBidiStreaming'],
            services['pw.test2.Alpha'].methods['Unary'],
            services['pw.test2.Bravo'].methods['BidiStreaming'],
        }
        self.assertEqual(set(self._client.methods()), all_methods)

    def test_method_present(self):
        self.assertIs(
            self._client.method('pw.test1.PublicService.SomeUnary'), self.
            _client.services['pw.test1.PublicService'].methods['SomeUnary'])
        self.assertIs(
            self._client.method('pw.test1.PublicService/SomeUnary'), self.
            _client.services['pw.test1.PublicService'].methods['SomeUnary'])

    def test_method_invalid_format(self):
        with self.assertRaises(ValueError):
            self._client.method('SomeUnary')

    def test_method_not_present(self):
        with self.assertRaises(KeyError):
            self._client.method('pw.test1.PublicService/ThisIsNotGood')

        with self.assertRaises(KeyError):
            self._client.method('nothing.Good')

    def test_process_packet_invalid_proto_data(self):
        self.assertIs(self._client.process_packet(b'NOT a packet!'),
                      Status.DATA_LOSS)

    def test_process_packet_not_for_client(self):
        self.assertIs(
            self._client.process_packet(
                RpcPacket(
                    type=packets.PacketType.REQUEST).SerializeToString()),
            Status.INVALID_ARGUMENT)

    def test_process_packet_unrecognized_channel(self):
        self.assertIs(
            self._client.process_packet(
                packets.encode_response(
                    (123, 456, 789),
                    self._protos.packages.pw.test2.Request())),
            Status.NOT_FOUND)

    def test_process_packet_unrecognized_service(self):
        self.assertIs(
            self._client.process_packet(
                packets.encode_response(
                    (1, 456, 789), self._protos.packages.pw.test2.Request())),
            Status.OK)

        self.assertEqual(
            self._last_packet_sent(),
            RpcPacket(type=packets.PacketType.CLIENT_ERROR,
                      channel_id=1,
                      service_id=456,
                      method_id=789,
                      status=Status.NOT_FOUND.value))

    def test_process_packet_unrecognized_method(self):
        service = next(iter(self._client.services))

        self.assertIs(
            self._client.process_packet(
                packets.encode_response(
                    (1, service.id, 789),
                    self._protos.packages.pw.test2.Request())), Status.OK)

        self.assertEqual(
            self._last_packet_sent(),
            RpcPacket(type=packets.PacketType.CLIENT_ERROR,
                      channel_id=1,
                      service_id=service.id,
                      method_id=789,
                      status=Status.NOT_FOUND.value))

    def test_process_packet_non_pending_method(self):
        service = next(iter(self._client.services))
        method = next(iter(service.methods))

        self.assertIs(
            self._client.process_packet(
                packets.encode_response(
                    (1, service.id, method.id),
                    self._protos.packages.pw.test2.Request())), Status.OK)

        self.assertEqual(
            self._last_packet_sent(),
            RpcPacket(type=packets.PacketType.CLIENT_ERROR,
                      channel_id=1,
                      service_id=service.id,
                      method_id=method.id,
                      status=Status.FAILED_PRECONDITION.value))


if __name__ == '__main__':
    unittest.main()
