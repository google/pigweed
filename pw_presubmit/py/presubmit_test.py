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
"""Tests for presubmit tools."""

import unittest

from pw_presubmit import presubmit


def _fake_function_1(_):
    """Fake presubmit function."""


def _fake_function_2(_):
    """Fake presubmit function."""


class ProgramsTest(unittest.TestCase):
    """Tests the presubmit Programs abstraction."""
    def setUp(self):
        self._programs = presubmit.Programs(
            first=[_fake_function_1, (), [(_fake_function_1, )]],
            second=[_fake_function_2],
        )

    def test_empty(self):
        self.assertEqual({}, presubmit.Programs())

    def test_access_present_members(self):
        self.assertEqual('first', self._programs['first'].name)
        self.assertEqual((_fake_function_1, _fake_function_1),
                         tuple(self._programs['first']))

        self.assertEqual('second', self._programs['second'].name)
        self.assertEqual((_fake_function_2, ), tuple(self._programs['second']))

    def test_access_missing_member(self):
        with self.assertRaises(KeyError):
            _ = self._programs['not_there']

    def test_all_steps(self):
        self.assertEqual(
            {
                '_fake_function_1': _fake_function_1,
                '_fake_function_2': _fake_function_2,
            }, self._programs.all_steps())


if __name__ == '__main__':
    unittest.main()
