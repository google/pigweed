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
"""Device classes to interact with targets via RPC."""

import logging
from pathlib import Path
from types import ModuleType
from typing import Any, Callable, List, Union, Optional

from pw_hdlc.rpc import HdlcRpcClient, channel_output
from pw_hdlc.rpc import NoEncodingSingleChannelRpcClient, RpcClient
from pw_log.log_decoder import (
    Log,
    LogStreamDecoder,
    log_decoded_log,
    timestamp_parser_ns_since_boot,
)
from pw_log_rpc.rpc_log_stream import LogStreamHandler
from pw_metric import metric_parser
from pw_rpc import callback_client, Channel, console_tools
from pw_thread.thread_analyzer import ThreadSnapshotAnalyzer
from pw_thread_protos import thread_pb2
from pw_tokenizer import detokenize
from pw_tokenizer.proto import decode_optionally_tokenized
from pw_unit_test.rpc import run_tests as pw_unit_test_run_tests

# Internal log for troubleshooting this tool (the console).
_LOG = logging.getLogger('tools')
DEFAULT_DEVICE_LOGGER = logging.getLogger('rpc_device')


class Device:
    """Represents an RPC Client for a device running a Pigweed target.

    The target must have and RPC support, RPC logging.
    Note: use this class as a base for specialized device representations.
    """

    def __init__(
        self,
        channel_id: int,
        read,
        write,
        proto_library: List[Union[ModuleType, Path]],
        detokenizer: Optional[detokenize.Detokenizer] = None,
        timestamp_decoder: Optional[Callable[[int], str]] = None,
        rpc_timeout_s: float = 5,
        use_rpc_logging: bool = True,
        use_hdlc_encoding: bool = True,
    ):
        self.channel_id = channel_id
        self.protos = proto_library
        self.detokenizer = detokenizer
        self.rpc_timeout_s = rpc_timeout_s

        self.logger = DEFAULT_DEVICE_LOGGER
        self.logger.setLevel(logging.DEBUG)  # Allow all device logs through.

        callback_client_impl = callback_client.Impl(
            default_unary_timeout_s=self.rpc_timeout_s,
            default_stream_timeout_s=None,
        )

        def detokenize_and_log_output(data: bytes, _detokenizer=None):
            log_messages = data.decode(
                encoding='utf-8', errors='surrogateescape'
            )

            if self.detokenizer:
                log_messages = decode_optionally_tokenized(
                    self.detokenizer, data
                )

            for line in log_messages.splitlines():
                self.logger.info(line)

        self.client: RpcClient
        if use_hdlc_encoding:
            channels = [Channel(self.channel_id, channel_output(write))]
            self.client = HdlcRpcClient(
                read,
                self.protos,
                channels,
                detokenize_and_log_output,
                client_impl=callback_client_impl,
            )
        else:
            channel = Channel(self.channel_id, write)
            self.client = NoEncodingSingleChannelRpcClient(
                read,
                self.protos,
                channel,
                client_impl=callback_client_impl,
            )

        if use_rpc_logging:
            # Create the log decoder used by the LogStreamHandler.

            def decoded_log_handler(log: Log) -> None:
                log_decoded_log(log, self.logger)

            self._log_decoder = LogStreamDecoder(
                decoded_log_handler=decoded_log_handler,
                detokenizer=self.detokenizer,
                source_name='RpcDevice',
                timestamp_parser=(
                    timestamp_decoder
                    if timestamp_decoder
                    else timestamp_parser_ns_since_boot
                ),
            )

            # Start listening to logs as soon as possible.
            self.log_stream_handler = LogStreamHandler(
                self.rpcs, self._log_decoder
            )
            self.log_stream_handler.listen_to_logs()

    def info(self) -> console_tools.ClientInfo:
        return console_tools.ClientInfo('device', self.rpcs, self.client.client)

    @property
    def rpcs(self) -> Any:
        """Returns an object for accessing services on the specified channel."""
        return next(iter(self.client.client.channels())).rpcs

    def run_tests(self, timeout_s: Optional[float] = 5) -> bool:
        """Runs the unit tests on this device."""
        return pw_unit_test_run_tests(self.rpcs, timeout_s=timeout_s)

    def get_and_log_metrics(self) -> dict:
        """Retrieves the parsed metrics and logs them to the console."""
        metrics = metric_parser.parse_metrics(
            self.rpcs, self.detokenizer, self.rpc_timeout_s
        )

        def print_metrics(metrics, path):
            """Traverses dictionaries, until a non-dict value is reached."""
            for path_name, metric in metrics.items():
                if isinstance(metric, dict):
                    print_metrics(metric, path + '/' + path_name)
                else:
                    _LOG.info('%s/%s: %s', path, path_name, str(metric))

        print_metrics(metrics, '')
        return metrics

    def snapshot_peak_stack_usage(self, thread_name: Optional[str] = None):
        snapshot_service = self.rpcs.pw.thread.proto.ThreadSnapshotService
        _, rsp = snapshot_service.GetPeakStackUsage(name=thread_name)

        thread_info = thread_pb2.SnapshotThreadInfo()
        for thread_info_block in rsp:
            for thread in thread_info_block.threads:
                thread_info.threads.append(thread)
        for line in str(ThreadSnapshotAnalyzer(thread_info)).splitlines():
            _LOG.info('%s', line)
