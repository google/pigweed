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
"""Utilities for using HDLC with pw_rpc."""

from concurrent.futures import ThreadPoolExecutor
import io
import logging
from queue import SimpleQueue
import sys
import threading
import time
import socket
import subprocess
from typing import (
    Any,
    BinaryIO,
    Callable,
    Dict,
    Iterable,
    List,
    NoReturn,
    Optional,
    Sequence,
    TypeVar,
    Union,
)

from pw_protobuf_compiler import python_protos
import pw_rpc
from pw_rpc import callback_client

from pw_hdlc.decode import Frame, FrameDecoder
from pw_hdlc import encode

_LOG = logging.getLogger(__name__)

STDOUT_ADDRESS = 1
DEFAULT_ADDRESS = ord('R')
DEFAULT_CHANNEL_ID = 1
_VERBOSE = logging.DEBUG - 1


def channel_output(
    writer: Callable[[bytes], Any],
    address: int = DEFAULT_ADDRESS,
    delay_s: float = 0,
) -> Callable[[bytes], None]:
    """Returns a function that can be used as a channel output for pw_rpc."""

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


FrameHandlers = Dict[int, Callable[[Frame], Any]]
FrameTypeT = TypeVar('FrameTypeT')


class DataReaderAndExecutor:
    """Reads incoming bytes, data processor that delegates frame handling.

    Executing callbacks in a ThreadPoolExecutor decouples reading the input
    stream from handling the data. That way, if a handler function takes a
    long time or crashes, this reading thread is not interrupted.
    """

    def __init__(
        self,
        read: Callable[[], bytes],
        on_read_error: Callable[[Exception], None],
        data_processor: Callable[[bytes], Iterable[FrameTypeT]],
        frame_handler: Callable[[FrameTypeT], None],
        handler_threads: Optional[int] = 1,
    ):
        """Creates the data reader and frame delegator.

        Args:
            read: Reads incoming bytes from the given transport, blocking until
              data is available or an exception is raised. Otherwise the reader
              will busy-loop.
            on_read_error: Called when there is an error reading incoming bytes.
            data_processor: Processes read bytes and returns a frame-like object
              that the frame_handler can process.
            frame_handler: Handles a received frame.
            handler_threads: The number of threads in the executor pool.
        """
        self._read = read
        self._on_read_error = on_read_error
        self._data_processor = data_processor
        self._frame_handler = frame_handler
        self._handler_threads = handler_threads

    def run(self) -> NoReturn:
        """Starts the reading process."""
        with ThreadPoolExecutor(max_workers=self._handler_threads) as executor:
            while True:
                try:
                    data = self._read()
                except Exception as exc:  # pylint: disable=broad-except
                    self._on_read_error(exc)
                    continue

                if not data:
                    continue

                _LOG.log(_VERBOSE, 'Read %2d B: %s', len(data), data)

                for frame in self._data_processor(data):
                    executor.submit(self._frame_handler, frame)


def write_to_file(data: bytes, output: BinaryIO = sys.stdout.buffer):
    output.write(data + b'\n')
    output.flush()


def default_channels(write: Callable[[bytes], Any]) -> List[pw_rpc.Channel]:
    return [pw_rpc.Channel(DEFAULT_CHANNEL_ID, channel_output(write))]


PathsModulesOrProtoLibrary = Union[
    Iterable[python_protos.PathOrModule], python_protos.Library
]


class RpcClient:
    """An RPC client with configurable incoming data processing."""

    def __init__(
        self,
        reader: DataReaderAndExecutor,
        paths_or_modules: PathsModulesOrProtoLibrary,
        channels: Iterable[pw_rpc.Channel],
        client_impl: Optional[pw_rpc.client.ClientImpl] = None,
    ):
        """Creates an RPC client.

        Args:
          read: Function that reads bytes; e.g serial_device.read.
          paths_or_modules: paths to .proto files or proto modules.
          channels: RPC channels to use for output.
        """
        if isinstance(paths_or_modules, python_protos.Library):
            self.protos = paths_or_modules
        else:
            self.protos = python_protos.Library.from_paths(paths_or_modules)

        if client_impl is None:
            client_impl = callback_client.Impl()

        self.client = pw_rpc.Client.from_modules(
            client_impl, channels, self.protos.modules()
        )

        # Start background thread that reads and processes RPC packets.
        threading.Thread(
            target=reader.run,
            daemon=True,
        ).start()

    def rpcs(self, channel_id: Optional[int] = None) -> Any:
        """Returns object for accessing services on the specified channel.

        This skips some intermediate layers to make it simpler to invoke RPCs
        from an HdlcRpcClient. If only one channel is in use, the channel ID is
        not necessary.
        """
        if channel_id is None:
            return next(iter(self.client.channels())).rpcs

        return self.client.channel(channel_id).rpcs

    def handle_rpc_packet(self, packet: bytes) -> None:
        if not self.client.process_packet(packet):
            _LOG.error('Packet not handled by RPC client: %s', packet)


class HdlcRpcClient(RpcClient):
    """An RPC client configured to run over HDLC."""

    def __init__(
        self,
        read: Callable[[], bytes],
        paths_or_modules: PathsModulesOrProtoLibrary,
        channels: Iterable[pw_rpc.Channel],
        output: Callable[[bytes], Any] = write_to_file,
        client_impl: Optional[pw_rpc.client.ClientImpl] = None,
        *,
        _incoming_packet_filter_for_testing: Optional[
            pw_rpc.ChannelManipulator
        ] = None,
    ):
        """Creates an RPC client configured to communicate using HDLC.

        Args:
          read: Function that reads bytes; e.g serial_device.read.
          paths_or_modules: paths to .proto files or proto modules.
          channels: RPC channels to use for output.
          output: where to write "stdout" output from the device.
        """
        # Set up frame handling.
        rpc_output: Callable[[bytes], Any] = self.handle_rpc_packet
        if _incoming_packet_filter_for_testing is not None:
            _incoming_packet_filter_for_testing.send_packet = rpc_output
            rpc_output = _incoming_packet_filter_for_testing

        frame_handlers: FrameHandlers = {
            DEFAULT_ADDRESS: lambda frame: rpc_output(frame.data),
            STDOUT_ADDRESS: lambda frame: output(frame.data),
        }

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
        reader = DataReaderAndExecutor(
            read, lambda exc: None, decoder.process_valid_frames, handle_frame
        )
        super().__init__(reader, paths_or_modules, channels, client_impl)


class NoEncodingSingleChannelRpcClient(RpcClient):
    """An RPC client without any frame encoding with a single channel output.

    The caveat is that the provided read function must read entire frames.
    """

    def __init__(
        self,
        read: Callable[[], bytes],
        paths_or_modules: PathsModulesOrProtoLibrary,
        channel: pw_rpc.Channel,
        client_impl: Optional[pw_rpc.client.ClientImpl] = None,
    ):
        """Creates an RPC client over a single channel with no frame encoding.

        Args:
          read: Function that reads bytes; e.g serial_device.read.
          paths_or_modules: paths to .proto files or proto modules.
          channel: RPC channel to use for output.
        """

        def process_data(data: bytes):
            yield data

        reader = DataReaderAndExecutor(
            read, lambda exc: None, process_data, self.handle_rpc_packet
        )
        super().__init__(reader, paths_or_modules, [channel], client_impl)


def _try_connect(port: int, attempts: int = 10) -> socket.socket:
    """Tries to connect to the specified port up to the given number of times.

    This is helpful when connecting to a process that was started by this
    script. The process may need time to start listening for connections, and
    that length of time can vary. This retries with a short delay rather than
    having to wait for the worst case delay every time.
    """
    timeout_s = 0.001
    while True:
        time.sleep(timeout_s)

        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(('localhost', port))
            return sock
        except ConnectionRefusedError:
            sock.close()
            attempts -= 1
            if attempts <= 0:
                raise

            timeout_s *= 2


class SocketSubprocess:
    """Executes a subprocess and connects to it with a socket."""

    def __init__(self, command: Sequence, port: int) -> None:
        self._server_process = subprocess.Popen(command, stdin=subprocess.PIPE)
        self.stdin = self._server_process.stdin

        try:
            self.socket: socket.socket = _try_connect(port)  # 🧦
        except:
            self._server_process.terminate()
            self._server_process.communicate()
            raise

    def close(self) -> None:
        try:
            self.socket.close()
        finally:
            self._server_process.terminate()
            self._server_process.communicate()

    def __enter__(self) -> 'SocketSubprocess':
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        self.close()


class HdlcRpcLocalServerAndClient:
    """Runs an RPC server in a subprocess and connects to it over a socket.

    This can be used to run a local RPC server in an integration test.
    """

    def __init__(
        self,
        server_command: Sequence,
        port: int,
        protos: PathsModulesOrProtoLibrary,
        *,
        incoming_processor: Optional[pw_rpc.ChannelManipulator] = None,
        outgoing_processor: Optional[pw_rpc.ChannelManipulator] = None,
    ) -> None:
        """Creates a new HdlcRpcLocalServerAndClient."""

        self.server = SocketSubprocess(server_command, port)

        self._bytes_queue: 'SimpleQueue[bytes]' = SimpleQueue()
        self._read_thread = threading.Thread(target=self._read_from_socket)
        self._read_thread.start()

        self.output = io.BytesIO()

        self.channel_output: Any = self.server.socket.sendall

        self._incoming_processor = incoming_processor
        if outgoing_processor is not None:
            outgoing_processor.send_packet = self.channel_output
            self.channel_output = outgoing_processor

        self.client = HdlcRpcClient(
            self._bytes_queue.get,
            protos,
            default_channels(self.channel_output),
            self.output.write,
            _incoming_packet_filter_for_testing=incoming_processor,
        ).client

    def _read_from_socket(self):
        while True:
            data = self.server.socket.recv(4096)
            self._bytes_queue.put(data)
            if not data:
                return

    def close(self):
        self.server.close()
        self.output.close()
        self._read_thread.join()

    def __enter__(self) -> 'HdlcRpcLocalServerAndClient':
        return self

    def __exit__(self, exc_type, exc_value, traceback) -> None:
        self.close()
