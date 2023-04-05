#!/usr/bin/env python3
# Copyright 2020 The Pigweed Authors
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
"""Tests for general purpose tools."""

import unittest

from pw_presubmit import tools


class FlattenTest(unittest.TestCase):
    """Tests the flatten function, which flattens iterables."""

    def test_empty(self):
        self.assertEqual([], list(tools.flatten()))
        self.assertEqual([], list(tools.flatten([])))
        self.assertEqual([], list(tools.flatten([], ())))
        self.assertEqual([], list(tools.flatten([[], (), [[]]], ((), []))))

    def test_no_nesting(self):
        self.assertEqual(
            ['a', 'bcd', 123, 45.6], list(tools.flatten('a', 'bcd', 123, 45.6))
        )
        self.assertEqual(
            ['a', 'bcd', 123, 45.6],
            list(tools.flatten(['a', 'bcd', 123, 45.6])),
        )
        self.assertEqual(
            ['a', 'bcd', 123, 45.6],
            list(tools.flatten(['a', 'bcd'], [123, 45.6])),
        )

    def test_nesting(self):
        self.assertEqual(
            ['a', 'bcd', 123, 45.6],
            list(tools.flatten('a', ['bcd'], [123], 45.6)),
        )
        self.assertEqual(
            ['a', 'bcd', 123, 45.6],
            list(tools.flatten([['a', ('bcd', [123])], 45.6])),
        )
        self.assertEqual(
            ['a', 'bcd', 123, 45.6],
            list(tools.flatten([('a', 'bcd')], [[[[123]]], 45.6])),
        )


class PluralTest(unittest.TestCase):
    """Test the plural function, which adds an 's' to nouns."""

    def test_single_list(self):
        self.assertEqual('1 item', tools.plural([1], 'item'))

    def test_single_count(self):
        self.assertEqual('1 item', tools.plural(1, 'item'))

    def test_multiple_list(self):
        self.assertEqual('3 items', tools.plural([1, 2, 3], 'item'))

    def test_multiple_count(self):
        self.assertEqual('3 items', tools.plural(3, 'item'))

    def test_single_these(self):
        self.assertEqual('this 1 item', tools.plural(1, 'item', these=True))

    def test_multiple_these(self):
        self.assertEqual('these 3 items', tools.plural(3, 'item', these=True))

    def test_single_are(self):
        self.assertEqual('1 item is', tools.plural(1, 'item', are=True))

    def test_multiple_are(self):
        self.assertEqual('3 items are', tools.plural(3, 'item', are=True))

    def test_single_exist(self):
        self.assertEqual('1 item exists', tools.plural(1, 'item', exist=True))

    def test_multiple_exist(self):
        self.assertEqual('3 items exist', tools.plural(3, 'item', exist=True))

    def test_single_y(self):
        self.assertEqual('1 thingy', tools.plural(1, 'thingy'))

    def test_multiple_y(self):
        self.assertEqual('3 thingies', tools.plural(3, 'thingy'))

    def test_single_s(self):
        self.assertEqual('1 bus', tools.plural(1, 'bus'))

    def test_multiple_s(self):
        self.assertEqual('3 buses', tools.plural(3, 'bus'))

    def test_format_hex(self):
        self.assertEqual(
            '14 items',
            tools.plural(20, 'item', count_format='x'),
        )


if __name__ == '__main__':
    unittest.main()
