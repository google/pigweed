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

import pathlib
import subprocess
import tempfile
import time
import unittest

from rules_python.python.runfiles import runfiles

SERVER_PORT = 3300
CLIENT_PORT = 3301


class PwTransferIntegrationTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        # TODO(tpudlik): This is Bazel-only. Support gn, too.
        r = runfiles.Create()
        cls._CLIENT_BINARY = r.Rlocation(
            "pigweed/pw_transfer/integration_test_client")
        cls._PROXY_BINARY = r.Rlocation("pigweed/pw_transfer/proxy")
        cls._SERVER_BINARY = r.Rlocation(
            "pigweed/pw_transfer/integration_test_server")

    def _client_write(self, transfer_id: int, f):
        """Runs client to transfer contents of f to server.

        Blocks until the client exits, and raises an exception on non-zero
        return codes.
        """
        subprocess.run(
            [self._CLIENT_BINARY,
             str(CLIENT_PORT),
             str(transfer_id), f.name],
            check=True)

    def _start_server(self, transfer_id: int, f):
        self._server = subprocess.Popen(
            [self._SERVER_BINARY,
             str(SERVER_PORT),
             str(transfer_id), f.name])

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

    def _perform_write(self, transfer_id: int, payload: bytes) -> bytes:
        """Performs a pw_transfer write.

        Args:
          transfer_id: The transfer ID to use.
          payload: bytes to write

        Returns: Bytes the server has saved after receiving the payload.
        """
        with tempfile.NamedTemporaryFile(
        ) as f_payload, tempfile.NamedTemporaryFile() as f_server_output:
            f_payload.write(payload)
            f_payload.flush()  # Ensure contents are there to read!
            self._start_proxy()
            time.sleep(3)  # TODO: Instead parse proxy logs?
            self._start_server(transfer_id, f_server_output)
            time.sleep(3)  # TODO: Instead parse server logs
            self._client_write(transfer_id, f_payload)

            DEADLINE = 10  # seconds
            returncode = self._server.wait(DEADLINE)
            self.assertEqual(returncode, 0)

            return f_server_output.read()

    def test_write(self):
        transfer_id = 12
        payload = b"some data"
        got = self._perform_write(transfer_id, payload)
        self.assertEqual(got, payload)


if __name__ == '__main__':
    unittest.main()
