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
from typing import List, Tuple

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


def _rpc(method_stub):
    return client.PendingRpc(method_stub.channel, method_stub.method.service,
                             method_stub.method)


def _message_bytes(msg) -> bytes:
    return msg if isinstance(msg, bytes) else msg.SerializeToString()


class CallbackClientImplTest(unittest.TestCase):  # pylint: disable=too-many-public-methods
    """Tests the callback_client as used within a pw_rpc Client."""
    def setUp(self):
        self._protos = python_protos.Library.from_strings(TEST_PROTO_1)
        self._request = self._protos.packages.pw.test1.SomeMessage

        self._client = client.Client.from_modules(
            callback_client.Impl(), [client.Channel(1, self._handle_request)],
            self._protos.modules())
        self._service = self._client.channel(1).rpcs.pw.test1.PublicService

        self._last_request: packet_pb2.RpcPacket = None
        self._next_packets: List[Tuple[bytes, Status]] = []
        self._send_responses_on_request = True

    def _enqueue_response(self,
                          channel_id: int,
                          method=None,
                          status: Status = Status.OK,
                          payload=b'',
                          *,
                          ids: Tuple[int, int] = None,
                          process_status=Status.OK):
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
                               process_status=Status.OK):
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
                       process_status=Status.OK):
        self._next_packets.append(
            (packet_pb2.RpcPacket(type=packet_pb2.PacketType.SERVER_ERROR,
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

    def test_invoke_unary_rpc_keep_open(self) -> None:
        method = self._service.SomeUnary.method

        payload_1 = method.response_type(payload='-_-')
        payload_2 = method.response_type(payload='0_o')

        self._enqueue_response(1, method, Status.ABORTED, payload_1)

        replies: list = []
        enqueue_replies = lambda _, reply: replies.append(reply)

        self._service.SomeUnary.invoke(method.request_type(magic_number=6),
                                       enqueue_replies,
                                       enqueue_replies,
                                       keep_open=True)

        self.assertEqual([payload_1, Status.ABORTED], replies)

        # Send another packet and make sure it is processed even though the RPC
        # terminated.
        self._client.process_packet(
            packet_pb2.RpcPacket(
                type=packet_pb2.PacketType.RESPONSE,
                channel_id=1,
                service_id=method.service.id,
                method_id=method.id,
                status=Status.OK.value,
                payload=payload_2.SerializeToString()).SerializeToString())

        self.assertEqual([payload_1, Status.ABORTED, payload_2, Status.OK],
                         replies)

    def test_invoke_unary_rpc_with_callback(self):
        method = self._service.SomeUnary.method

        for _ in range(3):
            self._enqueue_response(1, method, Status.ABORTED,
                                   method.response_type(payload='0_o'))

            callback = mock.Mock()
            self._service.SomeUnary.invoke(self._request(magic_number=5),
                                           callback, callback)

            callback.assert_has_calls([
                mock.call(_rpc(self._service.SomeUnary),
                          method.response_type(payload='0_o')),
                mock.call(_rpc(self._service.SomeUnary), Status.ABORTED)
            ])

            self.assertEqual(
                5,
                self._sent_payload(method.request_type).magic_number)

    def test_unary_rpc_server_error(self):
        method = self._service.SomeUnary.method

        for _ in range(3):
            self._enqueue_error(1, method, Status.NOT_FOUND)

            with self.assertRaises(callback_client.RpcError) as context:
                self._service.SomeUnary(method.request_type(magic_number=6))

            self.assertIs(context.exception.status, Status.NOT_FOUND)

    def test_invoke_unary_rpc_callback_exceptions_suppressed(self):
        stub = self._service.SomeUnary

        self._enqueue_response(1, stub.method)
        exception_msg = 'YOU BROKE IT O-]-<'

        with self.assertLogs(callback_client.__name__, 'ERROR') as logs:
            stub.invoke(self._request(),
                        mock.Mock(side_effect=Exception(exception_msg)))

        self.assertIn(exception_msg, ''.join(logs.output))

        # Make sure we can still invoke the RPC.
        self._enqueue_response(1, stub.method, Status.UNKNOWN)
        status, _ = stub()
        self.assertIs(status, Status.UNKNOWN)

    def test_invoke_unary_rpc_with_callback_cancel(self):
        callback = mock.Mock()

        for _ in range(3):
            call = self._service.SomeUnary.invoke(
                self._request(magic_number=55), callback)

            self.assertIsNotNone(self._last_request)
            self._last_request = None

            # Try to invoke the RPC again before cancelling, without overriding
            # pending RPCs.
            with self.assertRaises(client.Error):
                self._service.SomeUnary.invoke(self._request(magic_number=56),
                                               callback,
                                               override_pending=False)

            self.assertTrue(call.cancel())
            self.assertFalse(call.cancel())  # Already cancelled, returns False

            # Unary RPCs do not send a cancel request to the server.
            self.assertIsNone(self._last_request)

        callback.assert_not_called()

    def test_reinvoke_unary_rpc(self):
        for _ in range(3):
            self._last_request = None
            self._service.SomeUnary.invoke(self._request(magic_number=55),
                                           override_pending=True)
            self.assertEqual(self._last_request.type,
                             packet_pb2.PacketType.REQUEST)

    def test_invoke_server_streaming(self):
        method = self._service.SomeServerStreaming.method

        rep1 = method.response_type(payload='!!!')
        rep2 = method.response_type(payload='?')

        for _ in range(3):
            self._enqueue_server_stream(1, method, rep1)
            self._enqueue_server_stream(1, method, rep2)
            self._enqueue_response(1, method, Status.ABORTED)

            self.assertEqual(
                [rep1, rep2],
                list(self._service.SomeServerStreaming(magic_number=4)))

            self.assertEqual(
                4,
                self._sent_payload(method.request_type).magic_number)

    def test_invoke_server_streaming_with_deprecated_packet_format(self):
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

            call = self._service.SomeServerStreaming(magic_number=4)
            self.assertEqual([rep1, rep2], list(call))
            self.assertIs(call.status, Status.INVALID_ARGUMENT)

            self.assertEqual(
                4,
                self._sent_payload(method.request_type).magic_number)

    def test_invoke_server_streaming_with_callbacks(self):
        method = self._service.SomeServerStreaming.method

        rep1 = method.response_type(payload='!!!')
        rep2 = method.response_type(payload='?')

        for _ in range(3):
            self._enqueue_server_stream(1, method, rep1)
            self._enqueue_server_stream(1, method, rep2)
            self._enqueue_response(1, method, Status.ABORTED)

            callback = mock.Mock()
            self._service.SomeServerStreaming.invoke(
                self._request(magic_number=3), callback, callback)

            rpc = _rpc(self._service.SomeServerStreaming)
            callback.assert_has_calls([
                mock.call(rpc, method.response_type(payload='!!!')),
                mock.call(rpc, method.response_type(payload='?')),
                mock.call(rpc, Status.ABORTED),
            ])

            self.assertEqual(
                3,
                self._sent_payload(method.request_type).magic_number)

    def test_invoke_server_streaming_with_callback_cancel(self):
        stub = self._service.SomeServerStreaming

        resp = stub.method.response_type(payload='!!!')
        self._enqueue_server_stream(1, stub.method, resp)

        callback = mock.Mock()
        call = stub.invoke(self._request(magic_number=3), callback)
        callback.assert_called_once_with(
            _rpc(stub), stub.method.response_type(payload='!!!'))

        callback.reset_mock()

        call.cancel()

        self.assertEqual(self._last_request.type, packet_pb2.PacketType.CANCEL)

        # Ensure the RPC can be called after being cancelled.
        self._enqueue_server_stream(1, stub.method, resp)
        self._enqueue_response(1, stub.method, Status.OK)

        call = stub.invoke(self._request(magic_number=3), callback, callback)

        callback.assert_has_calls([
            mock.call(_rpc(stub), stub.method.response_type(payload='!!!')),
            mock.call(_rpc(stub), Status.OK),
        ])

    def test_invoke_client_streaming(self):
        """Tests a successful client streaming RPC ended by the server."""
        method = self._service.SomeClientStreaming.method

        payload_1 = method.response_type(payload='-_-')

        for _ in range(3):
            stream = self._service.SomeClientStreaming()
            self.assertFalse(stream.complete())

            stream.send(magic_number=31)
            self.assertIs(packet_pb2.PacketType.CLIENT_STREAM,
                          self._last_request.type)
            self.assertEqual(
                31,
                self._sent_payload(method.request_type).magic_number)
            self.assertFalse(stream.complete())

            # Enqueue the server response to be sent after the next message.
            self._enqueue_response(1, method, Status.OK, payload_1)

            stream.send(magic_number=32)
            self.assertIs(packet_pb2.PacketType.CLIENT_STREAM,
                          self._last_request.type)
            self.assertEqual(
                32,
                self._sent_payload(method.request_type).magic_number)

            self.assertTrue(stream.complete())
            self.assertIs(Status.OK, stream.status)
            self.assertIsNone(stream.error)
            self.assertEqual(payload_1, stream.response)

    def test_invoke_client_streaming_with_finish(self):
        """Tests a client streaming RPC ended by the client."""
        method = self._service.SomeClientStreaming.method

        payload_1 = method.response_type(payload='-_-')

        for _ in range(3):
            stream = self._service.SomeClientStreaming()
            self.assertFalse(stream.complete())

            stream.send(magic_number=37)
            self.assertIs(packet_pb2.PacketType.CLIENT_STREAM,
                          self._last_request.type)
            self.assertEqual(
                37,
                self._sent_payload(method.request_type).magic_number)
            self.assertFalse(stream.complete())

            # Enqueue the server response to be sent after the next message.
            self._enqueue_response(1, method, Status.OK, payload_1)

            stream.finish_and_wait()
            self.assertIs(packet_pb2.PacketType.CLIENT_STREAM_END,
                          self._last_request.type)

            self.assertTrue(stream.complete())
            self.assertIs(Status.OK, stream.status)
            self.assertIsNone(stream.error)
            self.assertEqual(payload_1, stream.response)

    def test_invoke_client_streaming_cancel(self):
        for _ in range(3):
            stream = self._service.SomeClientStreaming()
            stream.send(magic_number=37)

            self.assertTrue(stream.cancel())
            self.assertIs(packet_pb2.PacketType.CANCEL,
                          self._last_request.type)
            self.assertFalse(stream.cancel())

            self.assertTrue(stream.complete())
            self.assertIs(stream.error.status, Status.CANCELLED)

    def test_invoke_client_streaming_server_error(self):
        method = self._service.SomeClientStreaming.method

        for _ in range(3):
            stream = self._service.SomeClientStreaming()

            self._enqueue_error(1, method, Status.INVALID_ARGUMENT)
            stream.send(magic_number=2**32 - 1)

            with self.assertRaises(callback_client.RpcError) as context:
                stream.finish_and_wait()

            self.assertIs(context.exception.status, Status.INVALID_ARGUMENT)

    def test_invoke_client_streaming_server_error_after_stream_end(self):
        method = self._service.SomeClientStreaming.method

        for _ in range(3):
            stream = self._service.SomeClientStreaming()

            # Error will be sent in response to the CLIENT_STREAM_END packet.
            self._enqueue_error(1, method, Status.INVALID_ARGUMENT)

            with self.assertRaises(callback_client.RpcError) as context:
                stream.finish_and_wait()

            self.assertIs(context.exception.status, Status.INVALID_ARGUMENT)

    def test_invoke_bidirectional_streaming(self):
        """Tests a bidirectional streaming RPC ended by the server."""
        method = self._service.SomeBidiStreaming.method

        rep1 = method.response_type(payload='!!!')
        rep2 = method.response_type(payload='?')

        for _ in range(3):
            responses = []
            stream = self._service.SomeBidiStreaming(
                lambda _, res, responses=responses: responses.append(res))
            self.assertFalse(stream.complete())

            stream.send(magic_number=55)
            self.assertIs(packet_pb2.PacketType.CLIENT_STREAM,
                          self._last_request.type)
            self.assertEqual(
                55,
                self._sent_payload(method.request_type).magic_number)
            self.assertFalse(stream.complete())
            self.assertEqual([], responses)

            self._enqueue_server_stream(1, method, rep1)
            self._enqueue_server_stream(1, method, rep2)

            stream.send(magic_number=66)
            self.assertIs(packet_pb2.PacketType.CLIENT_STREAM,
                          self._last_request.type)
            self.assertEqual(
                66,
                self._sent_payload(method.request_type).magic_number)
            self.assertFalse(stream.complete())
            self.assertEqual([rep1, rep2], responses)

            self._enqueue_response(1, method, Status.OK)

            stream.send(magic_number=77)
            self.assertTrue(stream.complete())
            self.assertEqual([rep1, rep2], responses)

            self.assertIs(Status.OK, stream.status)
            self.assertIsNone(stream.error)

    def test_invoke_bidirectional_streaming_error(self):
        method = self._service.SomeBidiStreaming.method

        rep1 = method.response_type(payload='!!!')

        for _ in range(3):
            responses = []
            stream = self._service.SomeBidiStreaming(
                lambda _, res, responses=responses: responses.append(res))
            self.assertFalse(stream.complete())

            self._enqueue_server_stream(1, method, rep1)

            stream.send(magic_number=55)
            self.assertFalse(stream.complete())
            self.assertEqual([rep1], responses)

            self._enqueue_error(1, method, Status.OUT_OF_RANGE)

            stream.send(magic_number=99999)
            self.assertTrue(stream.complete())
            self.assertEqual([rep1], responses)

            self.assertIsNone(stream.status)
            self.assertIs(Status.OUT_OF_RANGE, stream.error.status)

            with self.assertRaises(callback_client.RpcError) as context:
                stream.finish_and_wait()
            self.assertIs(context.exception.status, Status.OUT_OF_RANGE)

    def test_ignore_bad_packets_with_pending_rpc(self):
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

    def test_pass_none_if_payload_fails_to_decode(self):
        method = self._service.SomeUnary.method

        self._enqueue_response(1,
                               method,
                               Status.OK,
                               b'INVALID DATA!!!',
                               process_status=Status.OK)

        status, response = self._service.SomeUnary(magic_number=6)
        self.assertIs(status, Status.OK)
        self.assertIsNone(response)

    def test_rpc_help_contains_method_name(self):
        rpc = self._service.SomeUnary
        self.assertIn(rpc.method.full_name, rpc.help())

    def test_default_timeouts_set_on_impl(self):
        impl = callback_client.Impl(None, 1.5)

        self.assertEqual(impl.default_unary_timeout_s, None)
        self.assertEqual(impl.default_stream_timeout_s, 1.5)

    def test_default_timeouts_set_for_all_rpcs(self):
        rpc_client = client.Client.from_modules(callback_client.Impl(
            99, 100), [client.Channel(1, lambda *a, **b: None)],
                                                self._protos.modules())
        rpcs = rpc_client.channel(1).rpcs

        self.assertEqual(
            rpcs.pw.test1.PublicService.SomeUnary.default_timeout_s, 99)
        self.assertEqual(
            rpcs.pw.test1.PublicService.SomeServerStreaming.default_timeout_s,
            100)

    def test_timeout_unary(self):
        with self.assertRaises(callback_client.RpcTimeout):
            self._service.SomeUnary(pw_rpc_timeout_s=0.0001)

    def test_timeout_unary_set_default(self):
        self._service.SomeUnary.default_timeout_s = 0.0001

        with self.assertRaises(callback_client.RpcTimeout):
            self._service.SomeUnary()

    def test_timeout_server_streaming_iteration(self):
        responses = self._service.SomeServerStreaming(pw_rpc_timeout_s=0.0001)
        with self.assertRaises(callback_client.RpcTimeout):
            for _ in responses:
                pass

    def test_timeout_server_streaming_responses(self):
        responses = self._service.SomeServerStreaming()
        with self.assertRaises(callback_client.RpcTimeout):
            for _ in responses.responses(timeout_s=0.0001):
                pass


if __name__ == '__main__':
    unittest.main()
