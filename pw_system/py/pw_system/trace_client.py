#!/usr/bin/env python3
# Copyright 2023 The Pigweed Authors
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
"""
Generates json trace files viewable using chrome://tracing using RPCs from a
connected trace service.

Example usage:
python pw_console/py/pw_console/trace_client.py
  -o trace.json
  -t out/host_device_simulator.speed_optimized/obj/pw_system/bin/system_example
"""

import argparse
import logging
import sys
from pathlib import Path
from types import ModuleType
from typing import List, Union


from pw_transfer import transfer_pb2
from pw_log.proto import log_pb2
from pw_trace_protos import trace_service_pb2
from pw_trace import trace
from pw_trace_tokenized import trace_tokenized
import pw_transfer
from pw_file import file_pb2
from pw_hdlc import rpc
from pw_system.device_tracing import DeviceWithTracing
from pw_tokenizer.detokenize import AutoUpdatingDetokenizer
from pw_console.socket_client import SocketClient


_LOG = logging.getLogger('pw_console_trace_client')
_LOG.level = logging.DEBUG
_LOG.addHandler(logging.StreamHandler(sys.stdout))


def start_tracing_on_device(client):
    """Start tracing on the device"""
    service = client.rpcs.pw.trace.proto.TraceService
    service.Start()


def stop_tracing_on_device(client):
    """Stop tracing on the device"""
    service = client.rpcs.pw.trace.proto.TraceService
    return service.Stop()


def list_files_on_device(client):
    """List files on the device"""
    service = client.rpcs.pw.file.FileSystem
    return service.List()


def delete_file_on_device(client, path):
    """Delete a file on the device"""
    service = client.rpcs.pw.file.FileSystem
    req = file_pb2.DeleteRequest(path=path)
    return service.Delete(req)


def _parse_args():
    """Parse and return command line arguments."""

    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    group = parser.add_mutually_exclusive_group(required=False)
    group.add_argument('-d', '--device', help='the serial port to use')
    parser.add_argument(
        '-b',
        '--baudrate',
        type=int,
        default=115200,
        help='the baud rate to use',
    )
    group.add_argument(
        '-s',
        '--socket-addr',
        type=str,
        default="default",
        help='use socket to connect to server, type default for\
            localhost:33000, or manually input the server address:port',
    )
    parser.add_argument(
        '-o',
        '--trace_output',
        dest='trace_output_file',
        help=('The json file to which to write the output.'),
    )
    parser.add_argument(
        '-t',
        '--trace_token_database',
        help='Databases (ELF, binary, or CSV) to use to lookup trace tokens.',
    )
    parser.add_argument(
        '-f',
        '--ticks_per_second',
        type=int,
        dest='ticks_per_second',
        help=('The clock rate of the trace events.'),
    )
    parser.add_argument(
        '--time_offset',
        type=int,
        dest='time_offset',
        default=0,
        help=('Time offset (us) of the trace events (Default 0).'),
    )
    parser.add_argument(
        '--channel-id',
        type=int,
        dest='channel_id',
        default=rpc.DEFAULT_CHANNEL_ID,
        help="Channel ID used in RPC communications.",
    )
    return parser.parse_args()


def _main(args) -> int:
    detokenizer = AutoUpdatingDetokenizer(args.trace_token_database + "#trace")
    detokenizer.show_errors = True

    socket_impl = SocketClient
    try:
        socket_device = socket_impl(args.socket_addr)
        reader = rpc.SelectableReader(socket_device)
        write = socket_device.write
    except ValueError:
        _LOG.exception('Failed to initialize socket at %s', args.socket_addr)
        return 1

    protos: List[Union[ModuleType, Path]] = [
        log_pb2,
        file_pb2,
        transfer_pb2,
        trace_service_pb2,
    ]

    with reader:
        device_client = DeviceWithTracing(
            args.ticks_per_second,
            args.channel_id,
            reader,
            write,
            protos,
            detokenizer=detokenizer,
            timestamp_decoder=None,
            rpc_timeout_s=5,
            use_rpc_logging=True,
            use_hdlc_encoding=True,
        )

        with device_client:
            _LOG.info("Starting tracing")
            start_tracing_on_device(device_client)

            _LOG.info("Stopping tracing")
            file_id = stop_tracing_on_device(device_client)
            _LOG.info("Trace file id = %d", file_id.response.file_id)

            _LOG.info("Listing Files")
            stream_response = list_files_on_device(device_client)

            if not stream_response.status.ok():
                _LOG.error('Failed to list files %s', stream_response.status)
                return 1

            for list_response in stream_response.responses:
                for file in list_response.paths:
                    _LOG.info("Transfering File: %s", file.path)
                    try:
                        data = device_client.transfer_manager.read(file.file_id)
                        events = trace_tokenized.get_trace_events(
                            [detokenizer.database],
                            data,
                            device_client.ticks_per_second,
                            args.time_offset,
                        )
                        json_lines = trace.generate_trace_json(events)
                        trace_tokenized.save_trace_file(
                            json_lines, args.trace_output_file
                        )
                    except pw_transfer.Error as err:
                        print('Failed to read:', err.status)

                    _LOG.info("Deleting File: %s", file.path)
                    delete_file_on_device(device_client, file.path)

            _LOG.info("All trace transfers completed successfully")

    return 0


if __name__ == '__main__':
    _main(_parse_args())
