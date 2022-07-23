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
"""Tests transfers between the Python client and C++ service."""

from pathlib import Path
import random
import tempfile
from typing import List, Tuple, Union
import unittest

from pw_hdlc import rpc
from pw_rpc import testing, lossy_channel
from pw_status import Status
import pw_transfer
from pw_transfer import transfer_pb2
from pw_transfer_test import test_server_pb2

ITERATIONS = 5
TIMEOUT_S = 1

_DATA_4096B = b'SPAM' * (4096 // len('SPAM'))


class IntegrationTestServer(unittest.TestCase):
    """Test server for transfers between the Python client and C++ service."""
    test_server_command: Tuple[str, ...] = ()
    port: int

    @staticmethod
    def incoming_processor():
        """Optional pre-processor for incoming RPC packets."""
        return None

    @staticmethod
    def outgoing_processor():
        """Optional pre-processor for incoming RPC packets."""
        return None

    def setUp(self) -> None:
        self._tempdir = tempfile.TemporaryDirectory(
            prefix=f'pw_transfer_{self.id().rsplit(".", 1)[-1]}_')
        self.directory = Path(self._tempdir.name)

        command = (*self.test_server_command, str(self.directory))
        self._context = rpc.HdlcRpcLocalServerAndClient(
            command,
            self.port, [transfer_pb2, test_server_pb2],
            outgoing_processor=self.outgoing_processor(),
            incoming_processor=self.incoming_processor())

        service = self._context.client.channel(1).rpcs.pw.transfer.Transfer
        self.manager = pw_transfer.Manager(
            service, default_response_timeout_s=TIMEOUT_S)

        self._test_server = self._context.client.channel(
            1).rpcs.pw.transfer.TestServer

    def tearDown(self) -> None:
        try:
            self._tempdir.cleanup()
        finally:
            if hasattr(self, '_context'):
                self._context.close()

    def transfer_file_path(self, resource_id: int) -> Path:
        return self.directory / str(resource_id)

    def set_content(self, resource_id: int, data: Union[bytes, str]) -> None:
        self.transfer_file_path(resource_id).write_bytes(
            data.encode() if isinstance(data, str) else data)
        self._test_server.ReloadTransferFiles()

    def get_content(self, resource_id: int) -> bytes:
        return self.transfer_file_path(resource_id).read_bytes()


class TransferServiceIntegrationTest(IntegrationTestServer):
    """Tests transfers between the Python client and C++ service."""
    def test_read_unknown_id(self) -> None:
        with self.assertRaises(pw_transfer.Error) as ctx:
            self.manager.read(99)
        self.assertEqual(ctx.exception.status, Status.NOT_FOUND)

    def test_read_empty(self) -> None:
        for _ in range(ITERATIONS):
            self.set_content(24, '')
            self.assertEqual(self.manager.read(24), b'')

    def test_read_single_byte(self) -> None:
        for _ in range(ITERATIONS):
            self.set_content(25, '0')
            self.assertEqual(self.manager.read(25), b'0')

    def test_read_small_amount_of_data(self) -> None:
        for _ in range(ITERATIONS):
            self.set_content(26, 'hunter2')
            self.assertEqual(self.manager.read(26), b'hunter2')

    def test_read_large_amount_of_data(self) -> None:
        for _ in range(ITERATIONS):
            size = 2**13  # TODO(hepler): Increase to 2**14 when it passes.
            self.set_content(27, '~' * size)
            self.assertEqual(self.manager.read(27), b'~' * size)

    def test_write_unknown_id(self) -> None:
        with self.assertRaises(pw_transfer.Error) as ctx:
            self.manager.write(99, '')
        self.assertEqual(ctx.exception.status, Status.NOT_FOUND)

    def test_write_empty(self) -> None:
        for _ in range(ITERATIONS):
            self.set_content(28, 'junk')
            self.manager.write(28, b'')
            self.assertEqual(self.get_content(28), b'')

    def test_write_single_byte(self) -> None:
        for _ in range(ITERATIONS):
            self.set_content(29, 'junk')
            self.manager.write(29, b'$')
            self.assertEqual(self.get_content(29), b'$')

    def test_write_small_amount_of_data(self) -> None:
        for _ in range(ITERATIONS):
            self.set_content(30, 'junk')
            self.manager.write(30, b'file transfer')
            self.assertEqual(self.get_content(30), b'file transfer')

    def test_write_large_amount_of_data(self) -> None:
        for _ in range(ITERATIONS):
            self.set_content(31, 'junk')
            self.manager.write(31, b'*' * 512)
            self.assertEqual(self.get_content(31), b'*' * 512)

    def test_write_very_large_amount_of_data(self) -> None:
        for _ in range(ITERATIONS):
            self.set_content(32, 'junk')

            # Larger than the transfer service's configured pending_bytes.
            self.manager.write(32, _DATA_4096B)
            self.assertEqual(self.get_content(32), _DATA_4096B)

    def test_write_string(self) -> None:
        for _ in range(ITERATIONS):
            # Write a string instead of bytes.
            self.set_content(33, 'junk')
            self.manager.write(33, 'hello world')
            self.assertEqual(self.get_content(33), b'hello world')


class ManualLossyTransferServiceIntegrationTest(IntegrationTestServer):
    """Tests transfers with explicit/manual packet drops and patterns."""
    def setUp(self):
        self._incoming_filter = lossy_channel.ManualPacketFilter()
        self._outgoing_filter = lossy_channel.ManualPacketFilter()
        super().setUp()

    def incoming_processor(self):
        return lossy_channel.LossyChannel('incoming RPC',
                                          self._incoming_filter)

    def outgoing_processor(self):
        return lossy_channel.LossyChannel('outgoing RPC',
                                          self._outgoing_filter)

    def test_write_drop_data_chunks_and_transfer_parameters(self) -> None:
        self.set_content(34, 'junk')

        # Allow the initial packet and first chunk, then drop the second chunk.
        self._outgoing_filter.keep(2)
        self._outgoing_filter.drop(1)

        # Allow the initial transfer parameters updates, then drop the next two.
        self._incoming_filter.keep(1)
        self._incoming_filter.drop(2)

        with self.assertLogs('pw_transfer', 'DEBUG') as logs:
            self.manager.write(34, _DATA_4096B)

        self.assertEqual(self.get_content(34), _DATA_4096B)

        # Verify that the client retried twice.
        messages = [r.getMessage() for r in logs.records]
        retry = f'Received no responses for {TIMEOUT_S:.3f}s; retrying {{}}/3'
        self.assertIn(retry.format(1), messages)
        self.assertIn(retry.format(2), messages)

    def test_write_regularly_drop_packets(self) -> None:
        self.set_content(35, 'junk')

        self._outgoing_filter.drop_every(5)  # drop one per window
        self._incoming_filter.drop_every(3)

        self.manager.write(35, _DATA_4096B)

        self.assertEqual(self.get_content(35), _DATA_4096B)

    def test_write_randomly_drop_packets(self) -> None:
        # Allow lots of retries since there are lots of drops.
        self.manager.max_retries = 9

        for seed in [1, 5678, 600613]:
            self.set_content(seed, 'junk')

            rand = random.Random(seed)
            self._incoming_filter.randomly_drop(3, rand)
            self._outgoing_filter.randomly_drop(3, rand)

            data = bytes(
                rand.randrange(256) for _ in range(rand.randrange(16384)))
            self.manager.write(seed, data)
            self.assertEqual(self.get_content(seed), data)

            self._incoming_filter.reset()
            self._outgoing_filter.reset()


class FuzzyLossTransferServiceIntegrationTest(IntegrationTestServer):
    """Tests transfers with psuedo-random transport failure modes."""
    def setUp(self):
        self._incoming_filter = lossy_channel.RandomLossGenerator(
            duplicated_packet_probability=0,
            max_duplications_per_packet=3,
            out_of_order_probability=0,
            delayed_packet_probability=0,
            delayed_packet_range_ms=(10, 100),
            dropped_packet_probability=0,
            seed=1965342297)
        self._outgoing_filter = lossy_channel.RandomLossGenerator(
            duplicated_packet_probability=0,
            max_duplications_per_packet=3,
            out_of_order_probability=0,
            delayed_packet_probability=0,
            delayed_packet_range_ms=(10, 100),
            dropped_packet_probability=0,
            seed=2513817234)
        super().setUp()

    def incoming_processor(self):
        return lossy_channel.LossyChannel('incoming RPC',
                                          self._incoming_filter)

    def outgoing_processor(self):
        return lossy_channel.LossyChannel('outgoing RPC',
                                          self._outgoing_filter)

    def read_large_amount_of_data(self) -> None:
        for _ in range(ITERATIONS):
            size = 2**13  # TODO(hepler): Increase to 2**14 when it passes.
            self.set_content(27, '~' * size)
            self.assertEqual(self.manager.read(27), b'~' * size)

    def write_very_large_amount_of_data(self) -> None:
        for _ in range(ITERATIONS):
            self.set_content(32, 'junk')

            # Larger than the transfer service's configured pending_bytes.
            self.manager.write(32, _DATA_4096B)
            self.assertEqual(self.get_content(32), _DATA_4096B)

    def test_packet_loss_during_read(self) -> None:
        self._incoming_filter.dropped_packet_probability = 0.1
        self._outgoing_filter.dropped_packet_probability = 0.1
        self.read_large_amount_of_data()

    def test_packet_loss_during_write(self) -> None:
        self._incoming_filter.dropped_packet_probability = 0.1
        self._outgoing_filter.dropped_packet_probability = 0.1
        self.write_very_large_amount_of_data()

    def test_packet_delay_during_read(self) -> None:
        self._incoming_filter.delayed_packet_probability = 0.1
        self._outgoing_filter.delayed_packet_probability = 0.1
        self.read_large_amount_of_data()

    def test_packet_delay_during_write(self) -> None:
        self._incoming_filter.delayed_packet_probability = 0.1
        self._outgoing_filter.delayed_packet_probability = 0.1
        self.write_very_large_amount_of_data()

    def test_packet_duplication_during_read(self) -> None:
        # TODO(amontanez): At 0.1 this fails.
        self._incoming_filter.duplicated_packet_probability = 0.05
        self._outgoing_filter.duplicated_packet_probability = 0.05
        self.read_large_amount_of_data()

    def test_packet_duplication_during_write(self) -> None:
        self._incoming_filter.duplicated_packet_probability = 0.1
        self._outgoing_filter.duplicated_packet_probability = 0.1
        self.write_very_large_amount_of_data()

    def test_packet_reordering_during_read(self) -> None:
        # TODO(amontanez): At 0.05 this fails.
        self._incoming_filter.out_of_order_probability = 0.01
        self._outgoing_filter.out_of_order_probability = 0.01
        self.read_large_amount_of_data()

    def test_packet_reordering_during_write(self) -> None:
        self._incoming_filter.out_of_order_probability = 0.05
        self._outgoing_filter.out_of_order_probability = 0.05
        self.write_very_large_amount_of_data()


def _main(test_server_command: List[str], port: int,
          unittest_args: List[str]) -> None:
    IntegrationTestServer.test_server_command = tuple(test_server_command)
    IntegrationTestServer.port = port

    unittest.main(argv=unittest_args)


if __name__ == '__main__':
    _main(**vars(testing.parse_test_server_args()))
