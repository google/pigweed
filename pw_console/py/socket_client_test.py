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
"""Tests for pw_console.socket_client"""

import socket
import unittest


from pw_console import socket_client


class TestSocketClient(unittest.TestCase):
    """Tests for SocketClient."""

    def test_parse_config_default(self) -> None:
        config = "default"
        with unittest.mock.patch.object(
            socket_client.SocketClient, 'connect', return_value=None
        ):
            client = socket_client.SocketClient(config)
            self.assertEqual(
                client._socket_init_args,  # pylint: disable=protected-access
                (socket.AF_INET6, socket.SOCK_STREAM),
            )
            self.assertEqual(
                client._address,  # pylint: disable=protected-access
                (
                    socket_client.SocketClient.DEFAULT_SOCKET_SERVER,
                    socket_client.SocketClient.DEFAULT_SOCKET_PORT,
                ),
            )

    def test_parse_config_unix_file(self) -> None:
        # Skip test if UNIX sockets are not supported.
        if not hasattr(socket, 'AF_UNIX'):
            return

        config = 'file:fake_file_path'
        with unittest.mock.patch.object(
            socket_client.SocketClient, 'connect', return_value=None
        ):
            client = socket_client.SocketClient(config)
            self.assertEqual(
                client._socket_init_args,  # pylint: disable=protected-access
                (
                    socket.AF_UNIX,  # pylint: disable=no-member
                    socket.SOCK_STREAM,
                ),
            )
            self.assertEqual(
                client._address,  # pylint: disable=protected-access
                'fake_file_path',
            )

    def _check_config_parsing(
        self, config: str, expected_address: str, expected_port: int
    ) -> None:
        with unittest.mock.patch.object(
            socket_client.SocketClient, 'connect', return_value=None
        ):
            fake_getaddrinfo_return_value = [
                (socket.AF_INET6, socket.SOCK_STREAM, 0, None, None)
            ]
            with unittest.mock.patch.object(
                socket,
                'getaddrinfo',
                return_value=fake_getaddrinfo_return_value,
            ) as mock_getaddrinfo:
                client = socket_client.SocketClient(config)
                mock_getaddrinfo.assert_called_with(
                    expected_address, expected_port, type=socket.SOCK_STREAM
                )
                # Assert the init args are what is returned by ``getaddrinfo``
                # not necessarily the correct ones, since this test should not
                # perform any network action.
                self.assertEqual(
                    client._socket_init_args,  # pylint: disable=protected-access
                    (
                        socket.AF_INET6,
                        socket.SOCK_STREAM,
                    ),
                )

    def test_parse_config_ipv4_domain(self) -> None:
        self._check_config_parsing(
            config='file.com/some_long/path:80',
            expected_address='file.com/some_long/path',
            expected_port=80,
        )

    def test_parse_config_ipv4_domain_no_port(self) -> None:
        self._check_config_parsing(
            config='file.com/some/path',
            expected_address='file.com/some/path',
            expected_port=socket_client.SocketClient.DEFAULT_SOCKET_PORT,
        )

    def test_parse_config_ipv4_address(self) -> None:
        self._check_config_parsing(
            config='8.8.8.8:8080',
            expected_address='8.8.8.8',
            expected_port=8080,
        )

    def test_parse_config_ipv4_address_no_port(self) -> None:
        self._check_config_parsing(
            config='8.8.8.8',
            expected_address='8.8.8.8',
            expected_port=socket_client.SocketClient.DEFAULT_SOCKET_PORT,
        )

    def test_parse_config_ipv6_domain(self) -> None:
        self._check_config_parsing(
            config='[file.com/some_long/path]:80',
            expected_address='file.com/some_long/path',
            expected_port=80,
        )

    def test_parse_config_ipv6_domain_no_port(self) -> None:
        self._check_config_parsing(
            config='[file.com/some/path]',
            expected_address='file.com/some/path',
            expected_port=socket_client.SocketClient.DEFAULT_SOCKET_PORT,
        )

    def test_parse_config_ipv6_address(self) -> None:
        self._check_config_parsing(
            config='[2001:4860:4860::8888:8080]:666',
            expected_address='2001:4860:4860::8888:8080',
            expected_port=666,
        )

    def test_parse_config_ipv6_address_no_port(self) -> None:
        self._check_config_parsing(
            config='[2001:4860:4860::8844]',
            expected_address='2001:4860:4860::8844',
            expected_port=socket_client.SocketClient.DEFAULT_SOCKET_PORT,
        )

    def test_parse_config_ipv6_local(self) -> None:
        self._check_config_parsing(
            config='[fe80::100%eth0]:80',
            expected_address='fe80::100%eth0',
            expected_port=80,
        )

    def test_parse_config_ipv6_local_no_port(self) -> None:
        self._check_config_parsing(
            config='[fe80::100%eth0]',
            expected_address='fe80::100%eth0',
            expected_port=socket_client.SocketClient.DEFAULT_SOCKET_PORT,
        )

    def test_parse_config_ipv6_local_windows(self) -> None:
        self._check_config_parsing(
            config='[fe80::100%4]:80',
            expected_address='fe80::100%4',
            expected_port=80,
        )

    def test_parse_config_ipv6_local_no_port_windows(self) -> None:
        self._check_config_parsing(
            config='[fe80::100%4]',
            expected_address='fe80::100%4',
            expected_port=socket_client.SocketClient.DEFAULT_SOCKET_PORT,
        )


if __name__ == '__main__':
    unittest.main()
