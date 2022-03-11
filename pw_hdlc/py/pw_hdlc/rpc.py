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

import collections
from concurrent.futures import ThreadPoolExecutor
import io
import logging
from queue import SimpleQueue
import random
import sys
import threading
import time
import socket
import subprocess
from typing import (Any, BinaryIO, Callable, Deque, Dict, Iterable, List,
                    NoReturn, Optional, Sequence, Tuple, Union)

from pw_protobuf_compiler import python_protos
import pw_rpc
from pw_rpc import callback_client

from pw_hdlc.decode import Frame, FrameDecoder
from pw_hdlc import encode

_LOG = logging.getLogger(__name__)

STDOUT_ADDRESS = 1
DEFAULT_ADDRESS = ord('R')
_VERBOSE = logging.DEBUG - 1


def channel_output(writer: Callable[[bytes], Any],
                   address: int = DEFAULT_ADDRESS,
                   delay_s: float = 0) -> Callable[[bytes], None]:
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


def _handle_error(frame: Frame) -> None:
    _LOG.error('Failed to parse frame: %s', frame.status.value)
    _LOG.debug('%s', frame.data)


FrameHandlers = Dict[int, Callable[[Frame], Any]]


def read_and_process_data(read: Callable[[], bytes],
                          on_read_error: Callable[[Exception], Any],
                          frame_handlers: FrameHandlers,
                          error_handler: Callable[[Frame],
                                                  Any] = _handle_error,
                          handler_threads: Optional[int] = 1) -> NoReturn:
    """Continuously reads and handles HDLC frames.

    Passes frames to an executor that calls frame handler functions in other
    threads.
    """
    def handle_frame(frame: Frame):
        try:
            if not frame.ok():
                error_handler(frame)
                return

            try:
                frame_handlers[frame.address](frame)
            except KeyError:
                _LOG.warning('Unhandled frame for address %d: %s',
                             frame.address, frame)
        except:  # pylint: disable=bare-except
            _LOG.exception('Exception in HDLC frame handler thread')

    decoder = FrameDecoder()

    # Execute callbacks in a ThreadPoolExecutor to decouple reading the input
    # stream from handling the data. That way, if a handler function takes a
    # long time or crashes, this reading thread is not interrupted.
    with ThreadPoolExecutor(max_workers=handler_threads) as executor:
        while True:
            try:
                data = read()
            except Exception as exc:  # pylint: disable=broad-except
                on_read_error(exc)
                continue

            if data:
                _LOG.log(_VERBOSE, 'Read %2d B: %s', len(data), data)

                for frame in decoder.process_valid_frames(data):
                    executor.submit(handle_frame, frame)


def write_to_file(data: bytes, output: BinaryIO = sys.stdout.buffer):
    output.write(data + b'\n')
    output.flush()


def default_channels(write: Callable[[bytes], Any]) -> List[pw_rpc.Channel]:
    return [pw_rpc.Channel(1, channel_output(write))]


PathsModulesOrProtoLibrary = Union[Iterable[python_protos.PathOrModule],
                                   python_protos.Library]


class HdlcRpcClient:
    """An RPC client configured to run over HDLC."""
    def __init__(self,
                 read: Callable[[], bytes],
                 paths_or_modules: PathsModulesOrProtoLibrary,
                 channels: Iterable[pw_rpc.Channel],
                 output: Callable[[bytes], Any] = write_to_file,
                 client_impl: pw_rpc.client.ClientImpl = None,
                 *,
                 _incoming_packet_filter_for_testing: '_PacketFilter' = None):
        """Creates an RPC client configured to communicate using HDLC.

        Args:
          read: Function that reads bytes; e.g serial_device.read.
          paths_or_modules: paths to .proto files or proto modules
          channel: RPC channels to use for output
          output: where to write "stdout" output from the device
        """
        if isinstance(paths_or_modules, python_protos.Library):
            self.protos = paths_or_modules
        else:
            self.protos = python_protos.Library.from_paths(paths_or_modules)

        if client_impl is None:
            client_impl = callback_client.Impl()

        self.client = pw_rpc.Client.from_modules(client_impl, channels,
                                                 self.protos.modules())

        self._test_filter = _incoming_packet_filter_for_testing

        frame_handlers: FrameHandlers = {
            DEFAULT_ADDRESS: self._handle_rpc_packet,
            STDOUT_ADDRESS: lambda frame: output(frame.data),
        }

        # Start background thread that reads and processes RPC packets.
        threading.Thread(target=read_and_process_data,
                         daemon=True,
                         args=(read, lambda exc: None,
                               frame_handlers)).start()

    def rpcs(self, channel_id: int = None) -> Any:
        """Returns object for accessing services on the specified channel.

        This skips some intermediate layers to make it simpler to invoke RPCs
        from an HdlcRpcClient. If only one channel is in use, the channel ID is
        not necessary.
        """
        if channel_id is None:
            return next(iter(self.client.channels())).rpcs

        return self.client.channel(channel_id).rpcs

    def _handle_rpc_packet(self, frame: Frame) -> None:
        if self._test_filter and not self._test_filter.keep_packet(frame.data):
            return

        if not self.client.process_packet(frame.data):
            _LOG.error('Packet not handled by RPC client: %s', frame.data)


def _try_connect(port: int, attempts: int = 10) -> socket.socket:
    """Tries to connect to the specified port up to the given number of times.

    This is helpful when connecting to a process that was started by this
    script. The process may need time to start listening for connections, and
    that length of time can vary. This retries with a short delay rather than
    having to wait for the worst case delay every time.
    """
    while True:
        attempts -= 1
        time.sleep(0.001)

        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(('localhost', port))
            return sock
        except ConnectionRefusedError:
            sock.close()
            if attempts <= 0:
                raise


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


class _PacketFilter:
    """Determines if a packet should be kept or dropped for testing purposes."""
    _Action = Callable[[int], Tuple[bool, bool]]
    _KEEP = lambda _: (True, False)
    _DROP = lambda _: (False, False)

    def __init__(self, name: str) -> None:
        self.name = name
        self.packet_count = 0
        self._actions: Deque[_PacketFilter._Action] = collections.deque()

    def reset(self) -> None:
        self.packet_count = 0
        self._actions.clear()

    def keep(self, count: int) -> None:
        """Keeps the next count packets."""
        self._actions.extend(_PacketFilter._KEEP for _ in range(count))

    def drop(self, count: int) -> None:
        """Drops the next count packets."""
        self._actions.extend(_PacketFilter._DROP for _ in range(count))

    def drop_every(self, every: int) -> None:
        """Drops every Nth packet forever."""
        self._actions.append(lambda count: (count % every != 0, True))

    def randomly_drop(self, one_in: int, gen: random.Random) -> None:
        """Drops packets randomly forever."""
        self._actions.append(lambda _: (gen.randrange(one_in) != 0, True))

    def keep_packet(self, packet: bytes) -> bool:
        """Returns whether the provided packet should be kept or dropped."""
        self.packet_count += 1

        if not self._actions:
            return True

        keep, repeat = self._actions[0](self.packet_count)

        if not repeat:
            self._actions.popleft()

        if not keep:
            _LOG.debug('Dropping %s packet %d for testing: %s', self.name,
                       self.packet_count, packet)
        return keep


class _TestChannelOutput:
    def __init__(self, send: Callable[[bytes], Any]) -> None:
        self._send = send
        self.packets = _PacketFilter('outgoing RPC')

    def __call__(self, data: bytes) -> None:
        if self.packets.keep_packet(data):
            self._send(data)


class HdlcRpcLocalServerAndClient:
    """Runs an RPC server in a subprocess and connects to it over a socket.

    This can be used to run a local RPC server in an integration test.
    """
    def __init__(self,
                 server_command: Sequence,
                 port: int,
                 protos: PathsModulesOrProtoLibrary,
                 *,
                 for_testing: bool = False) -> None:
        """Creates a new HdlcRpcLocalServerAndClient.

        If for_testing=True, the HdlcRpcLocalServerAndClient will have
        outgoing_packets and incoming_packets _PacketFilter members that can be
        used to program packet loss for testing purposes.
        """

        self.server = SocketSubprocess(server_command, port)

        self._bytes_queue: 'SimpleQueue[bytes]' = SimpleQueue()
        self._read_thread = threading.Thread(target=self._read_from_socket)
        self._read_thread.start()

        self.output = io.BytesIO()

        self.channel_output: Any = self.server.socket.sendall
        if for_testing:
            self.channel_output = _TestChannelOutput(self.channel_output)
            self.outgoing_packets = self.channel_output.packets
            self.incoming_packets = _PacketFilter('incoming RPC')
            incoming_filter: Optional[_PacketFilter] = self.incoming_packets
        else:
            incoming_filter = None

        self.client = HdlcRpcClient(
            self._bytes_queue.get,
            protos,
            default_channels(self.channel_output),
            self.output.write,
            _incoming_packet_filter_for_testing=incoming_filter).client

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
