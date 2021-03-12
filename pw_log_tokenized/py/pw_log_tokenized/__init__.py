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
"""Tools for working with tokenized logs."""

from dataclasses import dataclass


def _mask(value: int, start: int, count: int) -> int:
    mask = (1 << count) - 1
    return (value & (mask << start)) >> start


@dataclass(frozen=True)
class Metadata:
    """Parses the metadata payload sent by pw_log_tokenized."""
    _value: int

    log_bits: int = 6
    module_bits: int = 16
    flag_bits: int = 10

    def log_level(self) -> int:
        return _mask(self._value, 0, self.log_bits)

    def module_token(self) -> int:
        return _mask(self._value, self.log_bits, self.module_bits)

    def flags(self) -> int:
        return _mask(self._value, self.log_bits + self.module_bits,
                     self.flag_bits)
