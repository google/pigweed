#!/usr/bin/env python3
# Copyright 2021 The Pigweed Authors
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
from typing import Any, List, Optional, Tuple

from pw_protobuf_compiler import python_protos
from pw_status import Status

from pw_rpc import callback_client, client, packets
from pw_rpc.internal import packet_pb2

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


def _message_bytes(msg) -> bytes:
    return msg if isinstance(msg, bytes) else msg.SerializeToString()


class _CallbackClientImplTestBase(unittest.TestCase):
    """Supports writing tests that require responses from an RPC server."""
    def setUp(self) -> None:
        self._protos = python_protos.Library.from_strings(TEST_PROTO_1)
        self._request = self._protos.packages.pw.test1.SomeMessage

        self._client = client.Client.from_modules(
            callback_client.Impl(), [client.Channel(1, self._handle_request)],
            self._protos.modules())
        self._service = self._client.channel(1).rpcs.pw.test1.PublicService

        self._last_request_if_sent: Optional[packet_pb2.RpcPacket] = None
        self._next_packets: List[Tuple[bytes, Status]] = []
        self._send_responses_on_request = True

    def last_request(self) -> packet_pb2.RpcPacket:
        assert self._last_request_if_sent is not None
        return self._last_request_if_sent

    def _enqueue_response(self,
                          channel_id: int,
                          method=None,
                          status: Status = Status.OK,
                          payload=b'',
                          *,
                          ids: Tuple[int, int] = None,
                          process_status=Status.OK) -> None:
        if method:
            assert ids is None
            service_id, method_id = method.service.id, method.id
        else:
            assert ids is not None and method is None
            service_id, method_id = ids

        self._next_packets.append((packet_pb2.RpcPacket(
            type=packet_pb2.PacketType.RESPONSE,
            channel_id=channel_id,
            service_id=service_id,
            method_id=method_id,
            status=status.value,
            payload=_message_bytes(payload)).SerializeToString(),
                                   process_status))

    def _enqueue_server_stream(self,
                               channel_id: int,
                               method,
                               response,
                               process_status=Status.OK) -> None:
        self._next_packets.append((packet_pb2.RpcPacket(
            type=packet_pb2.PacketType.SERVER_STREAM,
            channel_id=channel_id,
            service_id=method.service.id,
            method_id=method.id,
            payload=_message_bytes(response)).SerializeToString(),
                                   process_status))

    def _enqueue_error(self,
                       channel_id: int,
                       method,
                       status: Status,
                       process_status=Status.OK) -> None:
        self._next_packets.append(
            (packet_pb2.RpcPacket(type=packet_pb2.PacketType.SERVER_ERROR,
                                  channel_id=channel_id,
                                  service_id=method.service.id,
                                  method_id=method.id,
                                  status=status.value).SerializeToString(),
             process_status))

    def _handle_request(self, data: bytes) -> None:
        # Disable this method to prevent infinite recursion if processing the
        # packet happens to send another packet.
        if not self._send_responses_on_request:
            return

        self._send_responses_on_request = False

        self._last_request_if_sent = packets.decode(data)

        for packet, status in self._next_packets:
            self.assertIs(status, self._client.process_packet(packet))

        self._next_packets.clear()
        self._send_responses_on_request = True

    def _sent_payload(self, message_type: type) -> Any:
        message = message_type()
        message.ParseFromString(self.last_request().payload)
        return message


class CallbackClientImplTest(_CallbackClientImplTestBase):
    """Tests the callback_client.Impl client implementation."""
    def test_callback_exceptions_suppressed(self) -> None:
        stub = self._service.SomeUnary

        self._enqueue_response(1, stub.method)
        exception_msg = 'YOU BROKE IT O-]-<'

        with self.assertLogs(callback_client.__package__, 'ERROR') as logs:
            stub.invoke(self._request(),
                        mock.Mock(side_effect=Exception(exception_msg)))

        self.assertIn(exception_msg, ''.join(logs.output))

        # Make sure we can still invoke the RPC.
        self._enqueue_response(1, stub.method, Status.UNKNOWN)
        status, _ = stub()
        self.assertIs(status, Status.UNKNOWN)

    def test_ignore_bad_packets_with_pending_rpc(self) -> None:
        method = self._service.SomeUnary.method
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
        self._enqueue_response(1,
                               ids=(service_id,
                                    self._service.SomeBidiStreaming.method.id),
                               process_status=Status.OK)

        self._enqueue_response(1, method, process_status=Status.OK)

        status, response = self._service.SomeUnary(magic_number=6)
        self.assertIs(Status.OK, status)
        self.assertEqual('', response.payload)

    def test_exception_if_payload_fails_to_decode(self) -> None:
        method = self._service.SomeUnary.method

        self._enqueue_response(1,
                               method,
                               Status.OK,
                               b'INVALID DATA!!!',
                               process_status=Status.OK)

        with self.assertRaises(callback_client.RpcError) as context:
            self._service.SomeUnary(magic_number=6)

        self.assertIs(context.exception.status, Status.DATA_LOSS)

    def test_rpc_help_contains_method_name(self) -> None:
        rpc = self._service.SomeUnary
        self.assertIn(rpc.method.full_name, rpc.help())

    def test_default_timeouts_set_on_impl(self) -> None:
        impl = callback_client.Impl(None, 1.5)

        self.assertEqual(impl.default_unary_timeout_s, None)
        self.assertEqual(impl.default_stream_timeout_s, 1.5)

    def test_default_timeouts_set_for_all_rpcs(self) -> None:
        rpc_client = client.Client.from_modules(callback_client.Impl(
            99, 100), [client.Channel(1, lambda *a, **b: None)],
                                                self._protos.modules())
        rpcs = rpc_client.channel(1).rpcs

        self.assertEqual(
            rpcs.pw.test1.PublicService.SomeUnary.default_timeout_s, 99)
        self.assertEqual(
            rpcs.pw.test1.PublicService.SomeServerStreaming.default_timeout_s,
            100)
        self.assertEqual(
            rpcs.pw.test1.PublicService.SomeClientStreaming.default_timeout_s,
            99)
        self.assertEqual(
            rpcs.pw.test1.PublicService.SomeBidiStreaming.default_timeout_s,
            100)


class UnaryTest(_CallbackClientImplTestBase):
    """Tests for invoking a unary RPC."""
    def test_blocking_call(self) -> None:
        method = self._service.SomeUnary.method

        for _ in range(3):
            self._enqueue_response(1, method, Status.ABORTED,
                                   method.response_type(payload='0_o'))

            status, response = self._service.SomeUnary(
                method.request_type(magic_number=6))

            self.assertEqual(
                6,
                self._sent_payload(method.request_type).magic_number)

            self.assertIs(Status.ABORTED, status)
            self.assertEqual('0_o', response.payload)

    def test_nonblocking_call(self) -> None:
        method = self._service.SomeUnary.method

        for _ in range(3):
            self._enqueue_response(1, method, Status.ABORTED,
                                   method.response_type(payload='0_o'))

            callback = mock.Mock()
            call = self._service.SomeUnary.invoke(
                self._request(magic_number=5), callback, callback)

            callback.assert_has_calls([
                mock.call(call, method.response_type(payload='0_o')),
                mock.call(call, Status.ABORTED)
            ])

            self.assertEqual(
                5,
                self._sent_payload(method.request_type).magic_number)

    def test_blocking_server_error(self) -> None:
        method = self._service.SomeUnary.method

        for _ in range(3):
            self._enqueue_error(1, method, Status.NOT_FOUND)

            with self.assertRaises(callback_client.RpcError) as context:
                self._service.SomeUnary(method.request_type(magic_number=6))

            self.assertIs(context.exception.status, Status.NOT_FOUND)

    def test_nonblocking_cancel(self) -> None:
        callback = mock.Mock()

        for _ in range(3):
            call = self._service.SomeUnary.invoke(
                self._request(magic_number=55), callback)

            self.assertIsNotNone(self._last_request_if_sent)
            self._last_request_if_sent = None

            self.assertTrue(call.cancel())
            self.assertFalse(call.cancel())  # Already cancelled, returns False

            # Unary RPCs do not send a cancel request to the server.
            self.assertIsNone(self._last_request_if_sent)

        callback.assert_not_called()

    def test_timeout_as_argument(self) -> None:
        with self.assertRaises(callback_client.RpcTimeout):
            self._service.SomeUnary(pw_rpc_timeout_s=0.0001)

    def test_timeout_set_default(self) -> None:
        self._service.SomeUnary.default_timeout_s = 0.0001

        with self.assertRaises(callback_client.RpcTimeout):
            self._service.SomeUnary()


class ServerStreamingTest(_CallbackClientImplTestBase):
    """Tests for server streaming RPCs."""
    def test_blocking_call(self) -> None:
        method = self._service.SomeServerStreaming.method

        rep1 = method.response_type(payload='!!!')
        rep2 = method.response_type(payload='?')

        for _ in range(3):
            self._enqueue_server_stream(1, method, rep1)
            self._enqueue_server_stream(1, method, rep2)
            self._enqueue_response(1, method, Status.ABORTED)

            self.assertEqual(
                [rep1, rep2],
                self._service.SomeServerStreaming(magic_number=4).responses)

            self.assertEqual(
                4,
                self._sent_payload(method.request_type).magic_number)

    def test_deprecated_packet_format(self) -> None:
        method = self._service.SomeServerStreaming.method

        rep1 = method.response_type(payload='!!!')
        rep2 = method.response_type(payload='?')

        for _ in range(3):
            # The original packet format used RESPONSE packets for the server
            # stream and a SERVER_STREAM_END packet as the last packet. These
            # are converted to SERVER_STREAM packets followed by a RESPONSE.
            self._enqueue_response(1, method, payload=rep1)
            self._enqueue_response(1, method, payload=rep2)

            self._next_packets.append((packet_pb2.RpcPacket(
                type=packet_pb2.PacketType.DEPRECATED_SERVER_STREAM_END,
                channel_id=1,
                service_id=method.service.id,
                method_id=method.id,
                status=Status.INVALID_ARGUMENT.value).SerializeToString(),
                                       Status.OK))

            status, replies = self._service.SomeServerStreaming(magic_number=4)
            self.assertEqual([rep1, rep2], replies)
            self.assertIs(status, Status.INVALID_ARGUMENT)

            self.assertEqual(
                4,
                self._sent_payload(method.request_type).magic_number)

    def test_nonblocking_call(self) -> None:
        method = self._service.SomeServerStreaming.method

        rep1 = method.response_type(payload='!!!')
        rep2 = method.response_type(payload='?')

        for _ in range(3):
            self._enqueue_server_stream(1, method, rep1)
            self._enqueue_server_stream(1, method, rep2)
            self._enqueue_response(1, method, Status.ABORTED)

            callback = mock.Mock()
            call = self._service.SomeServerStreaming.invoke(
                self._request(magic_number=3), callback, callback)

            callback.assert_has_calls([
                mock.call(call, method.response_type(payload='!!!')),
                mock.call(call, method.response_type(payload='?')),
                mock.call(call, Status.ABORTED),
            ])

            self.assertEqual(
                3,
                self._sent_payload(method.request_type).magic_number)

    def test_nonblocking_cancel(self) -> None:
        stub = self._service.SomeServerStreaming

        resp = stub.method.response_type(payload='!!!')
        self._enqueue_server_stream(1, stub.method, resp)

        callback = mock.Mock()
        call = stub.invoke(self._request(magic_number=3), callback)
        callback.assert_called_once_with(
            call, stub.method.response_type(payload='!!!'))

        callback.reset_mock()

        call.cancel()

        self.assertEqual(self.last_request().type,
                         packet_pb2.PacketType.CANCEL)

        # Ensure the RPC can be called after being cancelled.
        self._enqueue_server_stream(1, stub.method, resp)
        self._enqueue_response(1, stub.method, Status.OK)

        call = stub.invoke(self._request(magic_number=3), callback, callback)

        callback.assert_has_calls([
            mock.call(call, stub.method.response_type(payload='!!!')),
            mock.call(call, Status.OK),
        ])

    def test_blocking_timeout(self) -> None:
        with self.assertRaises(callback_client.RpcTimeout):
            self._service.SomeServerStreaming(pw_rpc_timeout_s=0.0001)

    def test_nonblocking_iteration_timeout(self) -> None:
        call = self._service.SomeServerStreaming.invoke(timeout_s=0.0001)
        with self.assertRaises(callback_client.RpcTimeout):
            for _ in call:
                pass


class ClientStreamingTest(_CallbackClientImplTestBase):
    """Tests for client streaming RPCs."""
    def test_nonblocking_call(self) -> None:
        """Tests a successful client streaming RPC ended by the server."""
        method = self._service.SomeClientStreaming.method

        payload_1 = method.response_type(payload='-_-')

        for _ in range(3):
            stream = self._service.SomeClientStreaming.invoke()
            self.assertFalse(stream.completed())

            stream.send(magic_number=31)
            self.assertIs(packet_pb2.PacketType.CLIENT_STREAM,
                          self.last_request().type)
            self.assertEqual(
                31,
                self._sent_payload(method.request_type).magic_number)
            self.assertFalse(stream.completed())

            # Enqueue the server response to be sent after the next message.
            self._enqueue_response(1, method, Status.OK, payload_1)

            stream.send(magic_number=32)
            self.assertIs(packet_pb2.PacketType.CLIENT_STREAM,
                          self.last_request().type)
            self.assertEqual(
                32,
                self._sent_payload(method.request_type).magic_number)

            self.assertTrue(stream.completed())
            self.assertIs(Status.OK, stream.status)
            self.assertIsNone(stream.error)
            self.assertEqual(payload_1, stream.response)

    def test_nonblocking_finish(self) -> None:
        """Tests a client streaming RPC ended by the client."""
        method = self._service.SomeClientStreaming.method

        payload_1 = method.response_type(payload='-_-')

        for _ in range(3):
            stream = self._service.SomeClientStreaming.invoke()
            self.assertFalse(stream.completed())

            stream.send(magic_number=37)
            self.assertIs(packet_pb2.PacketType.CLIENT_STREAM,
                          self.last_request().type)
            self.assertEqual(
                37,
                self._sent_payload(method.request_type).magic_number)
            self.assertFalse(stream.completed())

            # Enqueue the server response to be sent after the next message.
            self._enqueue_response(1, method, Status.OK, payload_1)

            stream.finish_and_wait()
            self.assertIs(packet_pb2.PacketType.CLIENT_STREAM_END,
                          self.last_request().type)

            self.assertTrue(stream.completed())
            self.assertIs(Status.OK, stream.status)
            self.assertIsNone(stream.error)
            self.assertEqual(payload_1, stream.response)

    def test_nonblocking_cancel(self) -> None:
        for _ in range(3):
            stream = self._service.SomeClientStreaming.invoke()
            stream.send(magic_number=37)

            self.assertTrue(stream.cancel())
            self.assertIs(packet_pb2.PacketType.CANCEL,
                          self.last_request().type)
            self.assertFalse(stream.cancel())

            self.assertTrue(stream.completed())
            self.assertIs(stream.error.status, Status.CANCELLED)

    def test_nonblocking_server_error(self) -> None:
        method = self._service.SomeClientStreaming.method

        for _ in range(3):
            stream = self._service.SomeClientStreaming.invoke()

            self._enqueue_error(1, method, Status.INVALID_ARGUMENT)
            stream.send(magic_number=2**32 - 1)

            with self.assertRaises(callback_client.RpcError) as context:
                stream.finish_and_wait()

            self.assertIs(context.exception.status, Status.INVALID_ARGUMENT)

    def test_nonblocking_server_error_after_stream_end(self) -> None:
        method = self._service.SomeClientStreaming.method

        for _ in range(3):
            stream = self._service.SomeClientStreaming.invoke()

            # Error will be sent in response to the CLIENT_STREAM_END packet.
            self._enqueue_error(1, method, Status.INVALID_ARGUMENT)

            with self.assertRaises(callback_client.RpcError) as context:
                stream.finish_and_wait()

            self.assertIs(context.exception.status, Status.INVALID_ARGUMENT)


class BidirectionalStreamingTest(_CallbackClientImplTestBase):
    """Tests for bidirectional streaming RPCs."""
    def test_nonblocking_call(self) -> None:
        """Tests a bidirectional streaming RPC ended by the server."""
        method = self._service.SomeBidiStreaming.method

        rep1 = method.response_type(payload='!!!')
        rep2 = method.response_type(payload='?')

        for _ in range(3):
            responses: list = []
            stream = self._service.SomeBidiStreaming.invoke(
                lambda _, res, responses=responses: responses.append(res))
            self.assertFalse(stream.completed())

            stream.send(magic_number=55)
            self.assertIs(packet_pb2.PacketType.CLIENT_STREAM,
                          self.last_request().type)
            self.assertEqual(
                55,
                self._sent_payload(method.request_type).magic_number)
            self.assertFalse(stream.completed())
            self.assertEqual([], responses)

            self._enqueue_server_stream(1, method, rep1)
            self._enqueue_server_stream(1, method, rep2)

            stream.send(magic_number=66)
            self.assertIs(packet_pb2.PacketType.CLIENT_STREAM,
                          self.last_request().type)
            self.assertEqual(
                66,
                self._sent_payload(method.request_type).magic_number)
            self.assertFalse(stream.completed())
            self.assertEqual([rep1, rep2], responses)

            self._enqueue_response(1, method, Status.OK)

            stream.send(magic_number=77)
            self.assertTrue(stream.completed())
            self.assertEqual([rep1, rep2], responses)

            self.assertIs(Status.OK, stream.status)
            self.assertIsNone(stream.error)

    @mock.patch('pw_rpc.callback_client.call._Call._default_response')
    def test_nonblocking(self, callback) -> None:
        """Tests a bidirectional streaming RPC ended by the server."""
        method = self._service.SomeBidiStreaming.method
        reply = method.response_type(payload='This is the payload!')
        self._enqueue_server_stream(1, method, reply)

        self._service.SomeBidiStreaming.invoke()

        callback.assert_called_once_with(mock.ANY, reply)

    def test_nonblocking_server_error(self) -> None:
        method = self._service.SomeBidiStreaming.method

        rep1 = method.response_type(payload='!!!')

        for _ in range(3):
            responses: list = []
            stream = self._service.SomeBidiStreaming.invoke(
                lambda _, res, responses=responses: responses.append(res))
            self.assertFalse(stream.completed())

            self._enqueue_server_stream(1, method, rep1)

            stream.send(magic_number=55)
            self.assertFalse(stream.completed())
            self.assertEqual([rep1], responses)

            self._enqueue_error(1, method, Status.OUT_OF_RANGE)

            stream.send(magic_number=99999)
            self.assertTrue(stream.completed())
            self.assertEqual([rep1], responses)

            self.assertIsNone(stream.status)
            self.assertIs(Status.OUT_OF_RANGE, stream.error.status)

            with self.assertRaises(callback_client.RpcError) as context:
                stream.finish_and_wait()
            self.assertIs(context.exception.status, Status.OUT_OF_RANGE)


if __name__ == '__main__':
    unittest.main()
