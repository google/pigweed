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
import logging
import sys
import threading
import time
from typing import Any, BinaryIO, Callable, Dict, Iterable, NoReturn, Optional

from pw_protobuf_compiler import python_protos
import pw_rpc
from pw_rpc import callback_client

from pw_hdlc_lite.decode import Frame, FrameDecoder
from pw_hdlc_lite import encode

_LOG = logging.getLogger(__name__)

STDOUT_ADDRESS = 1
DEFAULT_ADDRESS = ord('R')


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

        return lambda data: slow_write(encode.information_frame(address, data))

    def write_hdlc(data: bytes):
        frame = encode.information_frame(address, data)
        _LOG.debug('Write %2d B: %s', len(frame), frame)
        writer(frame)

    return write_hdlc


def _handle_error(frame: Frame) -> None:
    _LOG.error('Failed to parse frame: %s', frame.status.value)
    _LOG.debug('%s', frame.data)


_FrameHandlers = Dict[int, Callable[[Frame], Any]]


def read_and_process_data(read: Callable[[], bytes],
                          frame_handlers: _FrameHandlers,
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
            data = read()
            if data:
                _LOG.debug('Read %2d B: %s', len(data), data)

                for frame in decoder.process_valid_frames(data):
                    executor.submit(handle_frame, frame)


def write_to_file(data: bytes, output: BinaryIO = sys.stdout.buffer):
    output.write(data)
    output.write(b'\n')
    output.flush()


class HdlcRpcClient:
    """An RPC client configured to run over HDLC."""
    def __init__(self,
                 read: Callable[[], bytes],
                 write: Callable[[bytes], Any],
                 proto_paths_or_modules: Iterable[python_protos.PathOrModule],
                 output: Callable[[bytes], Any] = write_to_file,
                 channels: Iterable[pw_rpc.Channel] = None,
                 client_impl: pw_rpc.client.ClientImpl = None):
        """Creates an RPC client configured to communicate using HDLC.

        Args:
          read: Function that reads bytes; e.g serial_device.read.
          write: Function that writes bytes; e.g. serial_device.write
          proto_paths_or_modules: paths to .proto files or proto modules
          output: where to write "stdout" output from the device
        """
        self.protos = python_protos.Library.from_paths(proto_paths_or_modules)

        if channels is None:
            channels = [pw_rpc.Channel(1, channel_output(write))]

        if client_impl is None:
            client_impl = callback_client.Impl()

        self.client = pw_rpc.Client.from_modules(client_impl, channels,
                                                 self.protos.modules())
        frame_handlers: _FrameHandlers = {
            DEFAULT_ADDRESS: self._handle_rpc_packet,
            STDOUT_ADDRESS: lambda frame: output(frame.data),
        }

        # Start background thread that reads and processes RPC packets.
        threading.Thread(target=read_and_process_data,
                         daemon=True,
                         args=(read, frame_handlers)).start()

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
        if not self.client.process_packet(frame.data):
            _LOG.error('Packet not handled by RPC client: %s', frame.data)
