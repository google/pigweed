#!/usr/bin/env python3
# Copyright 2024 The Pigweed Authors
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
"""Tests for snapshot processing."""

import base64
import unittest
from pw_snapshot.processor import process_snapshot

_RISCV_EXPECTED_SNAPSHOT = """
        ____ _       __    _____ _   _____    ____  _____ __  ______  ______
       / __ \\ |     / /   / ___// | / /   |  / __ \\/ ___// / / / __ \\/_  __/
      / /_/ / | /| / /    \\__ \\/  |/ / /| | / /_/ /\\__ \\/ /_/ / / / / / /
     / ____/| |/ |/ /    ___/ / /|  / ___ |/ ____/___/ / __  / /_/ / / /
    /_/     |__/|__/____/____/_/ |_/_/  |_/_/    /____/_/ /_/\\____/ /_/
                  /_____/


Snapshot capture reason:
    Example Reason

Reason token:      0x6d617845
Project name:      example_project
Device:            hyper-fast-gshoe
CPU Arch:          RV32I
Device FW version: gShoe-debug-1.2.1-6f23412b+
Snapshot UUID:     00000001

All registers:
mepc       0x20000001
mcause     0x20000002
mstatus    0x20000003
"""

_ARM_EXPECTED_SNAPSHOT = """
        ____ _       __    _____ _   _____    ____  _____ __  ______  ______
       / __ \\ |     / /   / ___// | / /   |  / __ \\/ ___// / / / __ \\/_  __/
      / /_/ / | /| / /    \\__ \\/  |/ / /| | / /_/ /\\__ \\/ /_/ / / / / / /
     / ____/| |/ |/ /    ___/ / /|  / ___ |/ ____/___/ / __  / /_/ / / /
    /_/     |__/|__/____/____/_/ |_/_/  |_/_/    /____/_/ /_/\\____/ /_/
                  /_____/


Snapshot capture reason:
    Example Reason

Reason token:      0x6d617845
Project name:      example_project
Device:            hyper-fast-gshoe
CPU Arch:          ARMV7M
Device FW version: gShoe-debug-1.2.1-6f23412b+
Snapshot UUID:     00000001

Exception caused by a unknown exception.

No active Crash Fault Status Register (CFSR) fields.

All registers:
pc         0x20000001
lr         0x20000002
psr        0x20000003
"""


class ProcessorTest(unittest.TestCase):
    """Tests that the metadata processor produces expected results."""

    def test_riscv_process_snapshot(self):
        """Test processing snapshot of a RISCV CPU"""

        snapshot = base64.b64decode(
            "ggFYCg5FeGFtcGxlIFJlYXNvbhoPZXhhbXBs"
            "ZV9wcm9qZWN0IhtnU2hvZS1kZWJ1Zy0xLjIu"
            "MS02ZjIzNDEyYisyEGh5cGVyLWZhc3QtZ3No"
            "b2U6BAAAAAFABaIBEgiBgICAAhCCgICAAhiD"
            "gICAAg=="
        )

        output = process_snapshot(snapshot)
        self.assertEqual(output, _RISCV_EXPECTED_SNAPSHOT)

    def test_cortexm_process_snapshot(self):
        """Test processing snapshot of a ARM CPU"""

        snapshot = base64.b64decode(
            "ggFYCg5FeGFtcGxlIFJlYXNvbhoPZXhhbXBsZV9wc"
            "m9qZWN0IhtnU2hvZS1kZWJ1Zy0xLjIuMS02ZjIzND"
            "EyYisyEGh5cGVyLWZhc3QtZ3Nob2U6BAAAAAFAAqI"
            "BEgiBgICAAhCCgICAAhiDgICAAg=="
        )

        output = process_snapshot(snapshot)
        self.assertEqual(output, _ARM_EXPECTED_SNAPSHOT)


if __name__ == '__main__':
    unittest.main()
