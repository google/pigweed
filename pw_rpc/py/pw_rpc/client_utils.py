# Copyright 2024 The Pigweed Authors
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
"""Utilities for using ``pw_rpc.Client``."""

import logging
from typing import Any, Iterable

from pw_protobuf_compiler import python_protos
import pw_rpc
from pw_rpc import callback_client
from pw_stream import stream_readers

_LOG = logging.getLogger('pw_rpc.client_utils')
_VERBOSE = logging.DEBUG - 1


PathsModulesOrProtoLibrary = (
    Iterable[python_protos.PathOrModule] | python_protos.Library
)


class RpcClient:
    """An RPC client with configurable incoming data processing."""

    def __init__(
        self,
        reader_and_executor: stream_readers.DataReaderAndExecutor,
        paths_or_modules: PathsModulesOrProtoLibrary,
        channels: Iterable[pw_rpc.Channel],
        client_impl: pw_rpc.client.ClientImpl | None = None,
    ):
        """Creates an RPC client.

        Args:
          reader_and_executor: ``DataReaderAndExecutor`` instance.
          paths_or_modules: paths to .proto files or proto modules.
          channels: RPC channels to use for output.
          client_impl: The RPC client implementation. Defaults to the callback
            client implementation if not provided.
        """
        if isinstance(paths_or_modules, python_protos.Library):
            self.protos = paths_or_modules
        else:
            self.protos = python_protos.Library.from_paths(paths_or_modules)

        if client_impl is None:
            client_impl = callback_client.Impl()

        self.client = pw_rpc.Client.from_modules(
            client_impl, channels, self.protos.modules()
        )

        # Start background thread that reads and processes RPC packets.
        self._reader_and_executor = reader_and_executor
        self._reader_and_executor.start()

    def __enter__(self):
        return self

    def __exit__(self, *exc_info):
        self.close()

    def close(self) -> None:
        self._reader_and_executor.stop()

    def rpcs(self, channel_id: int | None = None) -> Any:
        """Returns object for accessing services on the specified channel.

        This skips some intermediate layers to make it simpler to invoke RPCs
        from an ``HdlcRpcClient``. If only one channel is in use, the channel ID
        is not necessary.
        """
        if channel_id is None:
            return next(iter(self.client.channels())).rpcs

        return self.client.channel(channel_id).rpcs

    def handle_rpc_packet(self, packet: bytes) -> None:
        if not self.client.process_packet(packet):
            _LOG.error('Packet not handled by RPC client: %s', packet)


class NoEncodingSingleChannelRpcClient(RpcClient):
    """An RPC client without any frame encoding with a single channel output.

    The caveat is that the provided read function must read entire frames.
    """

    def __init__(
        self,
        reader: stream_readers.CancellableReader,
        paths_or_modules: PathsModulesOrProtoLibrary,
        channel: pw_rpc.Channel,
        client_impl: pw_rpc.client.ClientImpl | None = None,
    ):
        """Creates an RPC client over a single channel with no frame encoding.

        Args:
          reader: Readable object used to receive RPC packets.
          paths_or_modules: paths to .proto files or proto modules.
          channel: RPC channel to use for output.
          client_impl: The RPC Client implementation. Defaults to the callback
            client implementation if not provided.
        """

        def process_data(data: bytes):
            yield data

        def on_read_error(exc: Exception) -> None:
            _LOG.error('data reader encountered an error', exc_info=exc)

        reader_and_executor = stream_readers.DataReaderAndExecutor(
            reader, on_read_error, process_data, self.handle_rpc_packet
        )
        super().__init__(
            reader_and_executor, paths_or_modules, [channel], client_impl
        )
