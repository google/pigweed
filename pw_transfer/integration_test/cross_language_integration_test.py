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
"""Cross-language test of pw_transfer.

Usage:

   bazel run pw_transfer/integration_test:cross_language_integration_test

"""

import asyncio
import logging
from parameterized import parameterized
import random
import sys
import tempfile
from typing import List
import unittest

from google.protobuf import text_format

from pigweed.pw_transfer.integration_test import config_pb2
from rules_python.python.runfiles import runfiles

SERVER_PORT = 3300
CLIENT_PORT = 3301

_LOG = logging.getLogger('pw_transfer_intergration_test_proxy')
_LOG.level = logging.DEBUG
_LOG.addHandler(logging.StreamHandler(sys.stdout))


class LogMonitor():
    """Monitors lines read from the reader, and logs them."""
    class Error(Exception):
        """Raised if wait_for_line reaches EOF before expected line."""
        pass

    def __init__(self, prefix: str, reader: asyncio.StreamReader):
        """Initializer.

        Args:
          prefix: Prepended to read lines before they are logged.
          reader: StreamReader to read lines from.
        """
        self._prefix = prefix
        self._reader = reader

        # Queue of messages waiting to be monitored.
        self._queue = asyncio.Queue()
        # Relog any messages read from the reader, and enqueue them for
        # monitoring.
        self._relog_and_enqueue_task = asyncio.create_task(
            self._relog_and_enqueue())

    async def wait_for_line(self, msg: str):
        """Wait for a line containing msg to be read from the reader."""
        while True:
            line = await self._queue.get()
            if not line:
                raise LogMonitor.Error(
                    f"Reached EOF before getting line matching {msg}")
            if msg in line.decode():
                return

    async def wait_for_eof(self):
        """Wait for the reader to reach EOF, relogging any lines read."""
        # Drain the queue, since we're not monitoring it any more.
        drain_queue = asyncio.create_task(self._drain_queue())
        await asyncio.gather(drain_queue, self._relog_and_enqueue_task)

    async def _relog_and_enqueue(self):
        """Reads lines from the reader, logs them, and puts them in queue."""
        while True:
            line = await self._reader.readline()
            await self._queue.put(line)
            if line:
                _LOG.info(f"{self._prefix} {line.decode().rstrip()}")
            else:
                # EOF. Note, we still put the EOF in the queue, so that the
                # queue reader can process it appropriately.
                return

    async def _drain_queue(self):
        while True:
            line = await self._queue.get()
            if not line:
                # EOF.
                return


class MonitoredSubprocess:
    """A subprocess with monitored asynchronous communication."""
    @staticmethod
    async def create(cmd: List[str], prefix: str, stdinput: bytes):
        """Starts the subprocess and writes stdinput to stdin.

        This method returns once stdinput has been written to stdin. The
        MonitoredSubprocess continues to log the process's stderr and stdout
        (with the prefix) until it terminates.

        Args:
          cmd: Command line to execute.
          prefix: Prepended to process logs.
          stdinput: Written to stdin on process startup.
        """
        self = MonitoredSubprocess()
        self._process = await asyncio.create_subprocess_exec(
            *cmd,
            stdin=asyncio.subprocess.PIPE,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE)

        self._stderr_monitor = LogMonitor(f"{prefix} ERR:",
                                          self._process.stderr)
        self._stdout_monitor = LogMonitor(f"{prefix} OUT:",
                                          self._process.stdout)

        self._process.stdin.write(stdinput)
        await self._process.stdin.drain()
        self._process.stdin.close()
        await self._process.stdin.wait_closed()
        return self

    async def wait_for_line(self, stream: str, msg: str, timeout: float):
        """Wait for a line containing msg to be read on the stream."""
        if stream == "stdout":
            monitor = self._stdout_monitor
        elif stream == "stderr":
            monitor = self._stderr_monitor
        else:
            raise ValueError(
                "Stream must be 'stdout' or 'stderr', got {stream}")

        await asyncio.wait_for(monitor.wait_for_line(msg), timeout)

    def returncode(self):
        return self._process.returncode

    def terminate(self):
        """Terminate the process."""
        self._process.terminate()

    async def wait_for_termination(self, timeout: float):
        """Wait for the process to terminate."""
        await asyncio.wait_for(
            asyncio.gather(self._process.wait(),
                           self._stdout_monitor.wait_for_eof(),
                           self._stderr_monitor.wait_for_eof()), timeout)

    async def terminate_and_wait(self, timeout: float):
        """Terminate the process and wait for it to exit."""
        if self.returncode() is not None:
            # Process already terminated
            return
        self.terminate()
        await self.wait_for_termination(timeout)


class PwTransferIntegrationTest(unittest.TestCase):
    # Prefix for log messages coming from the harness (as opposed to the server,
    # client, or proxy processes). Padded so that the length is the same as
    # "SERVER OUT:".
    _PREFIX = "HARNESS:   "

    @classmethod
    def setUpClass(cls):
        # TODO(tpudlik): This is Bazel-only. Support gn, too.
        r = runfiles.Create()
        cls._JAVA_CLIENT_BINARY = r.Rlocation(
            "pigweed/pw_transfer/integration_test/java_client")
        cls._CPP_CLIENT_BINARY = r.Rlocation(
            "pigweed/pw_transfer/integration_test/cpp_client")
        cls._PROXY_BINARY = r.Rlocation(
            "pigweed/pw_transfer/integration_test/proxy")
        cls._SERVER_BINARY = r.Rlocation(
            "pigweed/pw_transfer/integration_test/server")
        cls._CLIENT_BINARY = {
            "cpp": cls._CPP_CLIENT_BINARY,
            "java": cls._JAVA_CLIENT_BINARY,
        }

    async def _start_client(self, client_type: str,
                            config: config_pb2.ClientConfig):
        _LOG.info(f"{self._PREFIX} Starting client with config\n{config}")
        self._client = await MonitoredSubprocess.create(
            [self._CLIENT_BINARY[client_type],
             str(CLIENT_PORT)], "CLIENT",
            str(config).encode('ascii'))

    async def _start_server(self, config: config_pb2.ServerConfig):
        _LOG.info(f"{self._PREFIX} Starting server with config\n{config}")
        self._server = await MonitoredSubprocess.create(
            [self._SERVER_BINARY, str(SERVER_PORT)], "SERVER",
            str(config).encode('ascii'))

    async def _start_proxy(self, config: config_pb2.ProxyConfig):
        _LOG.info(f"{self._PREFIX} Starting proxy with config\n{config}")
        self._proxy = await MonitoredSubprocess.create(
            [
                self._PROXY_BINARY, "--server-port",
                str(SERVER_PORT), "--client-port",
                str(CLIENT_PORT)
            ],
            # Extra space in "PROXY " so that it lines up with "SERVER".
            "PROXY ",
            str(config).encode('ascii'))

    async def _perform_write(self, server_config: config_pb2.ServerConfig,
                             client_type: str,
                             client_config: config_pb2.ClientConfig,
                             proxy_config: config_pb2.ProxyConfig,
                             payload) -> bytes:
        """Performs a pw_transfer write.

        Args:
          server_config: Server configuration.
          client_type: Either "cpp" or "java".
          client_config: Client configuration.
          proxy_config: Proxy configuration.
          payload: bytes to write

        Returns: Bytes the server has saved after receiving the payload.
        """
        # Timeout for components (server, proxy) to come up or shut down after
        # write is finished or a signal is sent. Approximately arbitrary. Should
        # not be too long so that we catch bugs in the server that prevent it
        # from shutting down.
        TIMEOUT = 5  # seconds
        with tempfile.NamedTemporaryFile(
        ) as f_payload, tempfile.NamedTemporaryFile() as f_server_output:
            server_config.file = f_server_output.name
            client_config.file = f_payload.name

            f_payload.write(payload)
            f_payload.flush()  # Ensure contents are there to read!

            try:
                await self._start_proxy(proxy_config)
                await self._proxy.wait_for_line(
                    "stderr", "Listening for client connection", TIMEOUT)

                await self._start_server(server_config)
                await self._server.wait_for_line(
                    "stderr", "Starting pw_rpc server on port", TIMEOUT)

                await self._start_client(client_type, client_config)
                # No timeout: the client will only exit once the transfer
                # completes, and this can take a long time for large payloads.
                await self._client.wait_for_termination(None)
                self.assertEqual(self._client.returncode(), 0)

                # Wait for the server to exit.
                await self._server.wait_for_termination(TIMEOUT)
                self.assertEqual(self._server.returncode(), 0)

            finally:
                # Stop the server, if still running. (Only expected if the
                # wait_for above timed out.)
                if self._server:
                    await self._server.terminate_and_wait(TIMEOUT)
                # Stop the proxy. Unlike the server, we expect it to still be
                # running at this stage.
                if self._proxy:
                    await self._proxy.terminate_and_wait(TIMEOUT)

            return f_server_output.read()

    @parameterized.expand([
        ("cpp"),
        ("java"),
    ])
    def test_small_client_write(self, client_type):
        resource_id = 12
        payload = b"some data"
        server_config = config_pb2.ServerConfig(
            resource_id=resource_id,
            chunk_size_bytes=216,
            pending_bytes=32 * 1024,
            chunk_timeout_seconds=5,
            transfer_service_retries=4,
            extend_window_divisor=32,
        )
        client_config = config_pb2.ClientConfig(
            resource_id=resource_id,
            max_retries=5,
            initial_chunk_timeout_ms=10000,
            chunk_timeout_ms=4000,
        )
        proxy_config = text_format.Parse(
            """
            client_filter_stack: [
                { hdlc_packetizer: {} },
                { data_dropper: {rate: 0.01, seed: 1649963713563718435} }
            ]

            server_filter_stack: [
                { hdlc_packetizer: {} },
                { data_dropper: {rate: 0.01, seed: 1649963713563718436} }
        ]""", config_pb2.ProxyConfig())

        got = asyncio.run(
            self._perform_write(server_config, client_type, client_config,
                                proxy_config, payload))
        self.assertEqual(got, payload)

    @parameterized.expand([
        ("cpp"),
        ("java"),
    ])
    def test_3mb_write_dropped_data(self, client_type):
        resource_id = 12
        server_config = config_pb2.ServerConfig(
            resource_id=resource_id,
            chunk_size_bytes=216,
            pending_bytes=32 * 1024,
            chunk_timeout_seconds=5,
            transfer_service_retries=4,
            extend_window_divisor=32,
        )
        client_config = config_pb2.ClientConfig(
            resource_id=resource_id,
            max_retries=5,
            initial_chunk_timeout_ms=10000,
            chunk_timeout_ms=4000,
        )
        proxy_config = text_format.Parse(
            """
            client_filter_stack: [
                { rate_limiter: {rate: 50000} },
                { hdlc_packetizer: {} },
                { data_dropper: {rate: 0.01, seed: 1649963713563718435} }
            ]

            server_filter_stack: [
                { rate_limiter: {rate: 50000} },
                { hdlc_packetizer: {} },
                { data_dropper: {rate: 0.01, seed: 1649963713563718436} }
        ]""", config_pb2.ProxyConfig())

        payload = random.Random(1649963713563718437).randbytes(3 * 1024 * 1024)
        got = asyncio.run(
            self._perform_write(server_config, client_type, client_config,
                                proxy_config, payload))
        self.assertEqual(got, payload)

    @parameterized.expand([
        ("cpp"),
        ("java"),
    ])
    def test_3mb_write_reordered_data(self, client_type):
        resource_id = 12
        server_config = config_pb2.ServerConfig(
            resource_id=resource_id,
            chunk_size_bytes=216,
            pending_bytes=32 * 1024,
            chunk_timeout_seconds=5,
            transfer_service_retries=4,
            extend_window_divisor=32,
        )
        client_config = config_pb2.ClientConfig(
            resource_id=resource_id,
            max_retries=5,
            initial_chunk_timeout_ms=10000,
            chunk_timeout_ms=4000,
        )
        proxy_config = text_format.Parse(
            """
            client_filter_stack: [
                { rate_limiter: {rate: 50000} },
                { hdlc_packetizer: {} },
                { data_transposer: {rate: 0.005, timeout: 0.5, seed: 1649963713563718435} }
            ]

            server_filter_stack: [
                { rate_limiter: {rate: 50000} },
                { hdlc_packetizer: {} },
                { data_transposer: {rate: 0.005, timeout: 0.5, seed: 1649963713563718435} }
        ]""", config_pb2.ProxyConfig())

        payload = random.Random(1649963713563718437).randbytes(3 * 1024 * 1024)
        got = asyncio.run(
            self._perform_write(server_config, client_type, client_config,
                                proxy_config, payload))
        self.assertEqual(got, payload)


if __name__ == '__main__':
    unittest.main()
