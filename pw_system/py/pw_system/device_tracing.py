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
"""Device tracing classes to interact with targets via RPC."""

import os
import logging
import tempfile

# from pathlib import Path
# from types import ModuleType
# from typing import Callable, List, Optional, Union
from typing import List, Optional

import pw_transfer
from pw_file import file_pb2
from pw_rpc.callback_client.errors import RpcError
from pw_system.device import Device
from pw_trace import trace
from pw_trace_tokenized import trace_tokenized

_LOG = logging.getLogger('tracing')
DEFAULT_TICKS_PER_SECOND = 1000


class DeviceWithTracing(Device):
    """Represents an RPC Client for a device running a Pigweed target with
    tracing.

    The target must have and RPC support for the following services:
     - logging
     - file
     - transfer
     - tracing
    Note: use this class as a base for specialized device representations.
    """

    def __init__(self, ticks_per_second: Optional[int], *argv, **kargv):
        super().__init__(*argv, **kargv)

        # Create the transfer manager
        self.transfer_service = self.rpcs.pw.transfer.Transfer
        self.transfer_manager = pw_transfer.Manager(
            self.transfer_service,
            default_response_timeout_s=self.rpc_timeout_s,
            initial_response_timeout_s=self.rpc_timeout_s,
            default_protocol_version=pw_transfer.ProtocolVersion.LATEST,
        )

        if ticks_per_second:
            self.ticks_per_second = ticks_per_second
        else:
            self.ticks_per_second = self.get_ticks_per_second()
        _LOG.info('ticks_per_second set to %i', self.ticks_per_second)

    def get_ticks_per_second(self) -> int:
        trace_service = self.rpcs.pw.trace.proto.TraceService
        try:
            resp = trace_service.GetClockParameters()
            if not resp.status.ok():
                _LOG.error(
                    'Failed to get clock parameters: %s. Using default \
                    value',
                    resp.status,
                )
                return DEFAULT_TICKS_PER_SECOND
        except RpcError as rpc_err:
            _LOG.exception('%s. Using default value', rpc_err)
            return DEFAULT_TICKS_PER_SECOND

        return resp.response.clock_parameters.tick_period_seconds_denominator

    def list_files(self) -> List:
        """Lists all files on this device."""
        fs_service = self.rpcs.pw.file.FileSystem
        stream_response = fs_service.List()

        if not stream_response.status.ok():
            _LOG.error('Failed to list files %s', stream_response.status)
            return []

        return stream_response.responses

    def delete_file(self, path: str) -> bool:
        """Delete a file on this device."""
        fs_service = self.rpcs.pw.file.FileSystem
        req = file_pb2.DeleteRequest(path=path)
        stream_response = fs_service.Delete(req)
        if not stream_response.status.ok():
            _LOG.error(
                'Failed to delete file %s file: %s',
                path,
                stream_response.status,
            )
            return False

        return True

    def transfer_file(self, file_id: int, dest_path: str) -> bool:
        """Transfer a file on this device to the host."""
        try:
            data = self.transfer_manager.read(file_id)
            with open(dest_path, "wb") as bin_file:
                bin_file.write(data)
        except pw_transfer.Error:
            _LOG.exception('Failed to transfer file_id %i', file_id)
            return False

        return True

    def start_tracing(self) -> None:
        """Turns on tracing on this device."""
        trace_service = self.rpcs.pw.trace.proto.TraceService
        trace_service.Start()

    def stop_tracing(self, trace_output_path: str = "trace.json") -> None:
        """Turns off tracing on this device and downloads the trace file."""
        trace_service = self.rpcs.pw.trace.proto.TraceService
        resp = trace_service.Stop()

        # If there's no tokenizer, there's no need to transfer the trace
        # file from the device after stopping tracing, as there's not much
        # that can be done with it.
        if not self.detokenizer:
            _LOG.error('No tokenizer specified. Not transfering trace')
            return

        trace_bin_path = tempfile.NamedTemporaryFile(delete=False)
        trace_bin_path.close()
        try:
            if not self.transfer_file(
                resp.response.file_id, trace_bin_path.name
            ):
                return

            with open(trace_bin_path.name, 'rb') as bin_file:
                trace_data = bin_file.read()
                events = trace_tokenized.get_trace_events(
                    [self.detokenizer.database],
                    trace_data,
                    self.ticks_per_second,
                    self.time_offset,
                )
                json_lines = trace.generate_trace_json(events)
                trace_tokenized.save_trace_file(json_lines, trace_output_path)

            _LOG.info(
                'Wrote trace file %s',
                trace_output_path,
            )
        finally:
            os.remove(trace_bin_path.name)
