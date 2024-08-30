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
"""Utilities for using HDLC with ``pw_rpc``."""

from __future__ import annotations

import io
import logging
import queue
import sys
import threading
import time
from typing import (
    Any,
    BinaryIO,
    Callable,
    Iterable,
    Sequence,
    TypeVar,
)

import pw_rpc
from pw_rpc import client_utils
from pw_hdlc.decode import Frame, FrameDecoder
from pw_hdlc import encode
from pw_stream import stream_readers

_LOG = logging.getLogger('pw_hdlc.rpc')
_VERBOSE = logging.DEBUG - 1


# Aliases for objects moved to their proper place in pw_rpc formerly defined
# here. This is for backwards compatibility.
CancellableReader = stream_readers.CancellableReader
SelectableReader = stream_readers.SelectableReader
SocketReader = stream_readers.SocketReader
SerialReader = stream_readers.SerialReader
DataReaderAndExecutor = stream_readers.DataReaderAndExecutor
PathsModulesOrProtoLibrary = client_utils.PathsModulesOrProtoLibrary
RpcClient = client_utils.RpcClient
NoEncodingSingleChannelRpcClient = client_utils.NoEncodingSingleChannelRpcClient
SocketSubprocess = stream_readers.SocketSubprocess
FrameTypeT = TypeVar('FrameTypeT')


# Default values for channel using HDLC encoding.
DEFAULT_CHANNEL_ID = 1
DEFAULT_ADDRESS = ord('R')
STDOUT_ADDRESS = 1

FrameHandlers = dict[int, Callable[[Frame], Any]]


# Default channel output for using HDLC encoding.
def channel_output(
    writer: Callable[[bytes], Any],
    address: int = DEFAULT_ADDRESS,
    delay_s: float = 0,
) -> Callable[[bytes], None]:
    """
    Returns a function that can be used as a channel output for ``pw_rpc``.
    """

    if delay_s:

        def slow_write(data: bytes) -> None:
            """Slows down writes in case unbuffered serial is in use."""
            for byte in data:
                time.sleep(delay_s)
                writer(bytes([byte]))

        return lambda data: slow_write(encode.ui_frame(address, data))

    def write_hdlc(data: bytes):
        frame = encode.ui_frame(address, data)
        _LOG.log(_VERBOSE, 'Write %2d B: %s', len(frame), frame)
        writer(frame)

    return write_hdlc


def default_channels(write: Callable[[bytes], Any]) -> list[pw_rpc.Channel]:
    """Default Channel with HDLC encoding."""
    return [pw_rpc.Channel(DEFAULT_CHANNEL_ID, channel_output(write))]


# Writes to stdout by default, but sys.stdout.buffer is not guaranteed to exist
# (see https://docs.python.org/3/library/io.html#io.TextIOBase.buffer). Defer
# to sys.__stdout__.buffer if sys.stdout is wrapped with something that does not
# offer it.
def write_to_file(
    data: bytes,
    output: BinaryIO = getattr(sys.stdout, 'buffer', sys.__stdout__.buffer),
) -> None:
    output.write(data + b'\n')
    output.flush()


class HdlcRpcClient(client_utils.RpcClient):
    """An RPC client configured to run over HDLC.

    Expects HDLC frames to have addresses that dictate how to parse the HDLC
    payloads.
    """

    def __init__(
        self,
        reader: stream_readers.CancellableReader,
        paths_or_modules: client_utils.PathsModulesOrProtoLibrary,
        channels: Iterable[pw_rpc.Channel],
        output: Callable[[bytes], Any] = write_to_file,
        client_impl: pw_rpc.client.ClientImpl | None = None,
        *,
        _incoming_packet_filter_for_testing: (
            pw_rpc.ChannelManipulator | None
        ) = None,
        rpc_frames_address: int = DEFAULT_ADDRESS,
        log_frames_address: int = STDOUT_ADDRESS,
        extra_frame_handlers: FrameHandlers | None = None,
    ):
        """Creates an RPC client configured to communicate using HDLC.

        Args:
          reader: Readable object used to receive RPC packets.
          paths_or_modules: paths to .proto files or proto modules.
          channels: RPC channels to use for output.
          output: where to write ``stdout`` output from the device.
          client_impl: The RPC Client implementation. Defaults to the callback
            client implementation if not provided.
          rpc_frames_address: the address used in the HDLC frames for RPC
            packets. This can be the channel ID, or any custom address.
          log_frames_address: the address used in the HDLC frames for ``stdout``
            output from the device.
          extra_fram_handlers: Optional mapping of HDLC frame addresses to their
            callbacks.
        """
        # Set up frame handling.
        rpc_output: Callable[[bytes], Any] = self.handle_rpc_packet
        if _incoming_packet_filter_for_testing is not None:
            _incoming_packet_filter_for_testing.send_packet = rpc_output
            rpc_output = _incoming_packet_filter_for_testing

        frame_handlers: FrameHandlers = {
            rpc_frames_address: lambda frame: rpc_output(frame.data),
            log_frames_address: lambda frame: output(frame.data),
        }
        if extra_frame_handlers:
            frame_handlers.update(extra_frame_handlers)

        def handle_frame(frame: Frame) -> None:
            # Suppress raising any frame errors to avoid crashes on data
            # processing, which may hide or drop other data.
            try:
                if not frame.ok():
                    _LOG.error('Failed to parse frame: %s', frame.status.value)
                    _LOG.debug('%s', frame.data)
                    return

                try:
                    frame_handlers[frame.address](frame)
                except KeyError:
                    _LOG.warning(
                        'Unhandled frame for address %d: %s',
                        frame.address,
                        frame,
                    )
            except:  # pylint: disable=bare-except
                _LOG.exception('Exception in HDLC frame handler thread')

        decoder = FrameDecoder()

        def on_read_error(exc: Exception) -> None:
            _LOG.error('data reader encountered an error', exc_info=exc)

        reader_and_executor = stream_readers.DataReaderAndExecutor(
            reader, on_read_error, decoder.process_valid_frames, handle_frame
        )
        super().__init__(
            reader_and_executor, paths_or_modules, channels, client_impl
        )


class HdlcRpcLocalServerAndClient:
    """Runs an RPC server in a subprocess and connects to it over a socket.

    This can be used to run a local RPC server in an integration test.
    """

    def __init__(
        self,
        server_command: Sequence,
        port: int,
        protos: client_utils.PathsModulesOrProtoLibrary,
        *,
        incoming_processor: pw_rpc.ChannelManipulator | None = None,
        outgoing_processor: pw_rpc.ChannelManipulator | None = None,
    ) -> None:
        """Creates a new ``HdlcRpcLocalServerAndClient``."""

        self.server = stream_readers.SocketSubprocess(server_command, port)

        self._bytes_queue: queue.SimpleQueue[bytes] = queue.SimpleQueue()
        self._read_thread = threading.Thread(target=self._read_from_socket)
        self._read_thread.start()

        self.output = io.BytesIO()

        self.channel_output: Any = self.server.socket.sendall

        self._incoming_processor = incoming_processor
        if outgoing_processor is not None:
            outgoing_processor.send_packet = self.channel_output
            self.channel_output = outgoing_processor

        class QueueReader(stream_readers.CancellableReader):
            def read(self) -> bytes:
                try:
                    return self._base_obj.get(timeout=3)
                except queue.Empty:
                    return b''

            def cancel_read(self) -> None:
                pass

        self._rpc_client = HdlcRpcClient(
            QueueReader(self._bytes_queue),
            protos,
            default_channels(self.channel_output),
            self.output.write,
            _incoming_packet_filter_for_testing=incoming_processor,
        )
        self.client = self._rpc_client.client

    def _read_from_socket(self):
        while True:
            data = self.server.socket.recv(4096)
            self._bytes_queue.put(data)
            if not data:
                return

    def close(self):
        self.server.close()
        self.output.close()
        self._rpc_client.close()
        self._read_thread.join()

    def __enter__(self) -> HdlcRpcLocalServerAndClient:
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        self.close()
