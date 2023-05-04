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

"""Utils to decode logs."""

import logging

from pw_log.log_decoder import LogStreamDecoder
from pw_log.proto import log_pb2
import pw_rpc
import pw_status

_LOG = logging.getLogger(__name__)


class LogStreamHandler:
    """Handles an RPC Log Stream.

    Args:
        rpcs: RPC services to request RPC Log Streams.
        decoder: LogStreamDecoder
    """

    def __init__(
        self, rpcs: pw_rpc.client.Services, decoder: LogStreamDecoder
    ) -> None:
        self.rpcs = rpcs
        self._decoder = decoder

    def listen_to_logs(self) -> None:
        """Requests Logs streamed over RPC.

        The RPCs remain open until the server cancels or closes them, either
        with a response or error packet.
        """

        def on_log_entries(_, log_entries_proto: log_pb2.LogEntries) -> None:
            self._decoder.parse_log_entries_proto(log_entries_proto)

        self.rpcs.pw.log.Logs.Listen.open(
            on_next=on_log_entries,
            on_completed=lambda _, status: self.handle_log_stream_completed(
                status
            ),
            on_error=lambda _, error: self.handle_log_stream_error(error),
        )

    def handle_log_stream_error(self, error: pw_status.Status) -> None:
        """Resets the log stream RPC on error to avoid losing logs.

        Override this function to change default behavior.
        """
        _LOG.error(
            'Log stream error: %s from source %s',
            error,
            self._decoder.source_name,
        )
        # Only re-request logs if the RPC was not cancelled by the client.
        if error != pw_status.Status.CANCELLED:
            self.listen_to_logs()

    def handle_log_stream_completed(self, status: pw_status.Status) -> None:
        """Resets the log stream RPC on completed to avoid losing logs.

        Override this function to change default behavior.
        """
        _LOG.debug(
            'Log stream completed with status: %s for source: %s',
            status,
            self._decoder.source_name,
        )
        self.listen_to_logs()
