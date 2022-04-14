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
"""Cross-platform test of pw_transfer.

Usage:

   bazel run pw_transfer:cross_platform_integration_test

"""
from parameterized import parameterized
import pathlib
import subprocess
import tempfile
import time
import unittest

from pigweed.pw_transfer.integration_test import config_pb2
from rules_python.python.runfiles import runfiles

SERVER_PORT = 3300
CLIENT_PORT = 3301


class PwTransferIntegrationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # TODO(tpudlik): This is Bazel-only. Support gn, too.
        r = runfiles.Create()
        cls._JAVA_CLIENT_BINARY = r.Rlocation(
            "pigweed/pw_transfer/java_integration_test_client")
        cls._CPP_CLIENT_BINARY = r.Rlocation(
            "pigweed/pw_transfer/integration_test_client")
        cls._PROXY_BINARY = r.Rlocation("pigweed/pw_transfer/proxy")
        cls._SERVER_BINARY = r.Rlocation(
            "pigweed/pw_transfer/integration_test_server")
        cls._CLIENT_BINARY = {
            "cpp": cls._CPP_CLIENT_BINARY,
            "java": cls._JAVA_CLIENT_BINARY,
        }

    def _client_write(self, client_type: str, config: config_pb2.ClientConfig):
        """Runs client with the specified config.

        Blocks until the client exits, and raises an exception on non-zero
        return codes.
        """
        print(f"Starting client with config {config}")
        subprocess.run([self._CLIENT_BINARY[client_type],
                        str(CLIENT_PORT)],
                       input=str(config),
                       text=True,
                       check=True)

    def _start_server(self, config: config_pb2.ServerConfig):
        self._server = subprocess.Popen(
            [self._SERVER_BINARY, str(SERVER_PORT)], stdin=subprocess.PIPE)
        self._server.stdin.write(str(config).encode('ascii'))
        self._server.stdin.flush()
        self._server.stdin.close()

    def _start_proxy(self):
        self._proxy = subprocess.Popen([
            self._PROXY_BINARY, "--server-port",
            str(SERVER_PORT), "--client-port",
            str(CLIENT_PORT)
        ])

    def tearDown(self):
        """Magic unittest method, called after every test.

        After each test, ensures we've cleaned up the server and proxy
        processes.
        """
        super().tearDown()
        self._server.terminate()
        self._server.wait()
        self._proxy.terminate()
        self._proxy.wait()

    def _perform_write(self, server_config: config_pb2.ServerConfig,
                       client_type: str,
                       client_config: config_pb2.ClientConfig,
                       payload) -> bytes:
        """Performs a pw_transfer write.

        Args:
          resource_id: The transfer resource ID to use.
          payload: bytes to write

        Returns: Bytes the server has saved after receiving the payload.
        """
        with tempfile.NamedTemporaryFile(
        ) as f_payload, tempfile.NamedTemporaryFile() as f_server_output:
            server_config.file = f_server_output.name
            client_config.file = f_payload.name

            f_payload.write(payload)
            f_payload.flush()  # Ensure contents are there to read!

            self._start_proxy()
            time.sleep(3)  # TODO: Instead parse proxy logs?

            self._start_server(server_config)
            time.sleep(3)  # TODO: Instead parse server logs

            self._client_write(client_type, client_config)

            DEADLINE = 10  # seconds
            returncode = self._server.wait(DEADLINE)
            self.assertEqual(returncode, 0)

            return f_server_output.read()

    @parameterized.expand([
        ("cpp"),
        ("java"),
    ])
    def test_client_write(self, client_type):
        resource_id = 12
        payload = b"some data"
        server_config = config_pb2.ServerConfig(
            resource_id=resource_id,
            chunk_size_bytes=216,
            pending_bytes=2 * 1024,
            chunk_timeout_seconds=5,
            transfer_service_retries=4,
            extend_window_divisor=32,
        )
        client_config = config_pb2.ClientConfig(resource_id=resource_id)
        got = self._perform_write(server_config, client_type, client_config,
                                  payload)
        self.assertEqual(got, payload)


if __name__ == '__main__':
    unittest.main()
