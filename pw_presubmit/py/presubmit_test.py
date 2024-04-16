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


def _all_substeps(program):
    substeps = {}
    for step in program:
        # pylint: disable=protected-access
        for sub in step.substeps():
            substeps[sub.name or step.name] = sub._func
        # pylint: enable=protected-access
    return substeps


class ProgramsTest(unittest.TestCase):
    """Tests the presubmit Programs abstraction."""

    def setUp(self):
        self._programs = presubmit.Programs(
            first=[_fake_function_1, (), [(_fake_function_2,)]],
            second=[_fake_function_2],
        )

    def test_empty(self):
        self.assertEqual({}, presubmit.Programs())

    def test_access_present_members_first(self):
        self.assertEqual('first', self._programs['first'].name)
        self.assertEqual(
            ('_fake_function_1', '_fake_function_2'),
            tuple(x.name for x in self._programs['first']),
        )

        self.assertEqual(2, len(self._programs['first']))
        substeps = _all_substeps(
            self._programs['first']  # pylint: disable=protected-access
        ).values()
        self.assertEqual(2, len(substeps))
        self.assertEqual((_fake_function_1, _fake_function_2), tuple(substeps))

    def test_access_present_members_second(self):
        self.assertEqual('second', self._programs['second'].name)
        self.assertEqual(
            ('_fake_function_2',),
            tuple(x.name for x in self._programs['second']),
        )

        self.assertEqual(1, len(self._programs['second']))
        substeps = _all_substeps(
            self._programs['second']  # pylint: disable=protected-access
        ).values()
        self.assertEqual(1, len(substeps))
        self.assertEqual((_fake_function_2,), tuple(substeps))

    def test_access_missing_member(self):
        with self.assertRaises(KeyError):
            _ = self._programs['not_there']

    def test_all_steps(self):
        all_steps = self._programs.all_steps()
        self.assertEqual(len(all_steps), 2)
        all_substeps = _all_substeps(all_steps.values())
        self.assertEqual(len(all_substeps), 2)

        # pylint: disable=protected-access
        self.assertEqual(all_substeps['_fake_function_1'], _fake_function_1)
        self.assertEqual(all_substeps['_fake_function_2'], _fake_function_2)
        # pylint: enable=protected-access


if __name__ == '__main__':
    unittest.main()
