# Copyright 2023 The Pigweed Authors
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
"""Tests parsing the sized-entry ring buffer's in-memory representation."""

import unittest

from pw_containers import variable_length_entry_deque


def _buffer(head: int, tail: int, data: bytes) -> bytes:
    return (
        b''.join(i.to_bytes(4, 'little') for i in [len(data), head, tail])
        + data
    )


class TestEncodeTokenized(unittest.TestCase):
    def test_one_entry(self) -> None:
        self.assertEqual(
            list(
                variable_length_entry_deque.parse(
                    _buffer(1, 6, b'?\0041234?890')
                )
            ),
            [b'1234'],
        )

    def test_two_entries(self) -> None:
        self.assertEqual(
            list(
                variable_length_entry_deque.parse(
                    _buffer(1, 7, b'?\00212\00234?90')
                )
            ),
            [b'12', b'34'],
        )

    def test_two_entries_wrapped(self) -> None:
        self.assertEqual(
            list(
                variable_length_entry_deque.parse(
                    _buffer(6, 4, b'4\00212?x\004123')
                )
            ),
            [b'1234', b'12'],
        )


if __name__ == '__main__':
    unittest.main()
