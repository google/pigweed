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

import logging
from pathlib import Path
import sys
import threading
import time
from types import ModuleType
from typing import Any, BinaryIO, Callable, Iterable, List, NoReturn, Union

from pw_hdlc_lite.decode import FrameDecoder
from pw_hdlc_lite import encode
import pw_rpc
from pw_rpc import callback_client
from pw_protobuf_compiler import python_protos

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

    return lambda data: writer(encode.information_frame(address, data))


def read_and_process_data(rpc_client: pw_rpc.Client,
                          device: BinaryIO,
                          output: Callable[[bytes], Any],
                          rpc_address: int = DEFAULT_ADDRESS) -> NoReturn:
    """Reads HDLC frames from the device and passes them to the RPC client."""
    decoder = FrameDecoder()

    while True:
        byte = device.read()
        for frame in decoder.process_valid_frames(byte):
            if not frame.ok():
                _LOG.error('Failed to parse frame: %s', frame.status.value)
                continue

            if frame.address == rpc_address:
                if not rpc_client.process_packet(frame.data):
                    _LOG.error('Packet not handled by RPC client: %s', frame)
            elif frame.address == STDOUT_ADDRESS:
                output(frame.data)
            else:
                _LOG.error('Unhandled frame for address %d: %s', frame.address,
                           frame.data.decode(errors='replace'))


_PathOrModule = Union[str, Path, ModuleType]


def write_to_file(data: bytes, output: BinaryIO = sys.stdout.buffer):
    output.write(data)
    output.write(b'\n')
    output.flush()


class HdlcRpcClient:
    """An RPC client configured to run over HDLC."""
    def __init__(self,
                 device: BinaryIO,
                 proto_paths_or_modules: Iterable[_PathOrModule],
                 output: Callable[[bytes], Any] = write_to_file,
                 channels: Iterable[pw_rpc.Channel] = None,
                 client_impl: pw_rpc.client.ClientImpl = None):
        """Creates an RPC client configured to communicate using HDLC.

        Args:
          device: serial.Serial (or any BinaryIO class) for reading/writing data
          proto_paths_or_modules: paths to .proto files or proto modules
          output: where to write "stdout" output from the device
        """
        self.device = device

        proto_modules = []
        proto_paths: List[Union[Path, str]] = []
        for proto in proto_paths_or_modules:
            if isinstance(proto, (Path, str)):
                proto_paths.append(proto)
            else:
                proto_modules.append(proto)

        proto_modules += python_protos.compile_and_import(proto_paths)

        if channels is None:
            channels = [pw_rpc.Channel(1, channel_output(device.write))]

        if client_impl is None:
            client_impl = callback_client.Impl()

        self.client = pw_rpc.Client.from_modules(client_impl, channels,
                                                 proto_modules)

        # Start background thread that reads and processes RPC packets.
        threading.Thread(target=read_and_process_data,
                         daemon=True,
                         args=(self.client, device, output)).start()

    def rpcs(self, channel_id: int = None) -> Any:
        """Returns object for accessing services on the specified channel.

        This skips some intermediate layers to make it simpler to invoke RPCs
        from an HdlcRpcClient. If only one channel is in use, the channel ID is
        not necessary.
        """
        if channel_id is None:
            return next(iter(self.client.channels())).rpcs

        return self.client.channel(channel_id).rpcs
