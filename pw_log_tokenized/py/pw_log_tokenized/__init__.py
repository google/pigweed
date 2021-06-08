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
import re
from typing import Dict, Mapping


def _mask(value: int, start: int, count: int) -> int:
    mask = (1 << count) - 1
    return (value & (mask << start)) >> start


class Metadata:
    """Parses the metadata payload used by pw_log_tokenized."""
    def __init__(self,
                 value: int,
                 *,
                 log_bits: int = 3,
                 line_bits: int = 11,
                 flag_bits: int = 2,
                 module_bits: int = 16) -> None:
        self.value = value

        self.log_level = _mask(value, 0, log_bits)
        self.line = _mask(value, log_bits, line_bits)
        self.flags = _mask(value, log_bits + line_bits, flag_bits)
        self.module_token = _mask(value, log_bits + line_bits + flag_bits,
                                  module_bits)

    def __repr__(self) -> str:
        return (f'{type(self).__name__}('
                f'log_level={self.log_level}, '
                f'line={self.line}, '
                f'flags={self.flags}, '
                f'module_token={self.module_token})')


class FormatStringWithMetadata:
    """Parses metadata from a log format string with metadata fields."""
    _FIELD_KEY = re.compile(r'■([a-zA-Z]\w*)♦', flags=re.ASCII)

    def __init__(self, string: str) -> None:
        self.raw_string = string
        self.fields: Dict[str, str] = {}

        # Only look for fields if the raw string starts with one.
        if self._FIELD_KEY.match(self.raw_string):
            fields = self._FIELD_KEY.split(self.raw_string)[1:]
            for name, value in zip(fields[::2], fields[1::2]):
                self.fields[name] = value

    @property
    def message(self) -> str:
        """Displays the msg field or the whole string if it is not present."""
        return self.fields.get('msg', self.raw_string)

    @property
    def module(self) -> str:
        return self.fields.get('module', '')

    @property
    def file(self) -> str:
        return self.fields.get('file', '')

    def __repr__(self) -> str:
        return self.message
