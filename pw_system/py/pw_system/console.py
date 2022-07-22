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
"""Console for interacting with devices using HDLC.

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
import datetime
import glob
from inspect import cleandoc
import logging
from pathlib import Path
import sys
from types import ModuleType
from typing import (
    Any,
    Collection,
    Dict,
    Iterable,
    Iterator,
    List,
    Optional,
    Union,
)
import socket

import serial  # type: ignore

import pw_cli.log
import pw_console.python_logging
from pw_console import PwConsoleEmbed
from pw_console.pyserial_wrapper import SerialWithLogging
from pw_console.plugins.bandwidth_toolbar import BandwidthToolbar
from pw_console.log_store import LogStore
from pw_log.proto import log_pb2
from pw_metric_proto import metric_service_pb2
from pw_rpc.console_tools.console import flattened_rpc_completions
from pw_tokenizer.detokenize import AutoUpdatingDetokenizer
from pw_unit_test_proto import unit_test_pb2

from pw_system.device import Device

_LOG = logging.getLogger('tools')
_DEVICE_LOG = logging.getLogger('rpc_device')
_SERIAL_DEBUG = logging.getLogger('pw_console.serial_debug_logger')
_ROOT_LOG = logging.getLogger()

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
        '--serial-debug',
        action='store_true',
        help=('Enable debug log tracing of all data passed through'
              'pyserial read and write.'))
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
                        type=Path,
                        help="Path to tokenizer database csv file(s).")
    parser.add_argument('--config-file',
                        type=Path,
                        help='Path to a pw_console yaml config file.')
    parser.add_argument('--proto-globs',
                        nargs='+',
                        default=[],
                        help='glob pattern for .proto files')
    parser.add_argument('-v',
                        '--verbose',
                        action='store_true',
                        help='Enables debug logging when set')
    return parser.parse_args()


def _expand_globs(globs: Iterable[str]) -> Iterator[Path]:
    for pattern in globs:
        for file in glob.glob(pattern, recursive=True):
            yield Path(file)


def _start_ipython_terminal(
    device: Device,
    device_log_store: LogStore,
    root_log_store: LogStore,
    serial_debug_log_store: LogStore,
    log_file: str,
    serial_debug: bool = False,
    config_file_path: Optional[Path] = None,
) -> None:
    """Starts an interactive IPython terminal with preset variables."""
    local_variables = dict(
        client=device.client,
        device=device,
        rpcs=device.rpcs,
        protos=device.client.protos.packages,
        # Include the active pane logger for creating logs in the repl.
        DEVICE_LOG=_DEVICE_LOG,
        LOG=logging.getLogger(),
    )

    welcome_message = cleandoc("""
        Welcome to the Pigweed Console!

        Help: Press F1 or click the [Help] menu
        To move focus: Press Shift-Tab or click on a window

        Example Python commands:

          device.rpcs.pw.rpc.EchoService.Echo(msg='hello!')
          LOG.warning('Message appears in Host Logs window.')
          DEVICE_LOG.warning('Message appears in Device Logs window.')
    """)

    client_info = device.info()
    completions = flattened_rpc_completions([client_info])

    log_windows: Dict[str, Union[List[logging.Logger], LogStore]] = {
        'Device Logs': device_log_store,
        'Host Logs': root_log_store,
    }
    if serial_debug:
        log_windows['Serial Debug'] = serial_debug_log_store

    interactive_console = PwConsoleEmbed(
        global_vars=local_variables,
        local_vars=None,
        loggers=log_windows,
        repl_startup_message=welcome_message,
        help_text=__doc__,
        config_file_path=config_file_path,
    )
    interactive_console.add_sentence_completer(completions)
    if serial_debug:
        interactive_console.add_bottom_toolbar(BandwidthToolbar())

    # Setup Python logger propagation
    interactive_console.setup_python_logging(
        # Send any unhandled log messages to the external file.
        last_resort_filename=log_file,
        # Don't change propagation for these loggers.
        loggers_with_no_propagation=[_DEVICE_LOG])

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


#pylint: disable=too-many-arguments
def console(device: str,
            baudrate: int,
            proto_globs: Collection[str],
            token_databases: Collection[Path],
            socket_addr: str,
            logfile: str,
            output: Any,
            serial_debug: bool = False,
            config_file: Optional[Path] = None,
            verbose: bool = False,
            compiled_protos: Optional[List[ModuleType]] = None) -> int:
    """Starts an interactive RPC console for HDLC."""
    # argparse.FileType doesn't correctly handle '-' for binary files.
    if output is sys.stdout:
        output = sys.stdout.buffer

    # Don't send device logs to the root logger.
    _DEVICE_LOG.propagate = False
    # Create pw_console LogStore handlers. These are the data source for log
    # messages to be displayed in the UI.
    device_log_store = LogStore()
    root_log_store = LogStore()
    serial_debug_log_store = LogStore()
    # Attach the LogStores as handlers for each log window we want to show.
    # This should be done before device initialization to capture early
    # messages.
    _DEVICE_LOG.addHandler(device_log_store)
    _ROOT_LOG.addHandler(root_log_store)
    _SERIAL_DEBUG.addHandler(serial_debug_log_store)

    if not logfile:
        # Create a temp logfile to prevent logs from appearing over stdout. This
        # would corrupt the prompt toolkit UI.
        logfile = pw_console.python_logging.create_temp_log_file()

    log_level = logging.DEBUG if verbose else logging.INFO
    pw_cli.log.install(log_level, True, False, logfile)
    _LOG.setLevel(log_level)
    _DEVICE_LOG.setLevel(log_level)
    _ROOT_LOG.setLevel(log_level)
    _SERIAL_DEBUG.setLevel(log_level)

    detokenizer = None
    if token_databases:
        detokenizer = AutoUpdatingDetokenizer(*token_databases)
        detokenizer.show_errors = True

    protos: List[Union[ModuleType, Path]] = list(_expand_globs(proto_globs))

    if compiled_protos is None:
        compiled_protos = []

    # Append compiled log.proto library to avoid include errors when manually
    # provided, and shadowing errors due to ordering when the default global
    # search path is used.
    compiled_protos.append(log_pb2)
    compiled_protos.append(unit_test_pb2)
    protos.extend(compiled_protos)
    protos.append(metric_service_pb2)

    if not protos:
        _LOG.critical('No .proto files were found with %s',
                      ', '.join(proto_globs))
        _LOG.critical('At least one .proto file is required')
        return 1

    _LOG.debug('Found %d .proto files found with %s', len(protos),
               ', '.join(proto_globs))

    serial_impl = serial.Serial
    if serial_debug:
        serial_impl = SerialWithLogging

    timestamp_decoder = None
    if socket_addr is None:
        serial_device = serial_impl(
            device,
            baudrate,
            timeout=0,  # Non-blocking mode
        )
        read = lambda: serial_device.read(8192)
        write = serial_device.write

        # Overwrite decoder for serial device.
        def milliseconds_to_string(timestamp):
            """Parses milliseconds since boot to a human-readable string."""
            return str(datetime.timedelta(seconds=timestamp / 1e3))[:-3]

        timestamp_decoder = milliseconds_to_string
    else:
        try:
            socket_device = SocketClientImpl(socket_addr)
            read = socket_device.read
            write = socket_device.write
        except ValueError:
            _LOG.exception('Failed to initialize socket at %s', socket_addr)
            return 1

    device_client = Device(1,
                           read,
                           write,
                           protos,
                           detokenizer,
                           timestamp_decoder=timestamp_decoder,
                           rpc_timeout_s=5)

    _start_ipython_terminal(device_client, device_log_store, root_log_store,
                            serial_debug_log_store, logfile, serial_debug,
                            config_file)
    return 0


def main() -> int:
    return console(**vars(_parse_args()))


def main_with_compiled_protos(compiled_protos):
    return console(**vars(_parse_args()), compiled_protos=compiled_protos)


if __name__ == '__main__':
    sys.exit(main())
