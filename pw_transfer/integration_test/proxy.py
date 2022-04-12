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

import abc
import argparse
import asyncio
import logging
from pw_hdlc import decode
import random
import time
from typing import (Any, Awaitable, Callable, Optional)

_LOG = logging.getLogger('pw_transfer_intergration_test_proxy')


class Filter(abc.ABC):
    """An abstract interface for manipulating a stream of data.

    ``Filter``s are used to implement various transforms to simulate real
    world link properties.  Some examples include: data corruption,
    packet loss, packet reordering, rate limiting, latency modeling.

    A ``Filter`` implementation should implement the ``process`` method
    and call ``self.send_data()`` when it has data to send.
    """
    def __init__(self, send_data: Callable[[bytes], Awaitable[None]]):
        self.send_data = send_data
        pass

    @abc.abstractmethod
    async def process(self, data: bytes) -> None:
        """Processes incoming data.

        Implementations of this method may send arbitrary data, or none, using
        the ``self.send_data()`` handler.
        """

    async def __call__(self, data: bytes) -> None:
        await self.process(data)


class HdlcPacketizer(Filter):
    """A filter which aggregates data into complete HDLC packets.

    Since the proxy transport (SOCK_STREAM) has no framing and we want some
    filters to operates on whole frames, this filter can be used so that
    downstream filters see whole frames.
    """
    def __init__(self, send_data: Callable[[bytes], Awaitable[None]]):
        super().__init__(send_data)
        self.decoder = decode.FrameDecoder()

    async def process(self, data: bytes) -> None:
        for frame in self.decoder.process(data):
            await self.send_data(frame.raw_encoded)


class DataDropper(Filter):
    """A filter which drops some data.

    DataDropper will drop data passed through ``process()`` at the
    specified ``rate``.
    """
    def __init__(self,
                 send_data: Callable[[bytes], Awaitable[None]],
                 name: str,
                 rate: float,
                 seed: Optional[int] = None):
        super().__init__(send_data)
        self._rate = rate
        self._name = name
        if seed == None:
            seed = time.time_ns()
        self._rng = random.Random(seed)
        _LOG.info(f'{name} DataDropper initialized with seed {seed}')

    async def process(self, data: bytes) -> None:
        if self._rng.uniform(0.0, 1.0) < self._rate:
            _LOG.info(f'{self._name} dropped {len(data)} bytes of data')
        else:
            await self.send_data(data)


async def _handle_simplex_connection(name: str, reader: asyncio.StreamReader,
                                     writer: asyncio.StreamWriter) -> None:
    """Handle a single direction of a bidirectional connection between
    server and client."""
    async def send(data: bytes):
        writer.write(data)
        await writer.drain()

    filter_stack = HdlcPacketizer(DataDropper(send, name, 0.01))

    while True:
        # Arbitrarily chosen "page sized" read.
        data = await reader.read(4096)

        # An empty data indicates that the connection is closed.
        if not data:
            _LOG.info(f'{name} connection closed.')
            return

        await filter_stack.process(data)


async def _handle_connection(server_port: int,
                             client_reader: asyncio.StreamReader,
                             client_writer: asyncio.StreamWriter) -> None:
    """Handle a connection between server and client."""

    client_addr = client_writer.get_extra_info('peername')
    _LOG.info(f'New client connection from {client_addr}')

    # Open a new connection to the server for each client connection.
    #
    # TODO: catch exception and close client writer
    server_reader, server_writer = await asyncio.open_connection(
        'localhost', server_port)
    _LOG.info(f'New connection opened to server')

    # Instantiate two simplex handler one for each direction of the connection.
    _, pending = await asyncio.wait(
        [
            asyncio.create_task(
                _handle_simplex_connection("client", client_reader,
                                           server_writer)),
            asyncio.create_task(
                _handle_simplex_connection("server", server_reader,
                                           client_writer)),
        ],
        return_when=asyncio.FIRST_COMPLETED,
    )

    # When one side terminates the connection, also terminate the other side
    for task in pending:
        task.cancel()

    for stream in [client_writer, server_writer]:
        stream.close()


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
