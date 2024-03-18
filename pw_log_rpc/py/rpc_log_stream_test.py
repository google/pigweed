# Copyright 2023 The Pigweed Authors
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

"""RPC log stream handler tests."""

from dataclasses import dataclass
import logging
from typing import Any, Callable
from unittest import TestCase, main, mock

from google.protobuf import message
from pw_log.log_decoder import Log, LogStreamDecoder
from pw_log.proto import log_pb2
from pw_log_rpc.rpc_log_stream import LogStreamHandler
from pw_rpc import callback_client, client, packets
from pw_rpc.internal import packet_pb2
from pw_status import Status

_LOG = logging.getLogger(__name__)


def _encode_server_stream_packet(
    rpc: packets.RpcIds, payload: message.Message
) -> bytes:
    return packet_pb2.RpcPacket(
        type=packet_pb2.PacketType.SERVER_STREAM,
        channel_id=rpc.channel_id,
        service_id=rpc.service_id,
        method_id=rpc.method_id,
        call_id=rpc.call_id,
        payload=payload.SerializeToString(),
    ).SerializeToString()


def _encode_cancel(rpc: packets.RpcIds) -> bytes:
    return packet_pb2.RpcPacket(
        type=packet_pb2.PacketType.SERVER_ERROR,
        status=Status.CANCELLED.value,
        channel_id=rpc.channel_id,
        service_id=rpc.service_id,
        method_id=rpc.method_id,
        call_id=rpc.call_id,
    ).SerializeToString()


def _encode_error(rpc: packets.RpcIds) -> bytes:
    return packet_pb2.RpcPacket(
        type=packet_pb2.PacketType.SERVER_ERROR,
        status=Status.UNKNOWN.value,
        channel_id=rpc.channel_id,
        service_id=rpc.service_id,
        method_id=rpc.method_id,
        call_id=rpc.call_id,
    ).SerializeToString()


def _encode_completed(rpc: packets.RpcIds, status: Status) -> bytes:
    return packet_pb2.RpcPacket(
        type=packet_pb2.PacketType.RESPONSE,
        status=status.value,
        channel_id=rpc.channel_id,
        service_id=rpc.service_id,
        method_id=rpc.method_id,
        call_id=rpc.call_id,
    ).SerializeToString()


class _CallableWithCounter:
    """Wraps a function and counts how many time it was called."""

    @dataclass
    class CallParams:
        args: Any
        kwargs: Any

    def __init__(self, func: Callable[[Any], Any]):
        self._func = func
        self.calls: list[_CallableWithCounter.CallParams] = []

    def call_count(self) -> int:
        return len(self.calls)

    def __call__(self, *args, **kwargs) -> None:
        self.calls.append(_CallableWithCounter.CallParams(args, kwargs))
        self._func(*args, **kwargs)


class TestRpcLogStreamHandler(TestCase):
    """Tests for TestRpcLogStreamHandler."""

    def setUp(self) -> None:
        """Set up logs decoder."""
        self._channel_id = 1
        self.client = client.Client.from_modules(
            callback_client.Impl(),
            [client.Channel(self._channel_id, lambda _: None)],
            [log_pb2],
        )

        self.captured_logs: list[Log] = []

        def decoded_log_handler(log: Log) -> None:
            self.captured_logs.append(log)

        log_decoder = LogStreamDecoder(
            decoded_log_handler=decoded_log_handler,
            source_name='source',
        )
        self.log_stream_handler = LogStreamHandler(
            self.client.channel(self._channel_id).rpcs, log_decoder
        )

    def _get_rpc_ids(self) -> packets.RpcIds:
        service = next(iter(self.client.services))
        method = next(iter(service.methods))

        # To handle unrequested log streams, packets' call Ids are set to
        # kOpenCallId.
        call_id = client.OPEN_CALL_ID
        return packets.RpcIds(self._channel_id, service.id, method.id, call_id)

    def test_listen_to_logs_subsequent_calls(self):
        """Test a stream of RPC Logs."""
        self.log_stream_handler.handle_log_stream_error = mock.Mock()
        self.log_stream_handler.handle_log_stream_completed = mock.Mock()
        self.log_stream_handler.listen_to_logs()

        self.assertIs(
            self.client.process_packet(
                _encode_server_stream_packet(
                    self._get_rpc_ids(),
                    log_pb2.LogEntries(
                        first_entry_sequence_id=0,
                        entries=[
                            log_pb2.LogEntry(message=b'message0'),
                            log_pb2.LogEntry(message=b'message1'),
                        ],
                    ),
                )
            ),
            Status.OK,
        )
        self.assertFalse(self.log_stream_handler.handle_log_stream_error.called)
        self.assertFalse(
            self.log_stream_handler.handle_log_stream_completed.called
        )
        self.assertEqual(len(self.captured_logs), 2)

        # A subsequent RPC packet should be handled successfully.
        self.assertIs(
            self.client.process_packet(
                _encode_server_stream_packet(
                    self._get_rpc_ids(),
                    log_pb2.LogEntries(
                        first_entry_sequence_id=2,
                        entries=[
                            log_pb2.LogEntry(message=b'message2'),
                            log_pb2.LogEntry(message=b'message3'),
                        ],
                    ),
                )
            ),
            Status.OK,
        )
        self.assertFalse(self.log_stream_handler.handle_log_stream_error.called)
        self.assertFalse(
            self.log_stream_handler.handle_log_stream_completed.called
        )
        self.assertEqual(len(self.captured_logs), 4)

    def test_log_stream_cancelled(self):
        """Tests that a cancelled log stream is not restarted."""
        self.log_stream_handler.handle_log_stream_error = mock.Mock()
        self.log_stream_handler.handle_log_stream_completed = mock.Mock()

        listen_function = _CallableWithCounter(
            self.log_stream_handler.listen_to_logs
        )
        self.log_stream_handler.listen_to_logs = listen_function
        self.log_stream_handler.listen_to_logs()

        # Send logs prior to cancellation.
        self.assertIs(
            self.client.process_packet(
                _encode_server_stream_packet(
                    self._get_rpc_ids(),
                    log_pb2.LogEntries(
                        first_entry_sequence_id=0,
                        entries=[
                            log_pb2.LogEntry(message=b'message0'),
                            log_pb2.LogEntry(message=b'message1'),
                        ],
                    ),
                )
            ),
            Status.OK,
        )
        self.assertIs(
            self.client.process_packet(_encode_cancel(self._get_rpc_ids())),
            Status.OK,
        )
        self.log_stream_handler.handle_log_stream_error.assert_called_once_with(
            Status.CANCELLED
        )
        self.assertFalse(
            self.log_stream_handler.handle_log_stream_completed.called
        )
        self.assertEqual(len(self.captured_logs), 2)
        self.assertEqual(listen_function.call_count(), 1)

    def test_log_stream_error_stream_restarted(self):
        """Tests that an error on the log stream restarts the stream."""
        self.log_stream_handler.handle_log_stream_completed = mock.Mock()

        error_handler = _CallableWithCounter(
            self.log_stream_handler.handle_log_stream_error
        )
        self.log_stream_handler.handle_log_stream_error = error_handler

        listen_function = _CallableWithCounter(
            self.log_stream_handler.listen_to_logs
        )
        self.log_stream_handler.listen_to_logs = listen_function
        self.log_stream_handler.listen_to_logs()

        # Send logs prior to cancellation.
        self.assertIs(
            self.client.process_packet(
                _encode_server_stream_packet(
                    self._get_rpc_ids(),
                    log_pb2.LogEntries(
                        first_entry_sequence_id=0,
                        entries=[
                            log_pb2.LogEntry(message=b'message0'),
                            log_pb2.LogEntry(message=b'message1'),
                        ],
                    ),
                )
            ),
            Status.OK,
        )
        self.assertIs(
            self.client.process_packet(_encode_error(self._get_rpc_ids())),
            Status.OK,
        )

        self.assertFalse(
            self.log_stream_handler.handle_log_stream_completed.called
        )
        self.assertEqual(len(self.captured_logs), 2)
        self.assertEqual(listen_function.call_count(), 2)
        self.assertEqual(error_handler.call_count(), 1)
        self.assertEqual(error_handler.calls[0].args, (Status.UNKNOWN,))

    def test_log_stream_completed_ok_stream_restarted(self):
        """Tests that when the log stream completes the stream is restarted."""
        self.log_stream_handler.handle_log_stream_error = mock.Mock()

        completion_handler = _CallableWithCounter(
            self.log_stream_handler.handle_log_stream_completed
        )
        self.log_stream_handler.handle_log_stream_completed = completion_handler

        listen_function = _CallableWithCounter(
            self.log_stream_handler.listen_to_logs
        )
        self.log_stream_handler.listen_to_logs = listen_function
        self.log_stream_handler.listen_to_logs()

        # Send logs prior to cancellation.
        self.assertIs(
            self.client.process_packet(
                _encode_server_stream_packet(
                    self._get_rpc_ids(),
                    log_pb2.LogEntries(
                        first_entry_sequence_id=0,
                        entries=[
                            log_pb2.LogEntry(message=b'message0'),
                            log_pb2.LogEntry(message=b'message1'),
                        ],
                    ),
                )
            ),
            Status.OK,
        )
        self.assertIs(
            self.client.process_packet(
                _encode_completed(self._get_rpc_ids(), Status.OK)
            ),
            Status.OK,
        )

        self.assertFalse(self.log_stream_handler.handle_log_stream_error.called)
        self.assertEqual(len(self.captured_logs), 2)
        self.assertEqual(listen_function.call_count(), 2)
        self.assertEqual(completion_handler.call_count(), 1)
        self.assertEqual(completion_handler.calls[0].args, (Status.OK,))

    def test_log_stream_completed_with_error_stream_restarted(self):
        """Tests that when the log stream completes the stream is restarted."""
        self.log_stream_handler.handle_log_stream_error = mock.Mock()

        completion_handler = _CallableWithCounter(
            self.log_stream_handler.handle_log_stream_completed
        )
        self.log_stream_handler.handle_log_stream_completed = completion_handler

        listen_function = _CallableWithCounter(
            self.log_stream_handler.listen_to_logs
        )
        self.log_stream_handler.listen_to_logs = listen_function
        self.log_stream_handler.listen_to_logs()

        # Send logs prior to cancellation.
        self.assertIs(
            self.client.process_packet(
                _encode_server_stream_packet(
                    self._get_rpc_ids(),
                    log_pb2.LogEntries(
                        first_entry_sequence_id=0,
                        entries=[
                            log_pb2.LogEntry(message=b'message0'),
                            log_pb2.LogEntry(message=b'message1'),
                        ],
                    ),
                )
            ),
            Status.OK,
        )
        self.assertIs(
            self.client.process_packet(
                _encode_completed(self._get_rpc_ids(), Status.UNKNOWN)
            ),
            Status.OK,
        )

        self.assertFalse(self.log_stream_handler.handle_log_stream_error.called)
        self.assertEqual(len(self.captured_logs), 2)
        self.assertEqual(listen_function.call_count(), 2)
        self.assertEqual(completion_handler.call_count(), 1)
        self.assertEqual(completion_handler.calls[0].args, (Status.UNKNOWN,))


if __name__ == '__main__':
    main()
