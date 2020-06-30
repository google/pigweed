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
import tempfile
from pathlib import Path
from typing import List, Tuple

from pw_protobuf_compiler import python_protos
from pw_rpc import client, ids, packets
from pw_status import Status

TEST_PROTO_1 = b"""\
syntax = "proto3";

package pw.call.test1;

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

TEST_PROTO_2 = b"""\
syntax = "proto2";

package pw.call.test2;

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


class ClientTest(unittest.TestCase):
    """Tests the pw_rpc Python client."""
    def setUp(self):
        self._proto_dir = tempfile.TemporaryDirectory(prefix='proto_test')
        protos = []

        for i, contents in enumerate([TEST_PROTO_1, TEST_PROTO_2]):
            protos.append(Path(self._proto_dir.name, f'test_{i}.proto'))
            protos[-1].write_bytes(contents)

        self._protos = python_protos.Library(
            python_protos.compile_and_import(protos))

        self._impl = client.SimpleSynchronousClient()
        self._client = client.Client.from_modules(
            self._impl, [client.Channel(1, self._handle_request)],
            self._protos.modules())

        self._last_request: packets.RpcPacket = None
        self._next_packets: List[Tuple[bytes, bool]] = []

    def tearDown(self):
        self._proto_dir.cleanup()

    def _enqueue_response(self,
                          channel_id: int,
                          service_id: int,
                          method_id: int,
                          status: Status = Status.OK,
                          response=b'',
                          valid=True):
        if isinstance(response, bytes):
            payload = response
        else:
            payload = response.SerializeToString()

        self._next_packets.append(
            (packets.RpcPacket(channel_id=channel_id,
                               service_id=service_id,
                               method_id=method_id,
                               status=status.value,
                               payload=payload).SerializeToString(), valid))

    def _handle_request(self, data: bytes):
        self._last_request = packets.decode(data)

        self.assertTrue(self._next_packets)
        for packet, valid in self._next_packets:
            self.assertEqual(valid, self._client.process_packet(packet))

    def _last_payload(self, message_type):
        self.assertIsNotNone(self._last_request)
        message = message_type()
        message.ParseFromString(self._last_request.payload)
        return message

    def test_access_service_client_as_attribute_or_index(self):
        self.assertIs(
            self._client.channel(1).call.PublicService,
            self._client.channel(1).call['PublicService'])
        self.assertIs(
            self._client.channel(1).call.PublicService,
            self._client.channel(1).call[ids.calculate('PublicService')])

    def test_access_method_client_as_attribute_or_index(self):
        self.assertIs(
            self._client.channel(1).call.Alpha.Unary,
            self._client.channel(1).call['Alpha']['Unary'])
        self.assertIs(
            self._client.channel(1).call.Alpha.Unary,
            self._client.channel(1).call['Alpha'][ids.calculate('Unary')])

    def test_check_for_presence_of_services(self):
        self.assertIn('PublicService', self._client.channel(1).call)
        self.assertIn(ids.calculate('PublicService'),
                      self._client.channel(1).call)
        self.assertNotIn('NotAService', self._client.channel(1).call)
        self.assertNotIn(-1213, self._client.channel(1).call)

    def test_check_for_presence_of_methods(self):
        self.assertIn('SomeUnary', self._client.channel(1).call.PublicService)
        self.assertIn(ids.calculate('SomeUnary'),
                      self._client.channel(1).call.PublicService)

        self.assertNotIn('Unary', self._client.channel(1).call.PublicService)
        self.assertNotIn(12345, self._client.channel(1).call.PublicService)

    def test_invoke_unary_rpc(self):
        rpcs = self._client.channel(1).call
        method = rpcs.PublicService.SomeUnary.method

        for _ in range(3):
            self._enqueue_response(1, method.service.id, method.id,
                                   Status.ABORTED,
                                   method.response_type(payload='0_o'))

            status, response = rpcs.PublicService.SomeUnary(magic_number=6)

            self.assertEqual(
                6,
                self._last_payload(method.request_type).magic_number)

            self.assertIs(Status.ABORTED, status)
            self.assertEqual('0_o', response.payload)

    def test_ignore_bad_packets_with_pending_rpc(self):
        rpcs = self._client.channel(1).call
        method = rpcs.PublicService.SomeUnary.method
        service_id = method.service.id

        # Unknown channel
        self._enqueue_response(999, service_id, method.id, valid=False)
        # Bad service
        self._enqueue_response(1, 999, method.id, valid=False)
        # Bad method
        self._enqueue_response(1, service_id, 999, valid=False)
        # For RPC not pending (valid=True because the packet is processed)
        self._enqueue_response(1,
                               service_id,
                               rpcs.PublicService.SomeBidiStreaming.method.id,
                               valid=True)

        self._enqueue_response(1, service_id, method.id, valid=True)

        status, response = rpcs.PublicService.SomeUnary(magic_number=6)
        self.assertIs(Status.OK, status)
        self.assertEqual('', response.payload)

    def test_pass_none_if_payload_fails_to_decode(self):
        rpcs = self._client.channel(1).call
        method = rpcs.PublicService.SomeUnary.method

        self._enqueue_response(1,
                               method.service.id,
                               method.id,
                               Status.OK,
                               b'INVALID DATA!!!',
                               valid=True)

        status, response = rpcs.PublicService.SomeUnary(magic_number=6)
        self.assertIs(status, Status.OK)
        self.assertIsNone(response)

    def test_call_method_with_both_message_and_kwargs(self):
        req = self._client.services['Alpha'].methods['Unary'].request_type()

        with self.assertRaisesRegex(TypeError, r'either'):
            self._client.channel(1).call.Alpha.Unary(req, magic_number=1.0)

    def test_call_method_with_wrong_type(self):
        with self.assertRaisesRegex(TypeError, r'pw\.call\.test2\.Request'):
            self._client.channel(1).call.Alpha.Unary('This is str!')

    def test_call_method_with_incorrect_message_type(self):
        msg = self._protos.packages.pw.call.test1.AnotherMessage()
        with self.assertRaisesRegex(TypeError,
                                    r'pw\.call\.test1\.SomeMessage'):
            self._client.channel(1).call.PublicService.SomeUnary(msg)

    def test_process_packet_invalid_proto_data(self):
        self.assertFalse(self._client.process_packet(b'NOT a packet!'))

    def test_process_packet_unrecognized_channel(self):
        self.assertFalse(
            self._client.process_packet(
                packets.encode((123, 456, 789),
                               self._protos.packages.pw.call.test2.Request())))

    def test_process_packet_unrecognized_service(self):
        self.assertFalse(
            self._client.process_packet(
                packets.encode((1, 456, 789),
                               self._protos.packages.pw.call.test2.Request())))

    def test_process_packet_unrecognized_method(self):
        self.assertFalse(
            self._client.process_packet(
                packets.encode((1, next(iter(self._client.services)).id, 789),
                               self._protos.packages.pw.call.test2.Request())))


if __name__ == '__main__':
    unittest.main()
