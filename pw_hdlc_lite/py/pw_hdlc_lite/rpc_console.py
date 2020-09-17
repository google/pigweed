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
"""Console for interacting with pw_rpc over HDLC.

To start the console, provide a serial port as the --device argument and paths
or globs for .proto files that define the RPC services to support:

  python -m pw_hdlc_lite.rpc_console --device /dev/ttyUSB0 --protos my.proto

This will start an IPython console for communicating with the connected device.
A few variables will be defined:

    rpcs - used to invoke RPCs
    device - the serial device used for communication
    client - the pw_rpc.Client

An example echo RPC command:

  rpcs.pw.rpc.EchoService.Echo(msg="hello!")
"""

import argparse
import glob
import logging
import os
from pathlib import Path
import sys
import threading
from typing import Collection, Iterable, Iterator, NoReturn, BinaryIO

import IPython
import serial

from pw_hdlc_lite import decoder, rpc
from pw_protobuf_compiler import python_protos
from pw_rpc import callback_client, descriptors
from pw_rpc.client import Client

_LOG = logging.getLogger(__name__)


def _parse_args():
    """Parses and returns the command line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-d',
                        '--device',
                        required=True,
                        help='the serial port to use')
    parser.add_argument('-b',
                        '--baudrate',
                        type=int,
                        default=115200,
                        help='the baud rate to use')
    parser.add_argument('-p',
                        '--protos',
                        dest='proto_globs',
                        action='append',
                        help='glob pattern for .proto files')
    parser.add_argument(
        '-o',
        '--output',
        type=argparse.FileType('wb'),
        default=sys.stdout.buffer,
        help=('The file to which to write device output (HDLC channel 1); '
              'provide - or omit for stdout.'))
    return parser.parse_args()


def read_and_process_data(
    rpc_client: Client,
    device: serial.Serial,
    output: BinaryIO,
    output_sep: bytes = os.linesep.encode()) -> NoReturn:
    """Reads HDLC frames from the device and passes them to the RPC client."""
    decode = decoder.FrameDecoder()

    while True:
        byte = device.read()
        for frame in decode.process_valid_frames(byte):
            if not frame.ok():
                _LOG.error('Failed to parse frame: %s', frame.status.value)
                continue

            if frame.address == rpc.DEFAULT_ADDRESS:
                if not rpc_client.process_packet(frame.data):
                    _LOG.error('Packet not handled by rpc client: %s', frame)
            elif frame.address == 1:
                output.write(frame.data)
                output.write(output_sep)
                output.flush()
            else:
                _LOG.error('Unhandled frame for address %d: %s', frame.address,
                           frame.data.decode(errors='replace'))


def _expand_globs(globs: Iterable[str]) -> Iterator[Path]:
    for pattern in globs:
        for file in glob.glob(pattern, recursive=True):
            yield Path(file)


def _start_ipython_terminal(  # pylint: disable=unused-argument
        device: serial.Serial, client: Client) -> None:
    """Starts IPython with local variables available."""
    channel_client = client.channel(1)  # pylint: disable=unused-variable
    rpcs = channel_client.rpcs  # pylint: disable=unused-variable
    IPython.terminal.embed.InteractiveShellEmbed(banner1=__doc__)()


def console(device: serial.Serial, protos: Iterable[Path], output) -> None:
    """Starts an interactive RPC console for HDLC.

    Args:
      device: the serial device from which to read HDLC frames
      protos: .proto files with RPC services
    """

    # Compile the proto files that define the RPC services to expose.
    modules = python_protos.compile_and_import(protos)

    # Set up the pw_rpc server.
    channel = descriptors.Channel(1, rpc.channel_output(device.write))
    client = Client.from_modules(callback_client.Impl(), [channel], modules)

    # Start background thread that reads serial data and processes RPC packets.
    threading.Thread(target=read_and_process_data,
                     daemon=True,
                     args=(client, device, output)).start()

    _start_ipython_terminal(device, client)


def _prepare_console(device: str, baudrate: int, proto_globs: Collection[str],
                     output: BinaryIO) -> int:
    # argparse.FileType doesn't correctly handle '-' for binary files.
    if output is sys.stdout:
        output = sys.stdout.buffer

    if not proto_globs:
        proto_globs = ['**/*.proto']

    protos = list(_expand_globs(proto_globs))

    if not protos:
        _LOG.critical('No .proto files were found with %s',
                      ', '.join(proto_globs))
        _LOG.critical('At least one .proto file is required')
        return 1

    _LOG.debug('Found %d .proto files found with %s', len(protos),
               ', '.join(proto_globs))

    console(serial.Serial(device, baudrate), protos, output)
    return 0


def main() -> int:
    return _prepare_console(**vars(_parse_args()))


if __name__ == '__main__':
    sys.exit(main())
