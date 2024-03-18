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

from typing import Callable, TYPE_CHECKING

import errno
import re
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

    _InitArgsType = tuple[
        socket.AddressFamily, int  # pylint: disable=no-member
    ]
    # Can be a string, (address, port) for AF_INET or (address, port, flowinfo,
    # scope_id) AF_INET6.
    _AddressType = str | tuple[str, int] | tuple[str, int, int, int]

    def __init__(
        self,
        config: str,
        on_disconnect: Callable[[SocketClient], None] | None = None,
    ):
        """Creates a socket connection.

        Args:
          config: The socket configuration. Accepted values and formats are:
            'default' - uses the default configuration (localhost:33000)
            'address:port' - An IPv4 address and port.
            'address' - An IPv4 address. Uses default port 33000.
            '[address]:port' - An IPv6 address and port.
            '[address]' - An IPv6 address. Uses default port 33000.
            'file:path_to_file' - A Unix socket at ``path_to_file``.
            In the formats above,``address`` can be an actual address or a name
            that resolves to an address through name-resolution.
          on_disconnect: An optional callback called when the socket
            disconnects.

        Raises:
          TypeError: The type of socket is not supported.
          ValueError: The socket configuration is invalid.
        """
        self.socket: socket.socket
        (
            self._socket_init_args,
            self._address,
        ) = SocketClient._parse_socket_config(config)
        self._on_disconnect = on_disconnect
        self._connected = False
        self.connect()

    @staticmethod
    def _parse_socket_config(
        config: str,
    ) -> tuple[SocketClient._InitArgsType, SocketClient._AddressType]:
        """Sets the variables used to create a socket given a config string.

        Raises:
          TypeError: The type of socket is not supported.
          ValueError: The socket configuration is invalid.
        """
        init_args: SocketClient._InitArgsType
        address: SocketClient._AddressType

        # Check if this is using the default settings.
        if config == 'default':
            init_args = socket.AF_INET6, socket.SOCK_STREAM
            address = (
                SocketClient.DEFAULT_SOCKET_SERVER,
                SocketClient.DEFAULT_SOCKET_PORT,
            )
            return init_args, address

        # Check if this is a UNIX socket.
        unix_socket_file_setting = f'{SocketClient.FILE_SOCKET_SERVER}:'
        if config.startswith(unix_socket_file_setting):
            # Unix socket support is available on Windows 10 since April
            # 2018. However, there is no Python support on Windows yet.
            # See https://bugs.python.org/issue33408 for more information.
            if not hasattr(socket, 'AF_UNIX'):
                raise TypeError(
                    'Unix sockets are not supported in this environment.'
                )
            init_args = (
                socket.AF_UNIX,  # pylint: disable=no-member
                socket.SOCK_STREAM,
            )
            address = config[len(unix_socket_file_setting) :]
            return init_args, address

        # Search for IPv4 or IPv6 address or name and port.
        # First, try to capture an IPv6 address as anything inside []. If there
        # are no [] capture the IPv4 address. Lastly, capture the port as the
        # numbers after :, if any.
        match = re.match(
            r'(\[(?P<ipv6_addr>.+)\]:?|(?P<ipv4_addr>[a-zA-Z0-9\._\/]+):?)'
            r'(?P<port>[0-9]+)?',
            config,
        )
        invalid_config_message = (
            f'Invalid socket configuration "{config}"'
            'Accepted values are "default", "file:<file_path>", '
            '"<name_or_ipv4_address>" with optional ":<port>", and '
            '"[<name_or_ipv6_address>]" with optional ":<port>".'
        )
        if match is None:
            raise ValueError(invalid_config_message)

        info = match.groupdict()
        if info['port']:
            port = int(info['port'])
        else:
            port = SocketClient.DEFAULT_SOCKET_PORT

        if info['ipv4_addr']:
            ip_addr = info['ipv4_addr']
        elif info['ipv6_addr']:
            ip_addr = info['ipv6_addr']
        else:
            raise ValueError(invalid_config_message)

        sock_family, sock_type, _, _, address = socket.getaddrinfo(
            ip_addr, port, type=socket.SOCK_STREAM
        )[0]
        init_args = sock_family, sock_type
        return init_args, address

    def __del__(self):
        if self._connected:
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
        self.socket = socket.socket(*self._socket_init_args)

        # Enable reusing address and port for reconnections.
        self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        if hasattr(socket, 'SO_REUSEPORT'):
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
        self.socket.connect(self._address)
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
