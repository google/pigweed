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
import os
import time
from typing import Any, Callable, NoReturn, BinaryIO

import serial

from pw_hdlc_lite.decoder import FrameDecoder
from pw_hdlc_lite.encoder import encode_information_frame
from pw_rpc.client import Client

_LOG = logging.getLogger(__name__)

STDOUT_ADDRESS = 1
DEFAULT_ADDRESS = ord('R')


def channel_output(writer: Callable[[bytes], Any],
                   address: int = DEFAULT_ADDRESS,
                   delay_s: float = 0) -> Callable[[bytes], None]:
    """Returns a function that can be used as a channel output for pw_rpc."""

    if delay_s:

        def slow_write(data: bytes) -> None:
            """Slows down writes to support unbuffered serial."""
            for byte in data:
                time.sleep(delay_s)
                writer(bytes([byte]))

        return lambda data: slow_write(encode_information_frame(address, data))

    return lambda data: writer(encode_information_frame(address, data))


def read_and_process_data(rpc_client: Client,
                          device: serial.Serial,
                          output: BinaryIO,
                          output_sep: bytes = os.linesep.encode(),
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
                output.write(frame.data)
                output.write(output_sep)
                output.flush()
            else:
                _LOG.error('Unhandled frame for address %d: %s', frame.address,
                           frame.data.decoder(errors='replace'))
