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
import random
import sys
import time
from typing import (Any, Awaitable, Callable, List, Optional)

from google.protobuf import text_format

from pigweed.pw_transfer.integration_test import config_pb2
from pw_hdlc import decode

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


class RateLimiter(Filter):
    """A filter which limits transmission rate.

    This filter delays transmission of data by len(data)/rate.
    """
    def __init__(self, send_data: Callable[[bytes], Awaitable[None]],
                 rate: float):
        super().__init__(send_data)
        self._rate = rate

    async def process(self, data: bytes) -> None:
        delay = len(data) / self._rate
        await asyncio.sleep(delay)
        await self.send_data(data)


class DataTransposer(Filter):
    """A filter which occasionally transposes two chunks of data.

    This filter transposes data at the specified rate.  It does this by
    holding a chunk to transpose until another chunk arrives. The filter
    will not hold a chunk longer than ``timeout`` seconds.
    """
    def __init__(self, send_data: Callable[[bytes], Awaitable[None]],
                 name: str, rate: float, timeout: float, seed: int):
        super().__init__(send_data)
        self._name = name
        self._rate = rate
        self._timeout = timeout
        self._data_queue = asyncio.Queue()
        self._rng = random.Random(seed)
        self._transpose_task = asyncio.create_task(self._transpose_handler())

        _LOG.info(f'{name} DataTranspose initialized with seed {seed}')

    def __del__(self):
        _LOG.info(f'{self._name} cleaning up transpose task.')
        self._transpose_task.cancel()

    async def _transpose_handler(self):
        """Async task that handles the packet transposition and timeouts"""
        held_data: Optional[bytes] = None
        while True:
            # Only use timeout if we have data held for transposition
            timeout = None if held_data is None else self._timeout
            try:
                data = await asyncio.wait_for(self._data_queue.get(),
                                              timeout=timeout)

                if held_data is not None:
                    # If we have held data, send it out of order.
                    await self.send_data(data)
                    await self.send_data(held_data)
                    held_data = None
                else:
                    # Otherwise decide if we should transpose the current data.
                    if self._rng.uniform(0.0, 1.0) < self._rate:
                        _LOG.info(
                            f'{self._name} transposing {len(data)} bytes of data'
                        )
                        held_data = data
                    else:
                        await self.send_data(data)

            except asyncio.TimeoutError:
                _LOG.info(f'{self._name} sending data in order due to timeout')
                await self.send_data(held_data)
                held_data = None

    async def process(self, data: bytes) -> None:
        # Queue data for processing by the transpose task.
        await self._data_queue.put(data)


async def _handle_simplex_connection(name: str, filter_stack_config: List[
    config_pb2.FilterConfig], reader: asyncio.StreamReader,
                                     writer: asyncio.StreamWriter) -> None:
    """Handle a single direction of a bidirectional connection between
    server and client."""
    async def send(data: bytes):
        writer.write(data)
        await writer.drain()

    filter_stack = send

    # Build the filter stack from the bottom up
    for config in reversed(filter_stack_config):
        filter_name = config.WhichOneof("filter")
        if filter_name == "hdlc_packetizer":
            filter_stack = HdlcPacketizer(filter_stack)
        elif filter_name == "data_dropper":
            data_dropper = config.data_dropper
            filter_stack = DataDropper(filter_stack, name, data_dropper.rate,
                                       data_dropper.seed)
        elif filter_name == "rate_limiter":
            filter_stack = RateLimiter(filter_stack, config.rate_limiter.rate)
        elif filter_name == "data_transposer":
            transposer = config.data_transposer
            filter_stack = DataTransposer(filter_stack, name, transposer.rate,
                                          transposer.timeout, transposer.seed)
        else:
            sys.exit(f'Unknown filter {filter_name}')

    while True:
        # Arbitrarily chosen "page sized" read.
        data = await reader.read(4096)

        # An empty data indicates that the connection is closed.
        if not data:
            _LOG.info(f'{name} connection closed.')
            return

        await filter_stack.process(data)


async def _handle_connection(server_port: int, config: config_pb2.ProxyConfig,
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
                _handle_simplex_connection(
                    "client", config.client_filter_stack, client_reader,
                    server_writer)),
            asyncio.create_task(
                _handle_simplex_connection(
                    "server", config.server_filter_stack, server_reader,
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

    # Load config from stdin using synchronous IO
    text_config = sys.stdin.buffer.read()

    config = text_format.Parse(text_config, config_pb2.ProxyConfig())

    # Instantiate the TCP server.
    server = await asyncio.start_server(
        lambda reader, writer: _handle_connection(
            server_port, config, reader, writer), 'localhost', client_port)

    addrs = ', '.join(str(sock.getsockname()) for sock in server.sockets)
    _LOG.info(f'Listening for client connection on {addrs}')

    # Run the TCP server.
    async with server:
        await server.serve_forever()


if __name__ == '__main__':
    asyncio.run(_main(**vars(_parse_args())))
