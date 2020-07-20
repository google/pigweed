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
            self._client.channel(1).rpcs.pw.test1.PublicService,
            self._client.channel(1).rpcs['pw.test1.PublicService'])
        self.assertIs(
            self._client.channel(1).rpcs.pw.test1.PublicService,
            self._client.channel(1).rpcs[pw_rpc.ids.calculate(
                'pw.test1.PublicService')])

    def test_access_method_client_as_attribute_or_index(self):
        self.assertIs(
            self._client.channel(1).rpcs.pw.test2.Alpha.Unary,
            self._client.channel(1).rpcs['pw.test2.Alpha']['Unary'])
        self.assertIs(
            self._client.channel(1).rpcs.pw.test2.Alpha.Unary,
            self._client.channel(1).rpcs['pw.test2.Alpha'][
                pw_rpc.ids.calculate('Unary')])

    def test_service_name(self):
        self.assertEqual(
            self._client.channel(1).rpcs.pw.test2.Alpha.Unary.service.name,
            'Alpha')
        self.assertEqual(
            self._client.channel(
                1).rpcs.pw.test2.Alpha.Unary.service.full_name,
            'pw.test2.Alpha')

    def test_method_name(self):
        self.assertEqual(
            self._client.channel(1).rpcs.pw.test2.Alpha.Unary.method.name,
            'Unary')
        self.assertEqual(
            self._client.channel(1).rpcs.pw.test2.Alpha.Unary.method.full_name,
            'pw.test2.Alpha/Unary')

    def test_check_for_presence_of_services(self):
        self.assertIn('pw.test1.PublicService', self._client.channel(1).rpcs)
        self.assertIn(pw_rpc.ids.calculate('pw.test1.PublicService'),
                      self._client.channel(1).rpcs)

    def test_check_for_presence_of_missing_services(self):
        self.assertNotIn('PublicService', self._client.channel(1).rpcs)
        self.assertNotIn('NotAService', self._client.channel(1).rpcs)
        self.assertNotIn(-1213, self._client.channel(1).rpcs)

    def test_check_for_presence_of_methods(self):
        service = self._client.channel(1).rpcs.pw.test1.PublicService
        self.assertIn('SomeUnary', service)
        self.assertIn(pw_rpc.ids.calculate('SomeUnary'), service)

    def test_check_for_presence_of_missing_methods(self):
        service = self._client.channel(1).rpcs.pw.test1.PublicService
        self.assertNotIn('Some', service)
        self.assertNotIn('Unary', service)
        self.assertNotIn(12345, service)

    def test_method_get_request_with_both_message_and_kwargs(self):
        method = self._client.services['pw.test2.Alpha'].methods['Unary']

        with self.assertRaisesRegex(TypeError, r'either'):
            method.get_request(method.request_type(), {'magic_number': 1.0})

    def test_method_get_request_with_wrong_type(self):
        method = self._client.services['pw.test2.Alpha'].methods['Unary']
        with self.assertRaisesRegex(TypeError, r'pw\.test2\.Request'):
            method.get_request('a str!', {})

    def test_method_get_with_incorrect_message_type(self):
        msg = self._protos.packages.pw.test1.AnotherMessage()
        with self.assertRaisesRegex(TypeError, r'pw\.test1\.SomeMessage'):
            self._client.services['pw.test1.PublicService'].methods[
                'SomeUnary'].get_request(msg, {})

    def test_process_packet_invalid_proto_data(self):
        self.assertFalse(self._client.process_packet(b'NOT a packet!'))

    def test_process_packet_unrecognized_channel(self):
        self.assertFalse(
            self._client.process_packet(
                packets.encode_request(
                    (123, 456, 789),
                    self._protos.packages.pw.test2.Request())))

    def test_process_packet_unrecognized_service(self):
        self.assertFalse(
            self._client.process_packet(
                packets.encode_request(
                    (1, 456, 789), self._protos.packages.pw.test2.Request())))

    def test_process_packet_unrecognized_method(self):
        self.assertFalse(
            self._client.process_packet(
                packets.encode_request(
                    (1, next(iter(self._client.services)).id, 789),
                    self._protos.packages.pw.test2.Request())))


if __name__ == '__main__':
    unittest.main()
