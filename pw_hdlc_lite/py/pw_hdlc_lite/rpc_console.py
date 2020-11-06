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

  python -m pw_hdlc_lite.rpc_console --device /dev/ttyUSB0 sample.proto

This starts an IPython console for communicating with the connected device. A
few variables are predefined in the interactive console. These include:

    rpcs   - used to invoke RPCs
    device - the serial device used for communication
    client - the pw_rpc.Client

An example echo RPC command:

  rpcs.pw.rpc.EchoService.Echo(msg="hello!")
"""

import argparse
import glob
import logging
from pathlib import Path
import sys
from typing import Collection, Iterable, Iterator, BinaryIO

import IPython  # type: ignore
import serial  # type: ignore

from pw_hdlc_lite.rpc import HdlcRpcClient, write_to_file

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
    parser.add_argument(
        '-o',
        '--output',
        type=argparse.FileType('wb'),
        default=sys.stdout.buffer,
        help=('The file to which to write device output (HDLC channel 1); '
              'provide - or omit for stdout.'))
    parser.add_argument('proto_globs',
                        nargs='+',
                        help='glob pattern for .proto files')
    return parser.parse_args()


def _expand_globs(globs: Iterable[str]) -> Iterator[Path]:
    for pattern in globs:
        for file in glob.glob(pattern, recursive=True):
            yield Path(file)


def _start_ipython_terminal(client: HdlcRpcClient) -> None:
    """Starts an interactive IPython terminal with preset variables."""
    local_variables = dict(
        client=client,
        channel_client=client.client.channel(1),
        rpcs=client.client.channel(1).rpcs,
    )

    print(__doc__)  # Print the banner
    IPython.terminal.embed.InteractiveShellEmbed().mainloop(
        local_ns=local_variables, module=argparse.Namespace())


def console(device: str, baudrate: int, proto_globs: Collection[str],
            output: BinaryIO) -> int:
    """Starts an interactive RPC console for HDLC."""
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

    _start_ipython_terminal(
        HdlcRpcClient(serial.Serial(device, baudrate), protos,
                      lambda data: write_to_file(data, output)))
    return 0


def main() -> int:
    return console(**vars(_parse_args()))


if __name__ == '__main__':
    sys.exit(main())
