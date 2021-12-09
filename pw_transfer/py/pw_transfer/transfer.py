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
"""Classes for read and write transfers."""

import abc
import asyncio
from dataclasses import dataclass
import logging
import math
import threading
from typing import Any, Callable, Optional

from pw_status import Status
from pw_transfer.transfer_pb2 import Chunk

_LOG = logging.getLogger(__package__)


@dataclass(frozen=True)
class ProgressStats:
    bytes_sent: int
    bytes_confirmed_received: int
    total_size_bytes: Optional[int]

    def percent_received(self) -> float:
        if self.total_size_bytes is None:
            return math.nan

        return self.bytes_confirmed_received / self.total_size_bytes * 100

    def __str__(self) -> str:
        total = str(
            self.total_size_bytes) if self.total_size_bytes else 'unknown'
        return (f'{self.percent_received():5.1f}% ({self.bytes_sent} B sent, '
                f'{self.bytes_confirmed_received} B received of {total} B)')


ProgressCallback = Callable[[ProgressStats], Any]


class _Timer:
    """A timer which invokes a callback after a certain timeout."""
    def __init__(self, timeout_s: float, callback: Callable[[], Any]):
        self.timeout_s = timeout_s
        self._callback = callback
        self._task: Optional[asyncio.Task[Any]] = None

    def start(self, timeout_s: float = None) -> None:
        """Starts a new timer.

        If a timer is already running, it is stopped and a new timer started.
        This can be used to implement watchdog-like behavior, where a callback
        is invoked after some time without a kick.
        """
        self.stop()
        timeout_s = self.timeout_s if timeout_s is None else timeout_s
        self._task = asyncio.create_task(self._run(timeout_s))

    def stop(self) -> None:
        """Terminates a running timer."""
        if self._task is not None:
            self._task.cancel()
            self._task = None

    async def _run(self, timeout_s: float) -> None:
        await asyncio.sleep(timeout_s)
        self._task = None
        self._callback()


class Transfer(abc.ABC):
    """A client-side data transfer through a Manager.

    Subclasses are responsible for implementing all of the logic for their type
    of transfer, receiving messages from the server and sending the appropriate
    messages in response.
    """
    def __init__(self,
                 transfer_id: int,
                 send_chunk: Callable[[Chunk], None],
                 end_transfer: Callable[['Transfer'], None],
                 response_timeout_s: float,
                 initial_response_timeout_s: float,
                 max_retries: int,
                 progress_callback: ProgressCallback = None):
        self.id = transfer_id
        self.status = Status.OK
        self.done = threading.Event()

        self._send_chunk = send_chunk
        self._end_transfer = end_transfer

        self._retries = 0
        self._max_retries = max_retries
        self._response_timer = _Timer(response_timeout_s, self._on_timeout)
        self._initial_response_timeout_s = initial_response_timeout_s

        self._progress_callback = progress_callback

    async def begin(self) -> None:
        """Sends the initial chunk of the transfer."""
        self._send_chunk(self._initial_chunk())
        self._response_timer.start(self._initial_response_timeout_s)

    @property
    @abc.abstractmethod
    def data(self) -> bytes:
        """Returns the data read or written in this transfer."""

    @abc.abstractmethod
    def _initial_chunk(self) -> Chunk:
        """Returns the initial chunk to notify the sever of the transfer."""

    async def handle_chunk(self, chunk: Chunk) -> None:
        """Processes an incoming chunk from the server.

        Handles terminating chunks (i.e. those with a status) and forwards
        non-terminating chunks to handle_data_chunk.
        """
        self._response_timer.stop()
        self._retries = 0  # Received data from service, so reset the retries.

        _LOG.debug('Received chunk\n%s', str(chunk).rstrip())

        # Status chunks are only used to terminate a transfer. They do not
        # contain any data that requires processing.
        if chunk.HasField('status'):
            self.finish(Status(chunk.status))
            return

        await self._handle_data_chunk(chunk)

        # Start the timeout for the server to send a chunk in response.
        self._response_timer.start()

    @abc.abstractmethod
    async def _handle_data_chunk(self, chunk: Chunk) -> None:
        """Handles a chunk that contains or requests data."""

    @abc.abstractmethod
    def _retry_after_timeout(self) -> None:
        """Retries after a timeout occurs."""

    def _on_timeout(self) -> None:
        """Handles a timeout while waiting for a chunk."""
        if self.done.is_set():
            return

        self._retries += 1
        if self._retries > self._max_retries:
            self.finish(Status.DEADLINE_EXCEEDED)
            return

        _LOG.debug('Received no responses for %.3fs; retrying %d/%d',
                   self._response_timer.timeout_s, self._retries,
                   self._max_retries)
        self._retry_after_timeout()
        self._response_timer.start()

    def finish(self, status: Status, skip_callback: bool = False) -> None:
        """Ends the transfer with the specified status."""
        self._response_timer.stop()
        self.status = status

        if status.ok():
            total_size = len(self.data)
            self._update_progress(total_size, total_size, total_size)

        if not skip_callback:
            self._end_transfer(self)

        # Set done last so that the transfer has been fully cleaned up.
        self.done.set()

    def _update_progress(self, bytes_sent: int, bytes_confirmed_received: int,
                         total_size_bytes: Optional[int]) -> None:
        """Invokes the provided progress callback, if any, with the progress."""

        stats = ProgressStats(bytes_sent, bytes_confirmed_received,
                              total_size_bytes)
        _LOG.debug('Transfer %d progress: %s', self.id, stats)

        if self._progress_callback:
            self._progress_callback(stats)

    def _send_error(self, error: Status) -> None:
        """Sends an error chunk to the server and finishes the transfer."""
        self._send_chunk(Chunk(transfer_id=self.id, status=error.value))
        self.finish(error)


class WriteTransfer(Transfer):
    """A client -> server write transfer."""
    def __init__(
        self,
        transfer_id: int,
        data: bytes,
        send_chunk: Callable[[Chunk], None],
        end_transfer: Callable[[Transfer], None],
        response_timeout_s: float,
        initial_response_timeout_s: float,
        max_retries: int,
        progress_callback: ProgressCallback = None,
    ):
        super().__init__(transfer_id, send_chunk, end_transfer,
                         response_timeout_s, initial_response_timeout_s,
                         max_retries, progress_callback)
        self._data = data

        # Guard this class with a lock since a transfer parameters update might
        # arrive while responding to a prior update.
        self._lock = asyncio.Lock()
        self._offset = 0
        self._window_end_offset = 0
        self._max_chunk_size = 0
        self._chunk_delay_us: Optional[int] = None

        # The window ID increments for each parameters update.
        self._window_id = 0

        self._last_chunk = self._initial_chunk()

    @property
    def data(self) -> bytes:
        return self._data

    def _initial_chunk(self) -> Chunk:
        return Chunk(transfer_id=self.id)

    async def _handle_data_chunk(self, chunk: Chunk) -> None:
        """Processes an incoming chunk from the server.

        In a write transfer, the server only sends transfer parameter updates
        to the client. When a message is received, update local parameters and
        send data accordingly.
        """

        async with self._lock:
            self._window_id += 1
            window_id = self._window_id

            if not self._handle_parameters_update(chunk):
                return

            bytes_acknowledged = chunk.offset

        while True:
            if self._chunk_delay_us:
                await asyncio.sleep(self._chunk_delay_us / 1e6)

            async with self._lock:
                if self.done.is_set():
                    return

                if window_id != self._window_id:
                    _LOG.debug('Transfer %d: Skipping stale window', self.id)
                    return

                write_chunk = self._next_chunk()
                self._offset += len(write_chunk.data)
                sent_requested_bytes = self._offset == self._window_end_offset

            self._send_chunk(write_chunk)

            self._update_progress(self._offset, bytes_acknowledged,
                                  len(self.data))

            if sent_requested_bytes:
                break

        self._last_chunk = write_chunk

    def _handle_parameters_update(self, chunk: Chunk) -> bool:
        """Updates transfer state based on a transfer parameters update."""

        retransmit = True
        if chunk.HasField('type'):
            retransmit = chunk.type == Chunk.Type.PARAMETERS_RETRANSMIT

        if chunk.offset > len(self.data):
            # Bad offset; terminate the transfer.
            _LOG.error(
                'Transfer %d: server requested invalid offset %d (size %d)',
                self.id, chunk.offset, len(self.data))

            self._send_error(Status.OUT_OF_RANGE)
            return False

        if chunk.pending_bytes == 0:
            _LOG.error(
                'Transfer %d: service requested 0 bytes (invalid); aborting',
                self.id)
            self._send_error(Status.INTERNAL)
            return False

        if retransmit:
            # Check whether the client has sent a previous data offset, which
            # indicates that some chunks were lost in transmission.
            if chunk.offset < self._offset:
                _LOG.debug('Write transfer %d rolling back: offset %d from %d',
                           self.id, chunk.offset, self._offset)

            self._offset = chunk.offset

            # Retransmit is the default behavior for older versions of the
            # transfer protocol. The window_end_offset field is not guaranteed
            # to be set in these version, so it must be calculated.
            max_bytes_to_send = min(chunk.pending_bytes,
                                    len(self.data) - self._offset)
            self._window_end_offset = self._offset + max_bytes_to_send
        else:
            assert chunk.type == Chunk.Type.PARAMETERS_CONTINUE

            # Extend the window to the new end offset specified by the server.
            self._window_end_offset = min(chunk.window_end_offset,
                                          len(self.data))

        if chunk.HasField('max_chunk_size_bytes'):
            self._max_chunk_size = chunk.max_chunk_size_bytes

        if chunk.HasField('min_delay_microseconds'):
            self._chunk_delay_us = chunk.min_delay_microseconds

        return True

    def _retry_after_timeout(self) -> None:
        self._send_chunk(self._last_chunk)

    def _next_chunk(self) -> Chunk:
        """Returns the next Chunk message to send in the data transfer."""
        chunk = Chunk(transfer_id=self.id, offset=self._offset)
        max_bytes_in_chunk = min(self._max_chunk_size,
                                 self._window_end_offset - self._offset)

        chunk.data = self.data[self._offset:self._offset + max_bytes_in_chunk]

        # Mark the final chunk of the transfer.
        if len(self.data) - self._offset <= max_bytes_in_chunk:
            chunk.remaining_bytes = 0

        return chunk


class ReadTransfer(Transfer):
    """A client <- server read transfer.

    Although Python can effectively handle an unlimited transfer window, this
    client sets a conservative window and chunk size to avoid overloading the
    device. These are configurable in the constructor.
    """

    # The fractional position within a window at which a receive transfer should
    # extend its window size to minimize the amount of time the transmitter
    # spends blocked.
    #
    # For example, a divisor of 2 will extend the window when half of the
    # requested data has been received, a divisor of three will extend at a
    # third of the window, and so on.
    EXTEND_WINDOW_DIVISOR = 2

    def __init__(  # pylint: disable=too-many-arguments
            self,
            transfer_id: int,
            send_chunk: Callable[[Chunk], None],
            end_transfer: Callable[[Transfer], None],
            response_timeout_s: float,
            initial_response_timeout_s: float,
            max_retries: int,
            max_bytes_to_receive: int = 8192,
            max_chunk_size: int = 1024,
            chunk_delay_us: int = None,
            progress_callback: ProgressCallback = None):
        super().__init__(transfer_id, send_chunk, end_transfer,
                         response_timeout_s, initial_response_timeout_s,
                         max_retries, progress_callback)
        self._max_bytes_to_receive = max_bytes_to_receive
        self._max_chunk_size = max_chunk_size
        self._chunk_delay_us = chunk_delay_us

        self._remaining_transfer_size: Optional[int] = None
        self._data = bytearray()
        self._offset = 0
        self._pending_bytes = max_bytes_to_receive
        self._window_end_offset = max_bytes_to_receive

    @property
    def data(self) -> bytes:
        """Returns an immutable copy of the data that has been read."""
        return bytes(self._data)

    def _initial_chunk(self) -> Chunk:
        return self._transfer_parameters()

    async def _handle_data_chunk(self, chunk: Chunk) -> None:
        """Processes an incoming chunk from the server.

        In a read transfer, the client receives data chunks from the server.
        Once all pending data is received, the transfer parameters are updated.
        """

        if chunk.offset != self._offset:
            # Initially, the transfer service only supports in-order transfers.
            # If data is received out of order, request that the server
            # retransmit from the previous offset.
            self._send_chunk(self._transfer_parameters())
            return

        self._data += chunk.data
        self._pending_bytes -= len(chunk.data)
        self._offset += len(chunk.data)

        if chunk.HasField('remaining_bytes'):
            if chunk.remaining_bytes == 0:
                # No more data to read. Acknowledge receipt and finish.
                self._send_chunk(
                    Chunk(transfer_id=self.id, status=Status.OK.value))
                self.finish(Status.OK)
                return

            # The server may indicate if the amount of remaining data is known.
            self._remaining_transfer_size = chunk.remaining_bytes
        elif self._remaining_transfer_size is not None:
            # Update the remaining transfer size, if it is known.
            self._remaining_transfer_size -= len(chunk.data)

            # If the transfer size drops to zero, the estimate was inaccurate.
            if self._remaining_transfer_size <= 0:
                self._remaining_transfer_size = None

        total_size = None if self._remaining_transfer_size is None else (
            self._remaining_transfer_size + self._offset)
        self._update_progress(self._offset, self._offset, total_size)

        remaining_window_size = self._window_end_offset - self._offset
        extend_window = (remaining_window_size <= self._max_bytes_to_receive /
                         ReadTransfer.EXTEND_WINDOW_DIVISOR)

        if self._pending_bytes == 0:
            # All pending data was received. Send out a new parameters chunk for
            # the next block.
            self._send_chunk(self._transfer_parameters())
        elif extend_window:
            self._send_chunk(self._transfer_parameters(extend=True))

    def _retry_after_timeout(self) -> None:
        self._send_chunk(self._transfer_parameters())

    def _transfer_parameters(self, extend: bool = False) -> Chunk:
        """Sends an updated transfer parameters chunk to the server."""

        self._pending_bytes = self._max_bytes_to_receive
        self._window_end_offset = self._offset + self._max_bytes_to_receive

        chunk_type = (Chunk.Type.PARAMETERS_CONTINUE
                      if extend else Chunk.Type.PARAMETERS_RETRANSMIT)

        chunk = Chunk(transfer_id=self.id,
                      pending_bytes=self._pending_bytes,
                      window_end_offset=self._window_end_offset,
                      max_chunk_size_bytes=self._max_chunk_size,
                      offset=self._offset,
                      type=chunk_type)

        if self._chunk_delay_us:
            chunk.min_delay_microseconds = self._chunk_delay_us

        return chunk
