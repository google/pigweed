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
"""Tools to analyze RISCV CPU state context captured during an exception."""

from typing import Optional

from pw_cpu_exception_risc_v_protos import cpu_state_pb2
import pw_symbolizer

# These registers are symbolized when dumped.
_SYMBOLIZED_REGISTERS = (
    'mepc',
    'ra',
)


class RiscvExceptionAnalyzer:
    """This class provides helper functions to dump a RiscvCpuState proto."""

    def __init__(
        self, cpu_state, symbolizer: Optional[pw_symbolizer.Symbolizer] = None
    ):
        self._cpu_state = cpu_state
        self._symbolizer = symbolizer

    def dump_registers(self) -> str:
        """Dumps all captured CPU registers as a multi-line string."""
        registers = []
        for field in self._cpu_state.DESCRIPTOR.fields:
            if self._cpu_state.HasField(field.name):
                register_value = getattr(self._cpu_state, field.name)
                register_str = f'{field.name:<10} 0x{register_value:08x}'
                if (
                    self._symbolizer is not None
                    and field.name in _SYMBOLIZED_REGISTERS
                ):
                    symbol = self._symbolizer.symbolize(register_value)
                    if symbol.name:
                        register_str += f' {symbol}'
                registers.append(register_str)
        return '\n'.join(registers)

    def __str__(self):
        dump = []
        dump.extend(
            (
                'All registers:',
                self.dump_registers(),
            )
        )
        return '\n'.join(dump)


def process_snapshot(
    serialized_snapshot: bytes,
    symbolizer: Optional[pw_symbolizer.Symbolizer] = None,
) -> str:
    """Returns the stringified result of a SnapshotCpuStateOverlay message run
    though a RiscvExceptionAnalyzer.
    """
    snapshot = cpu_state_pb2.SnapshotCpuStateOverlay()
    snapshot.ParseFromString(serialized_snapshot)

    if snapshot.HasField('riscv_cpu_state'):
        state_analyzer = RiscvExceptionAnalyzer(
            snapshot.riscv_cpu_state, symbolizer
        )
        return f'{state_analyzer}\n'

    return ''
