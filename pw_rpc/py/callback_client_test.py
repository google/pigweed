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
"""Tests using the callback client for pw_rpc."""

import unittest
from unittest import mock
from typing import List, Tuple

from pw_protobuf_compiler import python_protos
from pw_status import Status

from pw_rpc import callback_client, client, packet_pb2, packets

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


def _rpc(method_stub):
    return client.PendingRpc(method_stub.channel, method_stub.method.service,
                             method_stub.method)


class CallbackClientImplTest(unittest.TestCase):
    """Tests the callback_client as used within a pw_rpc Client."""
    def setUp(self):
        self._protos = python_protos.Library.from_strings(TEST_PROTO_1)

        self._client = client.Client.from_modules(
            callback_client.Impl(), [client.Channel(1, self._handle_request)],
            self._protos.modules())

        self._last_request: packet_pb2.RpcPacket = None
        self._next_packets: List[Tuple[bytes, Status]] = []
        self._send_responses_on_request = True

    def _enqueue_response(self,
                          channel_id: int,
                          method=None,
                          status: Status = Status.OK,
                          response=b'',
                          *,
                          ids: Tuple[int, int] = None,
                          process_status=Status.OK):
        if method:
            assert ids is None
            service_id, method_id = method.service.id, method.id
        else:
            assert ids is not None and method is None
            service_id, method_id = ids

        if isinstance(response, bytes):
            payload = response
        else:
            payload = response.SerializeToString()

        self._next_packets.append(
            (packet_pb2.RpcPacket(type=packets.PacketType.RESPONSE,
                                  channel_id=channel_id,
                                  service_id=service_id,
                                  method_id=method_id,
                                  status=status.value,
                                  payload=payload).SerializeToString(),
             process_status))

    def _enqueue_stream_end(self,
                            channel_id: int,
                            method,
                            status: Status = Status.OK,
                            process_status=Status.OK):
        self._next_packets.append(
            (packet_pb2.RpcPacket(type=packets.PacketType.SERVER_STREAM_END,
                                  channel_id=channel_id,
                                  service_id=method.service.id,
                                  method_id=method.id,
                                  status=status.value).SerializeToString(),
             process_status))

    def _handle_request(self, data: bytes):
        # Disable this method to prevent infinite recursion if processing the
        # packet happens to send another packet.
        if not self._send_responses_on_request:
            return

        self._send_responses_on_request = False

        self._last_request = packets.decode(data)

        for packet, status in self._next_packets:
            self.assertIs(status, self._client.process_packet(packet))

        self._next_packets.clear()
        self._send_responses_on_request = True

    def _sent_payload(self, message_type):
        self.assertIsNotNone(self._last_request)
        message = message_type()
        message.ParseFromString(self._last_request.payload)
        return message

    def test_invoke_unary_rpc(self):
        stub = self._client.channel(1).rpcs.pw.test1.PublicService
        method = stub.SomeUnary.method

        for _ in range(3):
            self._enqueue_response(1, method, Status.ABORTED,
                                   method.response_type(payload='0_o'))

            status, response = stub.SomeUnary(magic_number=6)

            self.assertEqual(
                6,
                self._sent_payload(method.request_type).magic_number)

            self.assertIs(Status.ABORTED, status)
            self.assertEqual('0_o', response.payload)

    def test_invoke_unary_rpc_with_callback(self):
        stub = self._client.channel(1).rpcs.pw.test1.PublicService
        method = stub.SomeUnary.method

        for _ in range(3):
            self._enqueue_response(1, method, Status.ABORTED,
                                   method.response_type(payload='0_o'))

            callback = mock.Mock()
            stub.SomeUnary.invoke(callback, magic_number=5)

            callback.assert_called_once_with(
                _rpc(stub.SomeUnary), Status.ABORTED,
                method.response_type(payload='0_o'))

            self.assertEqual(
                5,
                self._sent_payload(method.request_type).magic_number)

    def test_invoke_unary_rpc_callback_errors_suppressed(self):
        stub = self._client.channel(1).rpcs.pw.test1.PublicService.SomeUnary

        self._enqueue_response(1, stub.method)
        exception_msg = 'YOU BROKE IT O-]-<'

        with self.assertLogs(callback_client.__name__, 'ERROR') as logs:
            stub.invoke(mock.Mock(side_effect=Exception(exception_msg)))

        self.assertIn(exception_msg, ''.join(logs.output))

        # Make sure we can still invoke the RPC.
        self._enqueue_response(1, stub.method, Status.UNKNOWN)
        status, _ = stub()
        self.assertIs(status, Status.UNKNOWN)

    def test_invoke_unary_rpc_with_callback_cancel(self):
        stub = self._client.channel(1).rpcs.pw.test1.PublicService
        callback = mock.Mock()

        for _ in range(3):
            call = stub.SomeUnary.invoke(callback, magic_number=55)

            self.assertIsNotNone(self._last_request)
            self._last_request = None

            # Try to invoke the RPC again before cancelling, which is an error.
            with self.assertRaises(client.Error):
                stub.SomeUnary.invoke(callback, magic_number=56)

            self.assertTrue(call.cancel())
            self.assertFalse(call.cancel())  # Already cancelled, returns False

            # Unary RPCs do not send a cancel request to the server.
            self.assertIsNone(self._last_request)

        callback.assert_not_called()

    def test_reinvoke_unary_rpc(self):
        stub = self._client.channel(1).rpcs.pw.test1.PublicService
        callback = mock.Mock()

        # The reinvoke method ignores pending rpcs, so can be called repeatedly.
        for _ in range(3):
            self._last_request = None
            stub.SomeUnary.reinvoke(callback, magic_number=55)
            self.assertEqual(self._last_request.type,
                             packets.PacketType.REQUEST)

    def test_invoke_server_streaming(self):
        stub = self._client.channel(1).rpcs.pw.test1.PublicService
        method = stub.SomeServerStreaming.method

        rep1 = method.response_type(payload='!!!')
        rep2 = method.response_type(payload='?')

        for _ in range(3):
            self._enqueue_response(1, method, response=rep1)
            self._enqueue_response(1, method, response=rep2)
            self._enqueue_stream_end(1, method, Status.ABORTED)

            self.assertEqual([rep1, rep2],
                             list(stub.SomeServerStreaming(magic_number=4)))

            self.assertEqual(
                4,
                self._sent_payload(method.request_type).magic_number)

    def test_invoke_server_streaming_with_callback(self):
        stub = self._client.channel(1).rpcs.pw.test1.PublicService
        method = stub.SomeServerStreaming.method

        rep1 = method.response_type(payload='!!!')
        rep2 = method.response_type(payload='?')

        for _ in range(3):
            self._enqueue_response(1, method, response=rep1)
            self._enqueue_response(1, method, response=rep2)
            self._enqueue_stream_end(1, method, Status.ABORTED)

            callback = mock.Mock()
            stub.SomeServerStreaming.invoke(callback, magic_number=3)

            rpc = _rpc(stub.SomeServerStreaming)
            callback.assert_has_calls([
                mock.call(rpc, None, method.response_type(payload='!!!')),
                mock.call(rpc, None, method.response_type(payload='?')),
                mock.call(rpc, Status.ABORTED, None),
            ])

            self.assertEqual(
                3,
                self._sent_payload(method.request_type).magic_number)

    def test_invoke_server_streaming_with_callback_cancel(self):
        stub = self._client.channel(
            1).rpcs.pw.test1.PublicService.SomeServerStreaming

        resp = stub.method.response_type(payload='!!!')
        self._enqueue_response(1, stub.method, response=resp)

        callback = mock.Mock()
        call = stub.invoke(callback, magic_number=3)
        callback.assert_called_once_with(
            _rpc(stub), None, stub.method.response_type(payload='!!!'))

        callback.reset_mock()

        call.cancel()

        self.assertEqual(self._last_request.type,
                         packets.PacketType.CANCEL_SERVER_STREAM)

        # Ensure the RPC can be called after being cancelled.
        self._enqueue_response(1, stub.method, response=resp)
        self._enqueue_stream_end(1, stub.method, Status.OK)

        call = stub.invoke(callback, magic_number=3)

        rpc = _rpc(stub)
        callback.assert_has_calls([
            mock.call(rpc, None, stub.method.response_type(payload='!!!')),
            mock.call(rpc, Status.OK, None),
        ])

    def test_ignore_bad_packets_with_pending_rpc(self):
        rpcs = self._client.channel(1).rpcs
        method = rpcs.pw.test1.PublicService.SomeUnary.method
        service_id = method.service.id

        # Unknown channel
        self._enqueue_response(999, method, process_status=Status.NOT_FOUND)
        # Bad service
        self._enqueue_response(1,
                               ids=(999, method.id),
                               process_status=Status.OK)
        # Bad method
        self._enqueue_response(1,
                               ids=(service_id, 999),
                               process_status=Status.OK)
        # For RPC not pending (is Status.OK because the packet is processed)
        self._enqueue_response(
            1,
            ids=(service_id,
                 rpcs.pw.test1.PublicService.SomeBidiStreaming.method.id),
            process_status=Status.OK)

        self._enqueue_response(1, method, process_status=Status.OK)

        status, response = rpcs.pw.test1.PublicService.SomeUnary(
            magic_number=6)
        self.assertIs(Status.OK, status)
        self.assertEqual('', response.payload)

    def test_pass_none_if_payload_fails_to_decode(self):
        rpcs = self._client.channel(1).rpcs
        method = rpcs.pw.test1.PublicService.SomeUnary.method

        self._enqueue_response(1,
                               method,
                               Status.OK,
                               b'INVALID DATA!!!',
                               process_status=Status.OK)

        status, response = rpcs.pw.test1.PublicService.SomeUnary(
            magic_number=6)
        self.assertIs(status, Status.OK)
        self.assertIsNone(response)

    def test_rpc_help_contains_method_name(self):
        rpc = self._client.channel(1).rpcs.pw.test1.PublicService.SomeUnary
        self.assertIn(rpc.method.full_name, rpc.help())


if __name__ == '__main__':
    unittest.main()
