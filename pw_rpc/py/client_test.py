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
from pw_rpc import callback_client, client, packets
import pw_rpc.ids

TEST_PROTO_1 = """\
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

TEST_PROTO_2 = """\
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
    """Tests the pw_rpc Client independently of the ClientImpl."""
    def setUp(self):
        self._protos = python_protos.Library.from_strings(
            [TEST_PROTO_1, TEST_PROTO_2])

        self._client = client.Client.from_modules(callback_client.Impl(),
                                                  [client.Channel(1, None)],
                                                  self._protos.modules())

    def test_access_service_client_as_attribute_or_index(self):
        self.assertIs(
            self._client.channel(1).call.PublicService,
            self._client.channel(1).call['PublicService'])
        self.assertIs(
            self._client.channel(1).call.PublicService,
            self._client.channel(1).call[pw_rpc.ids.calculate(
                'PublicService')])

    def test_access_method_client_as_attribute_or_index(self):
        self.assertIs(
            self._client.channel(1).call.Alpha.Unary,
            self._client.channel(1).call['Alpha']['Unary'])
        self.assertIs(
            self._client.channel(1).call.Alpha.Unary,
            self._client.channel(1).call['Alpha'][pw_rpc.ids.calculate(
                'Unary')])

    def test_check_for_presence_of_services(self):
        self.assertIn('PublicService', self._client.channel(1).call)
        self.assertIn(pw_rpc.ids.calculate('PublicService'),
                      self._client.channel(1).call)
        self.assertNotIn('NotAService', self._client.channel(1).call)
        self.assertNotIn(-1213, self._client.channel(1).call)

    def test_check_for_presence_of_methods(self):
        self.assertIn('SomeUnary', self._client.channel(1).call.PublicService)
        self.assertIn(pw_rpc.ids.calculate('SomeUnary'),
                      self._client.channel(1).call.PublicService)

        self.assertNotIn('Unary', self._client.channel(1).call.PublicService)
        self.assertNotIn(12345, self._client.channel(1).call.PublicService)

    def test_method_get_request_with_both_message_and_kwargs(self):
        req = self._client.services['Alpha'].methods['Unary'].request_type()

        with self.assertRaisesRegex(TypeError, r'either'):
            self._client.services['Alpha'].methods['Unary'].get_request(
                req, {'magic_number': 1.0})

    def test_method_get_request_with_wrong_type(self):
        with self.assertRaisesRegex(TypeError, r'pw\.call\.test2\.Request'):
            self._client.services['Alpha'].methods['Unary'].get_request(
                'str!', {})

    def test_method_get_with_incorrect_message_type(self):
        msg = self._protos.packages.pw.call.test1.AnotherMessage()
        with self.assertRaisesRegex(TypeError,
                                    r'pw\.call\.test1\.SomeMessage'):
            self._client.services['PublicService'].methods[
                'SomeUnary'].get_request(msg, {})

    def test_process_packet_invalid_proto_data(self):
        self.assertFalse(self._client.process_packet(b'NOT a packet!'))

    def test_process_packet_unrecognized_channel(self):
        self.assertFalse(
            self._client.process_packet(
                packets.encode_request(
                    (123, 456, 789),
                    self._protos.packages.pw.call.test2.Request())))

    def test_process_packet_unrecognized_service(self):
        self.assertFalse(
            self._client.process_packet(
                packets.encode_request(
                    (1, 456, 789),
                    self._protos.packages.pw.call.test2.Request())))

    def test_process_packet_unrecognized_method(self):
        self.assertFalse(
            self._client.process_packet(
                packets.encode_request(
                    (1, next(iter(self._client.services)).id, 789),
                    self._protos.packages.pw.call.test2.Request())))


if __name__ == '__main__':
    unittest.main()
