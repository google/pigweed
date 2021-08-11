#!/usr/bin/env python3
# Copyright 2021 The Pigweed Authors
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
"""Tests transfers between the Python client and C++ service."""

from typing import List, Tuple
import unittest

from pw_hdlc import rpc
from pw_rpc import testing
from pw_transfer import transfer, transfer_pb2

PORT = 33001
TRANSFER_ID = 99
ITERATIONS = 5


class TransferServiceIntegrationTest(unittest.TestCase):
    """Tests transfers between the Python client and C++ service."""
    test_server_command: Tuple[str, ...] = ()

    def setUp(self) -> None:
        self._context = rpc.HdlcRpcLocalServerAndClient(
            self.test_server_command, PORT, [transfer_pb2])
        service = self._context.client.channel(1).rpcs.pw.transfer.Transfer
        self.manager = transfer.Manager(service)

    def set_contents(self, data: str) -> None:
        assert self._context.server.stdin is not None
        self._context.server.stdin.write(data.encode() + b'\0')
        self._context.server.stdin.flush()

    def tearDown(self) -> None:
        if hasattr(self, '_context'):
            self._context.close()

    def test_read_empty(self) -> None:
        for _ in range(ITERATIONS):
            self.assertEqual(self.manager.read(TRANSFER_ID), b'')

    def test_read_single_byte(self) -> None:
        self.set_contents('\1')

        for _ in range(ITERATIONS):
            self.assertEqual(self.manager.read(TRANSFER_ID), b'\1')

    def test_read_small_amount_of_data(self) -> None:
        self.set_contents('hunter2')

        for _ in range(ITERATIONS):
            self.assertEqual(self.manager.read(TRANSFER_ID), b'hunter2')

    def test_read_large_amount_of_data(self) -> None:
        self.set_contents('~' * 512)

        for _ in range(ITERATIONS):
            self.assertEqual(self.manager.read(TRANSFER_ID), b'~' * 512)


def _main(test_server_command: List[str], unittest_args: List[str]) -> None:
    TransferServiceIntegrationTest.test_server_command = tuple(
        test_server_command)
    unittest.main(argv=unittest_args)


if __name__ == '__main__':
    _main(**vars(testing.parse_test_server_args()))
