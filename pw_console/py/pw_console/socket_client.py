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

from typing import Callable, Optional, TYPE_CHECKING, Tuple, Union

import errno
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

    def __init__(
        self,
        config: str,
        on_disconnect: Optional[Callable[[SocketClient], None]] = None,
    ):
        self._connection_type: int
        self._interface: Union[str, Tuple[str, int]]
        if config == 'default':
            self._connection_type = socket.AF_INET6
            self._interface = (
                self.DEFAULT_SOCKET_SERVER,
                self.DEFAULT_SOCKET_PORT,
            )
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
                self._connection_type = (
                    socket.AF_UNIX  # pylint: disable=no-member
                )
                self._interface = socket_port_or_file
            else:
                self._connection_type = socket.AF_INET6
                self._interface = (socket_server, int(socket_port_or_file))

        self._on_disconnect = on_disconnect
        self._connected = False
        self.connect()

    def __del__(self):
        self.socket.close()

    def write(self, data: ReadableBuffer) -> None:
        """Writes data and detects disconnects."""
        if not self._connected:
            raise Exception('Socket is not connected.')
        try:
            self.socket.sendall(data)
        except socket.error as exc:
            if isinstance(exc.args, tuple) and exc.args[0] == errno.EPIPE:
                self._handle_disconnect()
            else:
                raise exc

    def read(self, num_bytes: int = PW_RPC_MAX_PACKET_SIZE) -> bytes:
        """Blocks until data is ready and reads up to num_bytes."""
        if not self._connected:
            raise Exception('Socket is not connected.')
        data = self.socket.recv(num_bytes)
        # Since this is a blocking read, no data returned means the socket is
        # closed.
        if not data:
            self._handle_disconnect()
        return data

    def connect(self) -> None:
        """Connects to socket."""
        self.socket = socket.socket(self._connection_type, socket.SOCK_STREAM)

        # Enable reusing address and port for reconnections.
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        if hasattr(socket, 'SO_REUSEPORT'):
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        self.socket.connect(self._interface)
        self._connected = True

    def _handle_disconnect(self):
        """Escalates a socket disconnect to the user."""
        self.socket.close()
        self._connected = False
        if self._on_disconnect:
            self._on_disconnect(self)

    def fileno(self) -> int:
        return self.socket.fileno()


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
