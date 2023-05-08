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
"""Wrapers for socket clients to log read and write data."""
from __future__ import annotations

from typing import TYPE_CHECKING, Tuple, Union

import socket

from pw_console.plugins.bandwidth_toolbar import SerialBandwidthTracker

if TYPE_CHECKING:
    from _typeshed import ReadableBuffer


class SocketClient:
    """Socket transport implementation."""

    FILE_SOCKET_SERVER = 'file'
    DEFAULT_SOCKET_SERVER = 'localhost'
    DEFAULT_SOCKET_PORT = 33000
    PW_RPC_MAX_PACKET_SIZE = 256

    def __init__(self, config: str):
        connection_type: int
        interface: Union[str, Tuple[str, int]]
        if config == 'default':
            connection_type = socket.AF_INET
            interface = (self.DEFAULT_SOCKET_SERVER, self.DEFAULT_SOCKET_PORT)
        else:
            socket_server, socket_port_or_file = config.split(':')
            if socket_server == self.FILE_SOCKET_SERVER:
                # Unix socket support is available on Windows 10 since April
                # 2018. However, there is no Python support on Windows yet.
                # See https://bugs.python.org/issue33408 for more information.
                if not hasattr(socket, 'AF_UNIX'):
                    raise TypeError(
                        'Unix sockets are not supported in this environment.'
                    )
                connection_type = socket.AF_UNIX  # pylint: disable=no-member
                interface = socket_port_or_file
            else:
                connection_type = socket.AF_INET
                interface = (socket_server, int(socket_port_or_file))

        self.socket = socket.socket(connection_type, socket.SOCK_STREAM)
        self.socket.connect(interface)

    def write(self, data: ReadableBuffer) -> None:
        self.socket.sendall(data)

    def read(self, num_bytes: int = PW_RPC_MAX_PACKET_SIZE) -> bytes:
        return self.socket.recv(num_bytes)


class SocketClientWithLogging(SocketClient):
    """Socket with read and write wrappers for logging."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._bandwidth_tracker = SerialBandwidthTracker()

    def read(
        self, num_bytes: int = SocketClient.PW_RPC_MAX_PACKET_SIZE
    ) -> bytes:
        data = super().read(num_bytes)
        self._bandwidth_tracker.track_read_data(data)
        return data

    def write(self, data: ReadableBuffer) -> None:
        self._bandwidth_tracker.track_write_data(data)
        super().write(data)
