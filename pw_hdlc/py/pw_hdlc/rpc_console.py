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

  python -m pw_hdlc.rpc_console --device /dev/ttyUSB0 sample.proto

This starts an IPython console for communicating with the connected device. A
few variables are predefined in the interactive console. These include:

    rpcs   - used to invoke RPCs
    device - the serial device used for communication
    client - the pw_rpc.Client
    protos - protocol buffer messages indexed by proto package

An example echo RPC command:

  rpcs.pw.rpc.EchoService.Echo(msg="hello!")
"""

import argparse
import glob
from inspect import cleandoc
import logging
from pathlib import Path
import sys
from types import ModuleType
from typing import Any, Collection, Iterable, Iterator, BinaryIO, List, Union
import socket

import serial  # type: ignore

import pw_cli.log
from pw_console import PwConsoleEmbed
from pw_console.__main__ import create_temp_log_file
from pw_log.proto import log_pb2
from pw_tokenizer import tokens
from pw_tokenizer.database import LoadTokenDatabases
from pw_tokenizer.detokenize import Detokenizer, detokenize_base64
from pw_hdlc.rpc import HdlcRpcClient, default_channels
from pw_rpc.console_tools.console import ClientInfo, flattened_rpc_completions

_LOG = logging.getLogger(__name__)
_DEVICE_LOG = logging.getLogger('rpc_device')

PW_RPC_MAX_PACKET_SIZE = 256
SOCKET_SERVER = 'localhost'
SOCKET_PORT = 33000
MKFIFO_MODE = 0o666


def _parse_args():
    """Parses and returns the command line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('-d', '--device', help='the serial port to use')
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
    parser.add_argument('--logfile', help='Console debug log file.')
    group.add_argument('-s',
                       '--socket-addr',
                       type=str,
                       help='use socket to connect to server, type default for\
            localhost:33000, or manually input the server address:port')
    parser.add_argument("--token-databases",
                        metavar='elf_or_token_database',
                        nargs="+",
                        action=LoadTokenDatabases,
                        help="Path to tokenizer database csv file(s).")
    parser.add_argument('--proto-globs',
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
        device=client.client.channel(1),
        rpcs=client.client.channel(1).rpcs,
        protos=client.protos.packages,
        # Include the active pane logger for creating logs in the repl.
        LOG=_DEVICE_LOG,
    )

    welcome_message = cleandoc("""
        Welcome to the Pigweed Console!

        Press F1 for help.
        Example commands:

          device.rpcs.pw.rpc.EchoService.Echo(msg='hello!')

          LOG.warning('Message appears console log window.')
    """)

    client_info = ClientInfo('device',
                             client.client.channel(1).rpcs, client.client)
    completions = flattened_rpc_completions([client_info])

    interactive_console = PwConsoleEmbed(
        global_vars=local_variables,
        local_vars=None,
        loggers=[_DEVICE_LOG],
        repl_startup_message=welcome_message,
        help_text=__doc__,
    )
    interactive_console.add_sentence_completer(completions)
    interactive_console.embed()


class SocketClientImpl:
    def __init__(self, config: str):
        self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        socket_server = ''
        socket_port = 0

        if config == 'default':
            socket_server = SOCKET_SERVER
            socket_port = SOCKET_PORT
        else:
            socket_server, socket_port_str = config.split(':')
            socket_port = int(socket_port_str)
        self.socket.connect((socket_server, socket_port))

    def write(self, data: bytes):
        self.socket.sendall(data)

    def read(self, num_bytes: int = PW_RPC_MAX_PACKET_SIZE):
        return self.socket.recv(num_bytes)


def console(device: str, baudrate: int, proto_globs: Collection[str],
            token_databases: Collection[tokens.Database], socket_addr: str,
            logfile: str, output: Any) -> int:
    """Starts an interactive RPC console for HDLC."""
    # argparse.FileType doesn't correctly handle '-' for binary files.
    if output is sys.stdout:
        output = sys.stdout.buffer

    if not logfile:
        # Create a temp logfile to prevent logs from appearing over stdout. This
        # would corrupt the prompt toolkit UI.
        logfile = create_temp_log_file()
    pw_cli.log.install(logging.INFO, True, False, logfile)

    detokenizer = None
    if token_databases:
        detokenizer = Detokenizer(tokens.Database.merged(*token_databases),
                                  show_errors=False)

    if not proto_globs:
        proto_globs = ['**/*.proto']

    protos: List[Union[ModuleType, Path]] = list(_expand_globs(proto_globs))

    # Append compiled log.proto library to avoid include errors when manually
    # provided, and shadowing errors due to ordering when the default global
    # search path is used.
    protos.append(log_pb2)

    if not protos:
        _LOG.critical('No .proto files were found with %s',
                      ', '.join(proto_globs))
        _LOG.critical('At least one .proto file is required')
        return 1

    _LOG.debug('Found %d .proto files found with %s', len(protos),
               ', '.join(proto_globs))

    if socket_addr is None:
        serial_device = serial.Serial(device, baudrate, timeout=1)
        read = lambda: serial_device.read(8192)
        write = serial_device.write
    else:
        try:
            socket_device = SocketClientImpl(socket_addr)
            read = socket_device.read
            write = socket_device.write
        except ValueError:
            _LOG.exception('Failed to initialize socket at %s', socket_addr)
            return 1

    _start_ipython_terminal(
        HdlcRpcClient(
            read, protos, default_channels(write),
            lambda data: detokenize_and_write_to_output(
                data, output, detokenizer)))
    return 0


def detokenize_and_write_to_output(data: bytes,
                                   unused_output: BinaryIO = sys.stdout.buffer,
                                   detokenizer=None):
    log_line = data
    if detokenizer:
        log_line = detokenize_base64(detokenizer, data)

    for line in log_line.decode(errors="surrogateescape").splitlines():
        _DEVICE_LOG.info(line)


def main() -> int:
    return console(**vars(_parse_args()))


if __name__ == '__main__':
    sys.exit(main())
