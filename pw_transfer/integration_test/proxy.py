#!/usr/bin/env python3
# Copyright 2022 The Pigweed Authors
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
"""Proxy for transfer integration testing.

This module contains a proxy for transfer intergation testing.  It is capable
of introducing various link failures into the connection between the client and
server.
"""

import argparse
import asyncio
import logging

_LOG = logging.getLogger('pw_transfer_intergration_test_proxy')


async def _handle_simplex_connection(name: str, reader: asyncio.StreamReader,
                                     writer: asyncio.StreamWriter) -> None:
    """Handle a single direction of a bidirectional connection between
    server and client."""

    while True:
        _LOG.debug(f'reading from {name}.')

        # Arbitrarily chosen "page sized" read.
        data = await reader.read(4096)

        # An empty data indicates that the connection is closed.
        if not data:
            _LOG.info(f'{name} connection closed.')
            return

        _LOG.debug(f'Received {len(data)} bytes from {name}.')
        writer.write(data)
        await writer.drain()


async def _handle_connection(server_port: int,
                             client_reader: asyncio.StreamReader,
                             client_writer: asyncio.StreamWriter) -> None:
    """Handle a connection between server and client."""

    client_addr = client_writer.get_extra_info('peername')
    _LOG.info(f'New client connection from {client_addr}')

    # Open a new connection to the server for each client connection.
    server_reader, server_writer = await asyncio.open_connection(
        'localhost', server_port)
    _LOG.info(f'New connection opened to server')

    # Instantiate two simplex handler one for each direction of the connection.
    #
    # TODO: server never closes connection.  We need to catch one of these closing
    # then close the other side.
    await asyncio.gather(
        _handle_simplex_connection("client", client_reader, server_writer),
        _handle_simplex_connection("server", server_reader, client_writer),
    )


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)

    parser.add_argument(
        '--server-port',
        type=int,
        required=True,
        help=
        'Port of the integration test server.  The proxy will forward connections to this port',
    )
    parser.add_argument(
        '--client-port',
        type=int,
        required=True,
        help=
        'Port on which to listen for connections from integration test client.',
    )

    return parser.parse_args()


def _init_logging(level: int) -> None:
    _LOG.setLevel(logging.DEBUG)
    log_to_stderr = logging.StreamHandler()
    log_to_stderr.setLevel(level)
    log_to_stderr.setFormatter(
        logging.Formatter(
            fmt='%(asctime)s.%(msecs)03d-%(levelname)s: %(message)s',
            datefmt='%H:%M:%S'))

    _LOG.addHandler(log_to_stderr)


async def _main(server_port: int, client_port: int) -> None:
    _init_logging(logging.DEBUG)

    # Instantiate the TCP server.
    server = await asyncio.start_server(
        lambda reader, writer: _handle_connection(server_port, reader, writer),
        'localhost', client_port)

    addrs = ', '.join(str(sock.getsockname()) for sock in server.sockets)
    _LOG.info(f'Listening for client connection on {addrs}')

    # Run the TCP server.
    async with server:
        await server.serve_forever()


if __name__ == '__main__':
    asyncio.run(_main(**vars(_parse_args())))
