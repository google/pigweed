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
"""Tests dumped RISCV CPU state."""

import unittest

from pw_cpu_exception_risc_v import exception_analyzer
from pw_cpu_exception_risc_v_protos import cpu_state_pb2
import pw_symbolizer

# pylint: disable=protected-access


class TextDumpTest(unittest.TestCase):
    """Test larger state dumps."""

    def test_registers(self):
        """Validate output of general register dumps."""
        cpu_state_proto = cpu_state_pb2.RiscvCpuState()
        cpu_state_proto.mepc = 0xDFADD966
        cpu_state_proto.mcause = 0xAF2EA98A
        cpu_state_proto.mstatus = 0xF3B235B1
        cpu_state_info = exception_analyzer.RiscvExceptionAnalyzer(
            cpu_state_proto
        )
        expected_dump = '\n'.join(
            (
                'mepc       0xdfadd966',
                'mcause     0xaf2ea98a',
                'mstatus    0xf3b235b1',
            )
        )
        self.assertEqual(cpu_state_info.dump_registers(), expected_dump)

    def test_symbolization(self):
        """Ensure certain registers are symbolized."""
        cpu_state_proto = cpu_state_pb2.RiscvCpuState()
        known_symbols = (
            pw_symbolizer.Symbol(0x0800A200, 'foo()', 'src/foo.c', 41),
        )
        symbolizer = pw_symbolizer.FakeSymbolizer(known_symbols)
        cpu_state_proto.mepc = 0x0800A200
        cpu_state_info = exception_analyzer.RiscvExceptionAnalyzer(
            cpu_state_proto, symbolizer
        )
        expected_dump = '\n'.join(
            ('mepc       0x0800a200 foo() (src/foo.c:41)',)
        )
        self.assertEqual(cpu_state_info.dump_registers(), expected_dump)


if __name__ == '__main__':
    unittest.main()
