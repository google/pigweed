#!/usr/bin/env python3
# Copyright 2022 The Pigweed Authors
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
"""Tests for the transfer service client."""

import enum
import math
import unittest
from typing import Iterable, List

from pw_status import Status
from pw_rpc import callback_client, client, ids, packets
from pw_rpc.internal import packet_pb2

import pw_transfer
from pw_transfer.transfer_pb2 import Chunk

_TRANSFER_SERVICE_ID = ids.calculate('pw.transfer.Transfer')

# If the default timeout is too short, some tests become flaky on Windows.
DEFAULT_TIMEOUT_S = 0.3


class _Method(enum.Enum):
    READ = ids.calculate('Read')
    WRITE = ids.calculate('Write')


class TransferManagerTest(unittest.TestCase):
    """Tests for the transfer manager."""
    def setUp(self) -> None:
        self._client = client.Client.from_modules(
            callback_client.Impl(), [client.Channel(1, self._handle_request)],
            (pw_transfer.transfer_pb2, ))
        self._service = self._client.channel(1).rpcs.pw.transfer.Transfer

        self._sent_chunks: List[Chunk] = []
        self._packets_to_send: List[List[bytes]] = []

    def _enqueue_server_responses(
            self, method: _Method,
            responses: Iterable[Iterable[Chunk]]) -> None:
        for group in responses:
            serialized_group = []
            for response in group:
                serialized_group.append(
                    packet_pb2.RpcPacket(
                        type=packet_pb2.PacketType.SERVER_STREAM,
                        channel_id=1,
                        service_id=_TRANSFER_SERVICE_ID,
                        method_id=method.value,
                        status=Status.OK.value,
                        payload=response.SerializeToString()).
                    SerializeToString())
            self._packets_to_send.append(serialized_group)

    def _enqueue_server_error(self, method: _Method, error: Status) -> None:
        self._packets_to_send.append([
            packet_pb2.RpcPacket(type=packet_pb2.PacketType.SERVER_ERROR,
                                 channel_id=1,
                                 service_id=_TRANSFER_SERVICE_ID,
                                 method_id=method.value,
                                 status=error.value).SerializeToString()
        ])

    def _handle_request(self, data: bytes) -> None:
        packet = packets.decode(data)
        if packet.type is not packet_pb2.PacketType.CLIENT_STREAM:
            return

        chunk = Chunk()
        chunk.MergeFromString(packet.payload)
        self._sent_chunks.append(chunk)

        if self._packets_to_send:
            responses = self._packets_to_send.pop(0)
            for response in responses:
                self._client.process_packet(response)

    def _received_data(self) -> bytearray:
        data = bytearray()
        for chunk in self._sent_chunks:
            data.extend(chunk.data)
        return data

    def test_read_transfer_basic(self):
        manager = pw_transfer.Manager(
            self._service, default_response_timeout_s=DEFAULT_TIMEOUT_S)

        self._enqueue_server_responses(
            _Method.READ,
            ((Chunk(transfer_id=3, offset=0, data=b'abc',
                    remaining_bytes=0), ), ),
        )

        data = manager.read(3)
        self.assertEqual(data, b'abc')
        self.assertEqual(len(self._sent_chunks), 2)
        self.assertTrue(self._sent_chunks[-1].HasField('status'))
        self.assertEqual(self._sent_chunks[-1].status, 0)

    def test_read_transfer_multichunk(self) -> None:
        manager = pw_transfer.Manager(
            self._service, default_response_timeout_s=DEFAULT_TIMEOUT_S)

        self._enqueue_server_responses(
            _Method.READ,
            ((
                Chunk(transfer_id=3, offset=0, data=b'abc', remaining_bytes=3),
                Chunk(transfer_id=3, offset=3, data=b'def', remaining_bytes=0),
            ), ),
        )

        data = manager.read(3)
        self.assertEqual(data, b'abcdef')
        self.assertEqual(len(self._sent_chunks), 2)
        self.assertTrue(self._sent_chunks[-1].HasField('status'))
        self.assertEqual(self._sent_chunks[-1].status, 0)

    def test_read_transfer_progress_callback(self) -> None:
        manager = pw_transfer.Manager(
            self._service, default_response_timeout_s=DEFAULT_TIMEOUT_S)

        self._enqueue_server_responses(
            _Method.READ,
            ((
                Chunk(transfer_id=3, offset=0, data=b'abc', remaining_bytes=3),
                Chunk(transfer_id=3, offset=3, data=b'def', remaining_bytes=0),
            ), ),
        )

        progress: List[pw_transfer.ProgressStats] = []

        data = manager.read(3, progress.append)
        self.assertEqual(data, b'abcdef')
        self.assertEqual(len(self._sent_chunks), 2)
        self.assertTrue(self._sent_chunks[-1].HasField('status'))
        self.assertEqual(self._sent_chunks[-1].status, 0)
        self.assertEqual(progress, [
            pw_transfer.ProgressStats(3, 3, 6),
            pw_transfer.ProgressStats(6, 6, 6),
        ])

    def test_read_transfer_retry_bad_offset(self) -> None:
        """Server responds with an unexpected offset in a read transfer."""
        manager = pw_transfer.Manager(
            self._service, default_response_timeout_s=DEFAULT_TIMEOUT_S)

        self._enqueue_server_responses(
            _Method.READ,
            (
                (
                    Chunk(transfer_id=3,
                          offset=0,
                          data=b'123',
                          remaining_bytes=6),

                    # Incorrect offset; expecting 3.
                    Chunk(transfer_id=3,
                          offset=1,
                          data=b'456',
                          remaining_bytes=3),
                ),
                (
                    Chunk(transfer_id=3,
                          offset=3,
                          data=b'456',
                          remaining_bytes=3),
                    Chunk(transfer_id=3,
                          offset=6,
                          data=b'789',
                          remaining_bytes=0),
                ),
            ))

        data = manager.read(3)
        self.assertEqual(data, b'123456789')

        # Two transfer parameter requests should have been sent.
        self.assertEqual(len(self._sent_chunks), 3)
        self.assertTrue(self._sent_chunks[-1].HasField('status'))
        self.assertEqual(self._sent_chunks[-1].status, 0)

    def test_read_transfer_retry_timeout(self) -> None:
        """Server doesn't respond to read transfer parameters."""
        manager = pw_transfer.Manager(
            self._service, default_response_timeout_s=DEFAULT_TIMEOUT_S)

        self._enqueue_server_responses(
            _Method.READ,
            (
                (),  # Send nothing in response to the initial parameters.
                (Chunk(transfer_id=3, offset=0, data=b'xyz',
                       remaining_bytes=0), ),
            ))

        data = manager.read(3)
        self.assertEqual(data, b'xyz')

        # Two transfer parameter requests should have been sent.
        self.assertEqual(len(self._sent_chunks), 3)
        self.assertTrue(self._sent_chunks[-1].HasField('status'))
        self.assertEqual(self._sent_chunks[-1].status, 0)

    def test_read_transfer_timeout(self) -> None:
        manager = pw_transfer.Manager(
            self._service, default_response_timeout_s=DEFAULT_TIMEOUT_S)

        with self.assertRaises(pw_transfer.Error) as context:
            manager.read(27)

        exception = context.exception
        self.assertEqual(exception.transfer_id, 27)
        self.assertEqual(exception.status, Status.DEADLINE_EXCEEDED)

        # The client should have sent four transfer parameters requests: one
        # initial, and three retries.
        self.assertEqual(len(self._sent_chunks), 4)

    def test_read_transfer_error(self) -> None:
        manager = pw_transfer.Manager(
            self._service, default_response_timeout_s=DEFAULT_TIMEOUT_S)

        self._enqueue_server_responses(
            _Method.READ,
            ((Chunk(transfer_id=31, status=Status.NOT_FOUND.value), ), ),
        )

        with self.assertRaises(pw_transfer.Error) as context:
            manager.read(31)

        exception = context.exception
        self.assertEqual(exception.transfer_id, 31)
        self.assertEqual(exception.status, Status.NOT_FOUND)

    def test_read_transfer_server_error(self) -> None:
        manager = pw_transfer.Manager(
            self._service, default_response_timeout_s=DEFAULT_TIMEOUT_S)

        self._enqueue_server_error(_Method.READ, Status.NOT_FOUND)

        with self.assertRaises(pw_transfer.Error) as context:
            manager.read(31)

        exception = context.exception
        self.assertEqual(exception.transfer_id, 31)
        self.assertEqual(exception.status, Status.INTERNAL)

    def test_write_transfer_basic(self) -> None:
        manager = pw_transfer.Manager(
            self._service, default_response_timeout_s=DEFAULT_TIMEOUT_S)

        self._enqueue_server_responses(
            _Method.WRITE,
            (
                (Chunk(transfer_id=4,
                       offset=0,
                       pending_bytes=32,
                       max_chunk_size_bytes=8), ),
                (Chunk(transfer_id=4, status=Status.OK.value), ),
            ),
        )

        manager.write(4, b'hello')
        self.assertEqual(len(self._sent_chunks), 2)
        self.assertEqual(self._received_data(), b'hello')

    def test_write_transfer_max_chunk_size(self) -> None:
        manager = pw_transfer.Manager(
            self._service, default_response_timeout_s=DEFAULT_TIMEOUT_S)

        self._enqueue_server_responses(
            _Method.WRITE,
            (
                (Chunk(transfer_id=4,
                       offset=0,
                       pending_bytes=32,
                       max_chunk_size_bytes=8), ),
                (),
                (Chunk(transfer_id=4, status=Status.OK.value), ),
            ),
        )

        manager.write(4, b'hello world')
        self.assertEqual(len(self._sent_chunks), 3)
        self.assertEqual(self._received_data(), b'hello world')
        self.assertEqual(self._sent_chunks[1].data, b'hello wo')
        self.assertEqual(self._sent_chunks[2].data, b'rld')

    def test_write_transfer_multiple_parameters(self) -> None:
        manager = pw_transfer.Manager(
            self._service, default_response_timeout_s=DEFAULT_TIMEOUT_S)

        self._enqueue_server_responses(
            _Method.WRITE,
            (
                (Chunk(transfer_id=4,
                       offset=0,
                       pending_bytes=8,
                       max_chunk_size_bytes=8), ),
                (Chunk(transfer_id=4,
                       offset=8,
                       pending_bytes=8,
                       max_chunk_size_bytes=8), ),
                (Chunk(transfer_id=4, status=Status.OK.value), ),
            ),
        )

        manager.write(4, b'data to write')
        self.assertEqual(len(self._sent_chunks), 3)
        self.assertEqual(self._received_data(), b'data to write')
        self.assertEqual(self._sent_chunks[1].data, b'data to ')
        self.assertEqual(self._sent_chunks[2].data, b'write')

    def test_write_transfer_progress_callback(self) -> None:
        manager = pw_transfer.Manager(
            self._service, default_response_timeout_s=DEFAULT_TIMEOUT_S)

        self._enqueue_server_responses(
            _Method.WRITE,
            (
                (Chunk(transfer_id=4,
                       offset=0,
                       pending_bytes=8,
                       max_chunk_size_bytes=8), ),
                (Chunk(transfer_id=4,
                       offset=8,
                       pending_bytes=8,
                       max_chunk_size_bytes=8), ),
                (Chunk(transfer_id=4, status=Status.OK.value), ),
            ),
        )

        progress: List[pw_transfer.ProgressStats] = []

        manager.write(4, b'data to write', progress.append)
        self.assertEqual(len(self._sent_chunks), 3)
        self.assertEqual(self._received_data(), b'data to write')
        self.assertEqual(self._sent_chunks[1].data, b'data to ')
        self.assertEqual(self._sent_chunks[2].data, b'write')
        self.assertEqual(progress, [
            pw_transfer.ProgressStats(8, 0, 13),
            pw_transfer.ProgressStats(13, 8, 13),
            pw_transfer.ProgressStats(13, 13, 13)
        ])

    def test_write_transfer_rewind(self) -> None:
        """Write transfer in which the server re-requests an earlier offset."""
        manager = pw_transfer.Manager(
            self._service, default_response_timeout_s=DEFAULT_TIMEOUT_S)

        self._enqueue_server_responses(
            _Method.WRITE,
            (
                (Chunk(transfer_id=4,
                       offset=0,
                       pending_bytes=8,
                       max_chunk_size_bytes=8), ),
                (Chunk(transfer_id=4,
                       offset=8,
                       pending_bytes=8,
                       max_chunk_size_bytes=8), ),
                (
                    Chunk(
                        transfer_id=4,
                        offset=4,  # rewind
                        pending_bytes=8,
                        max_chunk_size_bytes=8), ),
                (
                    Chunk(
                        transfer_id=4,
                        offset=12,
                        pending_bytes=16,  # update max size
                        max_chunk_size_bytes=16), ),
                (Chunk(transfer_id=4, status=Status.OK.value), ),
            ),
        )

        manager.write(4, b'pigweed data transfer')
        self.assertEqual(len(self._sent_chunks), 5)
        self.assertEqual(self._sent_chunks[1].data, b'pigweed ')
        self.assertEqual(self._sent_chunks[2].data, b'data tra')
        self.assertEqual(self._sent_chunks[3].data, b'eed data')
        self.assertEqual(self._sent_chunks[4].data, b' transfer')

    def test_write_transfer_bad_offset(self) -> None:
        manager = pw_transfer.Manager(
            self._service, default_response_timeout_s=DEFAULT_TIMEOUT_S)

        self._enqueue_server_responses(
            _Method.WRITE,
            (
                (Chunk(transfer_id=4,
                       offset=0,
                       pending_bytes=8,
                       max_chunk_size_bytes=8), ),
                (
                    Chunk(
                        transfer_id=4,
                        offset=100,  # larger offset than data
                        pending_bytes=8,
                        max_chunk_size_bytes=8), ),
                (Chunk(transfer_id=4, status=Status.OK.value), ),
            ),
        )

        with self.assertRaises(pw_transfer.Error) as context:
            manager.write(4, b'small data')

        exception = context.exception
        self.assertEqual(exception.transfer_id, 4)
        self.assertEqual(exception.status, Status.OUT_OF_RANGE)

    def test_write_transfer_error(self) -> None:
        manager = pw_transfer.Manager(
            self._service, default_response_timeout_s=DEFAULT_TIMEOUT_S)

        self._enqueue_server_responses(
            _Method.WRITE,
            ((Chunk(transfer_id=21, status=Status.UNAVAILABLE.value), ), ),
        )

        with self.assertRaises(pw_transfer.Error) as context:
            manager.write(21, b'no write')

        exception = context.exception
        self.assertEqual(exception.transfer_id, 21)
        self.assertEqual(exception.status, Status.UNAVAILABLE)

    def test_write_transfer_server_error(self) -> None:
        manager = pw_transfer.Manager(
            self._service, default_response_timeout_s=DEFAULT_TIMEOUT_S)

        self._enqueue_server_error(_Method.WRITE, Status.NOT_FOUND)

        with self.assertRaises(pw_transfer.Error) as context:
            manager.write(21, b'server error')

        exception = context.exception
        self.assertEqual(exception.transfer_id, 21)
        self.assertEqual(exception.status, Status.INTERNAL)

    def test_write_transfer_timeout_after_initial_chunk(self) -> None:
        manager = pw_transfer.Manager(self._service,
                                      default_response_timeout_s=0.001,
                                      max_retries=2)

        with self.assertRaises(pw_transfer.Error) as context:
            manager.write(22, b'no server response!')

        self.assertEqual(
            self._sent_chunks,
            [
                Chunk(transfer_id=22,
                      type=Chunk.Type.TRANSFER_START),  # initial chunk
                Chunk(transfer_id=22,
                      type=Chunk.Type.TRANSFER_START),  # retry 1
                Chunk(transfer_id=22,
                      type=Chunk.Type.TRANSFER_START),  # retry 2
            ])

        exception = context.exception
        self.assertEqual(exception.transfer_id, 22)
        self.assertEqual(exception.status, Status.DEADLINE_EXCEEDED)

    def test_write_transfer_timeout_after_intermediate_chunk(self) -> None:
        """Tests write transfers that timeout after the initial chunk."""
        manager = pw_transfer.Manager(
            self._service,
            default_response_timeout_s=DEFAULT_TIMEOUT_S,
            max_retries=2)

        self._enqueue_server_responses(
            _Method.WRITE,
            [[Chunk(transfer_id=22, pending_bytes=10, max_chunk_size_bytes=5)]
             ])

        with self.assertRaises(pw_transfer.Error) as context:
            manager.write(22, b'0123456789')

        last_data_chunk = Chunk(transfer_id=22,
                                data=b'56789',
                                offset=5,
                                remaining_bytes=0,
                                type=Chunk.Type.TRANSFER_DATA)

        self.assertEqual(
            self._sent_chunks,
            [
                Chunk(transfer_id=22, type=Chunk.Type.TRANSFER_START),
                Chunk(transfer_id=22,
                      data=b'01234',
                      type=Chunk.Type.TRANSFER_DATA),
                last_data_chunk,  # last chunk
                last_data_chunk,  # retry 1
                last_data_chunk,  # retry 2
            ])

        exception = context.exception
        self.assertEqual(exception.transfer_id, 22)
        self.assertEqual(exception.status, Status.DEADLINE_EXCEEDED)

    def test_write_zero_pending_bytes_is_internal_error(self) -> None:
        manager = pw_transfer.Manager(
            self._service, default_response_timeout_s=DEFAULT_TIMEOUT_S)

        self._enqueue_server_responses(
            _Method.WRITE,
            ((Chunk(transfer_id=23, pending_bytes=0), ), ),
        )

        with self.assertRaises(pw_transfer.Error) as context:
            manager.write(23, b'no write')

        exception = context.exception
        self.assertEqual(exception.transfer_id, 23)
        self.assertEqual(exception.status, Status.INTERNAL)


class ProgressStatsTest(unittest.TestCase):
    def test_received_percent_known_total(self) -> None:
        self.assertEqual(
            pw_transfer.ProgressStats(75, 0, 100).percent_received(), 0.0)
        self.assertEqual(
            pw_transfer.ProgressStats(75, 50, 100).percent_received(), 50.0)
        self.assertEqual(
            pw_transfer.ProgressStats(100, 100, 100).percent_received(), 100.0)

    def test_received_percent_unknown_total(self) -> None:
        self.assertTrue(
            math.isnan(
                pw_transfer.ProgressStats(75, 50, None).percent_received()))
        self.assertTrue(
            math.isnan(
                pw_transfer.ProgressStats(100, 100, None).percent_received()))

    def test_str_known_total(self) -> None:
        stats = str(pw_transfer.ProgressStats(75, 50, 100))
        self.assertIn('75', stats)
        self.assertIn('50', stats)
        self.assertIn('100', stats)

    def test_str_unknown_total(self) -> None:
        stats = str(pw_transfer.ProgressStats(75, 50, None))
        self.assertIn('75', stats)
        self.assertIn('50', stats)
        self.assertIn('unknown', stats)


if __name__ == '__main__':
    unittest.main()
