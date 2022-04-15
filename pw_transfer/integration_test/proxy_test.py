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
"""Unit test for proxy.py"""

import abc
import asyncio
import time
from typing import List
import unittest

import proxy


class MockRng(abc.ABC):
    def __init__(self, results: List[float]):
        self._results = results

    def uniform(self, from_val: float, to_val: float) -> float:
        val_range = to_val - from_val
        val = self._results.pop()
        val *= val_range
        val += from_val
        return val


class ProxyTest(unittest.IsolatedAsyncioTestCase):
    async def test_transposer_simple(self):
        sent_packets: List[bytes] = []

        # Async helper so DataTransposer can await on it.
        async def append(list: List[bytes], data: bytes):
            list.append(data)

        transposer = proxy.DataTransposer(
            lambda data: append(sent_packets, data),
            name="test",
            rate=0.5,
            timeout=100,
            seed=1234567890)
        transposer._rng = MockRng([0.6, 0.4])
        await transposer.process(b'aaaaaaaaaa')
        await transposer.process(b'bbbbbbbbbb')

        # Give the transposer task time to process the data.
        await asyncio.sleep(0.05)

        self.assertEqual(sent_packets, [b'bbbbbbbbbb', b'aaaaaaaaaa'])

    async def test_transposer_timeout(self):
        sent_packets: List[bytes] = []

        # Async helper so DataTransposer can await on it.
        async def append(list: List[bytes], data: bytes):
            list.append(data)

        transposer = proxy.DataTransposer(
            lambda data: append(sent_packets, data),
            name="test",
            rate=0.5,
            timeout=0.100,
            seed=1234567890)
        transposer._rng = MockRng([0.4, 0.6])
        await transposer.process(b'aaaaaaaaaa')

        # Even though this should be transposed, there is no following data so
        # the transposer should timout and send this in-order.
        await transposer.process(b'bbbbbbbbbb')

        # Give the transposer time to timeout.
        await asyncio.sleep(0.5)

        self.assertEqual(sent_packets, [b'aaaaaaaaaa', b'bbbbbbbbbb'])


if __name__ == '__main__':
    unittest.main()
