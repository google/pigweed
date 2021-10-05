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
"""Provides a simple interface for transferring bulk data over pw_rpc."""

import abc
import asyncio
from dataclasses import dataclass
import logging
import threading
from typing import Any, Callable, Dict, Optional, Union

from pw_rpc.callback_client import BidirectionalStreamingCall
from pw_status import Status
from pw_transfer.transfer_pb2 import Chunk

_LOG = logging.getLogger(__package__)


@dataclass(frozen=True)
class ProgressStats:
    total_size_bytes: Optional[int]
    bytes_confirmed_received: int
    bytes_sent: int

    def __str__(self) -> str:
        total = str(
            self.total_size_bytes) if self.total_size_bytes else 'unknown'
        return f'{self.bytes_confirmed_received} / {total}'


ProgressCallback = Callable[[ProgressStats], Any]


class _Timer:
    """A timer which invokes a callback after a certain timeout."""
    def __init__(self, timeout_s: float, callback: Callable[[], Any]):
        self.timeout_s = timeout_s
        self._callback = callback
        self._task: Optional[asyncio.Task[Any]] = None

    def start(self) -> None:
        """Starts a new timer.

        If a timer is already running, it is stopped and a new timer started.
        This can be used to implement watchdog-like behavior, where a callback
        is invoked after some time without a kick.
        """
        self.stop()
        self._task = asyncio.create_task(self._run(self.timeout_s))

    def stop(self) -> None:
        """Terminates a running timer."""
        if self._task is not None:
            self._task.cancel()
            self._task = None

    async def _run(self, timeout_s: float) -> None:
        await asyncio.sleep(timeout_s)
        self._task = None
        self._callback()


class _Transfer(abc.ABC):
    """A client-side data transfer through a Manager.

    Subclasses are responsible for implementing all of the logic for their type
    of transfer, receiving messages from the server and sending the appropriate
    messages in response.
    """
    def __init__(self,
                 transfer_id: int,
                 send_chunk: Callable[[Chunk], None],
                 end_transfer: Callable[['_Transfer'], None],
                 response_timeout_s: float,
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

        self._progress_callback = progress_callback

    async def begin(self) -> None:
        """Sends the initial chunk of the transfer."""
        self._send_chunk(self._initial_chunk())
        self._response_timer.start()

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

        stats = ProgressStats(total_size_bytes, bytes_confirmed_received,
                              bytes_sent)
        _LOG.debug('Transfer %d progress: %s', self.id, stats)

        if self._progress_callback:
            self._progress_callback(stats)

    def _send_error(self, error: Status) -> None:
        """Sends an error chunk to the server and finishes the transfer."""
        self._send_chunk(Chunk(transfer_id=self.id, status=error.value))
        self.finish(error)


class _WriteTransfer(_Transfer):
    """A client -> server write transfer."""
    def __init__(
        self,
        transfer_id: int,
        data: bytes,
        send_chunk: Callable[[Chunk], None],
        end_transfer: Callable[[_Transfer], None],
        response_timeout_s: float,
        max_retries: int,
        progress_callback: ProgressCallback = None,
    ):
        super().__init__(transfer_id, send_chunk, end_transfer,
                         response_timeout_s, max_retries, progress_callback)
        self._data = data

        # Guard this class with a lock since a transfer parameters update might
        # arrive while responding to a prior update.
        self._lock = asyncio.Lock()
        self._offset = 0
        self._max_bytes_to_send = 0
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
                if window_id != self._window_id:
                    _LOG.debug('Transfer %d: Skipping stale window')
                    return

                write_chunk = self._next_chunk()
                self._offset += len(write_chunk.data)
                self._max_bytes_to_send -= len(write_chunk.data)
                sent_requested_bytes = self._max_bytes_to_send == 0

            self._send_chunk(write_chunk)

            self._update_progress(self._offset, bytes_acknowledged,
                                  len(self.data))

            if sent_requested_bytes:
                break

        self._last_chunk = write_chunk

    def _handle_parameters_update(self, chunk: Chunk) -> bool:
        """Updates transfer state based on a transfer parameters update."""

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

        # Check whether the client has sent a previous data offset, which
        # indicates that some chunks were lost in transmission.
        if chunk.offset < self._offset:
            _LOG.debug('Write transfer %d rolling back to offset %d from %d',
                       self.id, chunk.offset, self._offset)

        self._offset = chunk.offset
        self._max_bytes_to_send = min(chunk.pending_bytes,
                                      len(self.data) - self._offset)

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
        max_bytes_in_chunk = min(self._max_chunk_size, self._max_bytes_to_send)

        chunk.data = self.data[self._offset:self._offset + max_bytes_in_chunk]

        # Mark the final chunk of the transfer.
        if len(self.data) - self._offset <= max_bytes_in_chunk:
            chunk.remaining_bytes = 0

        return chunk


class _ReadTransfer(_Transfer):
    """A client <- server read transfer.

    Although Python can effectively handle an unlimited transfer window, this
    client sets a conservative window and chunk size to avoid overloading the
    device. These are configurable in the constructor.
    """
    def __init__(self,
                 transfer_id: int,
                 send_chunk: Callable[[Chunk], None],
                 end_transfer: Callable[[_Transfer], None],
                 response_timeout_s: float,
                 max_retries: int,
                 max_bytes_to_receive: int = 8192,
                 max_chunk_size: int = 1024,
                 chunk_delay_us: int = None,
                 progress_callback: ProgressCallback = None):
        super().__init__(transfer_id, send_chunk, end_transfer,
                         response_timeout_s, max_retries, progress_callback)
        self._max_bytes_to_receive = max_bytes_to_receive
        self._max_chunk_size = max_chunk_size
        self._chunk_delay_us = chunk_delay_us

        self._remaining_transfer_size: Optional[int] = None
        self._data = bytearray()
        self._offset = 0
        self._pending_bytes = max_bytes_to_receive

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
            self._pending_bytes = 0
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

        if self._pending_bytes == 0:
            # All pending data was received. Send out a new parameters chunk for
            # the next block.
            self._send_chunk(self._transfer_parameters())

    def _retry_after_timeout(self) -> None:
        self._send_chunk(self._transfer_parameters())

    def _transfer_parameters(self) -> Chunk:
        """Sends an updated transfer parameters chunk to the server."""

        self._pending_bytes = self._max_bytes_to_receive

        chunk = Chunk(transfer_id=self.id,
                      pending_bytes=self._pending_bytes,
                      max_chunk_size_bytes=self._max_chunk_size,
                      offset=self._offset)

        if self._chunk_delay_us:
            chunk.min_delay_microseconds = self._chunk_delay_us

        return chunk


_TransferDict = Dict[int, _Transfer]


class Manager:  # pylint: disable=too-many-instance-attributes
    """A manager for transmitting data through an RPC TransferService.

    This should be initialized with an active Manager over an RPC channel. Only
    one instance of this class should exist for a configured RPC TransferService
    -- the Manager supports multiple simultaneous transfers.

    When created, a Manager starts a separate thread in which transfer
    communications and events are handled.
    """
    def __init__(self,
                 rpc_transfer_service,
                 default_response_timeout_s: float = 2.0,
                 max_retries: int = 3):
        """Initializes a Manager on top of a TransferService."""
        self._service: Any = rpc_transfer_service
        self._default_response_timeout_s = default_response_timeout_s
        self._max_retries = max_retries

        # Ongoing transfers in the service by ID.
        self._read_transfers: _TransferDict = {}
        self._write_transfers: _TransferDict = {}

        # RPC streams for read and write transfers. These are shareable by
        # multiple transfers of the same type.
        self._read_stream: Optional[BidirectionalStreamingCall] = None
        self._write_stream: Optional[BidirectionalStreamingCall] = None

        self._loop = asyncio.new_event_loop()

        # Queues are used for communication between the Manager context and the
        # dedicated asyncio transfer thread.
        self._new_transfer_queue: asyncio.Queue = asyncio.Queue()
        self._read_chunk_queue: asyncio.Queue = asyncio.Queue()
        self._write_chunk_queue: asyncio.Queue = asyncio.Queue()
        self._quit_event = asyncio.Event()

        self._thread = threading.Thread(target=self._start_event_loop_thread,
                                        daemon=True)

        self._thread.start()

    def __del__(self):
        # Notify the thread that the transfer manager is being destroyed and
        # wait for it to exit.
        if self._thread.is_alive():
            self._loop.call_soon_threadsafe(self._quit_event.set)
            self._thread.join()

    def read(self,
             transfer_id: int,
             progress_callback: ProgressCallback = None) -> bytes:
        """Receives ("downloads") data from the server.

        Raises:
          Error: the transfer failed to complete
        """

        if transfer_id in self._read_transfers:
            raise ValueError(f'Read transfer {transfer_id} already exists')

        transfer = _ReadTransfer(transfer_id,
                                 self._send_read_chunk,
                                 self._end_read_transfer,
                                 self._default_response_timeout_s,
                                 self._max_retries,
                                 progress_callback=progress_callback)
        self._start_read_transfer(transfer)

        transfer.done.wait()

        if not transfer.status.ok():
            raise Error(transfer.id, transfer.status)

        return transfer.data

    def write(self,
              transfer_id: int,
              data: Union[bytes, str],
              progress_callback: ProgressCallback = None) -> None:
        """Transmits ("uploads") data to the server.

        Args:
          transfer_id: ID of the write transfer
          data: Data to send to the server.
          progress_callback: Optional callback periodically invoked throughout
              the transfer with the transfer state. Can be used to provide user-
              facing status updates such as progress bars.

        Raises:
          Error: the transfer failed to complete
        """

        if isinstance(data, str):
            data = data.encode()

        if transfer_id in self._write_transfers:
            raise ValueError(f'Write transfer {transfer_id} already exists')

        transfer = _WriteTransfer(transfer_id,
                                  data,
                                  self._send_write_chunk,
                                  self._end_write_transfer,
                                  self._default_response_timeout_s,
                                  self._max_retries,
                                  progress_callback=progress_callback)
        self._start_write_transfer(transfer)

        transfer.done.wait()

        if not transfer.status.ok():
            raise Error(transfer.id, transfer.status)

    def _send_read_chunk(self, chunk: Chunk) -> None:
        assert self._read_stream is not None
        self._read_stream.send(chunk)

    def _send_write_chunk(self, chunk: Chunk) -> None:
        assert self._write_stream is not None
        self._write_stream.send(chunk)

    def _start_event_loop_thread(self):
        """Entry point for event loop thread that starts an asyncio context."""
        asyncio.set_event_loop(self._loop)

        # Recreate the async communication channels in the context of the
        # running event loop.
        self._new_transfer_queue = asyncio.Queue()
        self._read_chunk_queue = asyncio.Queue()
        self._write_chunk_queue = asyncio.Queue()
        self._quit_event = asyncio.Event()

        self._loop.create_task(self._transfer_event_loop())
        self._loop.run_forever()

    async def _transfer_event_loop(self):
        """Main async event loop."""
        exit_thread = self._loop.create_task(self._quit_event.wait())
        new_transfer = self._loop.create_task(self._new_transfer_queue.get())
        read_chunk = self._loop.create_task(self._read_chunk_queue.get())
        write_chunk = self._loop.create_task(self._write_chunk_queue.get())

        while not self._quit_event.is_set():
            # Perform a select(2)-like wait for one of several events to occur.
            done, _ = await asyncio.wait(
                (exit_thread, new_transfer, read_chunk, write_chunk),
                return_when=asyncio.FIRST_COMPLETED)

            if exit_thread in done:
                break

            if new_transfer in done:
                await new_transfer.result().begin()
                new_transfer = self._loop.create_task(
                    self._new_transfer_queue.get())

            if read_chunk in done:
                self._loop.create_task(
                    self._handle_chunk(self._read_transfers,
                                       read_chunk.result()))
                read_chunk = self._loop.create_task(
                    self._read_chunk_queue.get())

            if write_chunk in done:
                self._loop.create_task(
                    self._handle_chunk(self._write_transfers,
                                       write_chunk.result()))
                write_chunk = self._loop.create_task(
                    self._write_chunk_queue.get())

        self._loop.stop()

    @staticmethod
    async def _handle_chunk(transfers: _TransferDict, chunk: Chunk) -> None:
        """Processes an incoming chunk from a stream.

        The chunk is dispatched to an active transfer based on its ID. If the
        transfer indicates that it is complete, the provided completion callback
        is invoked.
        """

        try:
            transfer = transfers[chunk.transfer_id]
        except KeyError:
            _LOG.error(
                'TransferManager received chunk for unknown transfer %d',
                chunk.transfer_id)
            # TODO(frolv): What should be done here, if anything?
            return

        await transfer.handle_chunk(chunk)

    def _open_read_stream(self) -> None:
        self._read_stream = self._service.Read.invoke(
            lambda _, chunk: self._loop.call_soon_threadsafe(
                self._read_chunk_queue.put_nowait, chunk),
            on_error=lambda _, status: self._on_read_error(status))

    def _on_read_error(self, status: Status) -> None:
        """Callback for an RPC error in the read stream."""

        if status is Status.FAILED_PRECONDITION:
            # FAILED_PRECONDITION indicates that the stream packet was not
            # recognized as the stream is not open. This could occur if the
            # server resets during an active transfer. Re-open the stream to
            # allow pending transfers to continue.
            self._open_read_stream()
        else:
            # Other errors are unrecoverable. Clear the stream and cancel any
            # pending transfers with an INTERNAL status as this is a system
            # error.
            self._read_stream = None

            for transfer in self._read_transfers.values():
                transfer.finish(Status.INTERNAL, skip_callback=True)
            self._read_transfers.clear()

            _LOG.error('Read stream shut down: %s', status)

    def _open_write_stream(self) -> None:
        self._write_stream = self._service.Write.invoke(
            lambda _, chunk: self._loop.call_soon_threadsafe(
                self._write_chunk_queue.put_nowait, chunk),
            on_error=lambda _, status: self._on_write_error(status))

    def _on_write_error(self, status: Status) -> None:
        """Callback for an RPC error in the write stream."""

        if status is Status.FAILED_PRECONDITION:
            # FAILED_PRECONDITION indicates that the stream packet was not
            # recognized as the stream is not open. This could occur if the
            # server resets during an active transfer. Re-open the stream to
            # allow pending transfers to continue.
            self._open_write_stream()
        else:
            # Other errors are unrecoverable. Clear the stream and cancel any
            # pending transfers with an INTERNAL status as this is a system
            # error.
            self._write_stream = None

            for transfer in self._write_transfers.values():
                transfer.finish(Status.INTERNAL, skip_callback=True)
            self._write_transfers.clear()

            _LOG.error('Write stream shut down: %s', status)

    def _start_read_transfer(self, transfer: _Transfer) -> None:
        """Begins a new read transfer, opening the stream if it isn't."""

        self._read_transfers[transfer.id] = transfer

        if not self._read_stream:
            self._open_read_stream()

        _LOG.debug('Starting new read transfer %d', transfer.id)
        self._loop.call_soon_threadsafe(self._new_transfer_queue.put_nowait,
                                        transfer)

    def _end_read_transfer(self, transfer: _Transfer) -> None:
        """Completes a read transfer."""
        del self._read_transfers[transfer.id]

        if not transfer.status.ok():
            _LOG.error('Read transfer %d terminated with status %s',
                       transfer.id, transfer.status)

        # TODO(frolv): This doesn't seem to work. Investigate why.
        # If no more transfers are using the read stream, close it.
        # if not self._read_transfers and self._read_stream:
        #     self._read_stream.cancel()
        #     self._read_stream = None

    def _start_write_transfer(self, transfer: _Transfer) -> None:
        """Begins a new write transfer, opening the stream if it isn't."""

        self._write_transfers[transfer.id] = transfer

        if not self._write_stream:
            self._open_write_stream()

        _LOG.debug('Starting new write transfer %d', transfer.id)
        self._loop.call_soon_threadsafe(self._new_transfer_queue.put_nowait,
                                        transfer)

    def _end_write_transfer(self, transfer: _Transfer) -> None:
        """Completes a write transfer."""
        del self._write_transfers[transfer.id]

        if not transfer.status.ok():
            _LOG.error('Write transfer %d terminated with status %s',
                       transfer.id, transfer.status)

        # TODO(frolv): This doesn't seem to work. Investigate why.
        # If no more transfers are using the write stream, close it.
        # if not self._write_transfers and self._write_stream:
        #     self._write_stream.cancel()
        #     self._write_stream = None


class Error(Exception):
    """Exception raised when a transfer fails.

    Stores the ID of the failed transfer and the error that occurred.
    """
    def __init__(self, transfer_id: int, status: Status):
        super().__init__(f'Transfer {transfer_id} failed with status {status}')
        self.transfer_id = transfer_id
        self.status = status
