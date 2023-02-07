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

import dataclasses
import re
import unittest

from pw_presubmit import presubmit


class TestFileFilter(unittest.TestCase):
    """Test FileFilter class"""

    @dataclasses.dataclass
    class TestData:
        filter: presubmit.FileFilter
        value: str
        expected: bool

    test_scenarios = (
        TestData(presubmit.FileFilter(endswith=('bar', 'foo')), 'foo', True),
        TestData(presubmit.FileFilter(endswith=('bar', 'boo')), 'foo', False),
        TestData(
            presubmit.FileFilter(exclude=(re.compile('a/.+'),), name=('foo',)),
            '/a/b/c/foo',
            False,
        ),
        TestData(
            presubmit.FileFilter(exclude=(re.compile('x/.+'),), name=('foo',)),
            '/a/b/c/foo',
            True,
        ),
        TestData(
            presubmit.FileFilter(exclude=(re.compile('a+'), re.compile('b+'))),
            'cccc',
            True,
        ),
        TestData(presubmit.FileFilter(name=('foo',)), 'foo', True),
        TestData(presubmit.FileFilter(name=('foo',)), 'food', False),
        TestData(presubmit.FileFilter(name=(re.compile('foo'),)), 'foo', True),
        TestData(
            presubmit.FileFilter(name=(re.compile('foo'),)), 'food', False
        ),
        TestData(presubmit.FileFilter(name=(re.compile('fo+'),)), 'foo', True),
        TestData(presubmit.FileFilter(name=(re.compile('fo+'),)), 'fd', False),
        TestData(
            presubmit.FileFilter(suffix=('.exe',)), 'a/b.py/foo.exe', True
        ),
        TestData(
            presubmit.FileFilter(suffix=('.py',)), 'a/b.py/foo.exe', False
        ),
        TestData(
            presubmit.FileFilter(suffix=('.exe',)), 'a/b.py/foo.py.exe', True
        ),
        TestData(
            presubmit.FileFilter(suffix=('.py',)), 'a/b.py/foo.py.exe', False
        ),
        TestData(presubmit.FileFilter(suffix=('.a', '.b')), 'foo.b', True),
        TestData(presubmit.FileFilter(suffix=('.a', '.b')), 'foo.c', False),
    )

    def test_matches(self):
        for test_num, test_data in enumerate(self.test_scenarios):
            with self.subTest(i=test_num):
                self.assertEqual(
                    test_data.filter.matches(test_data.value),
                    test_data.expected,
                )


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


class ContextTest(unittest.TestCase):
    def test_presubmitcontext(self):  # pylint: disable=no-self-use
        _ = presubmit.PresubmitContext.create_for_testing()

    def test_lucicontext(self):  # pylint: disable=no-self-use
        _ = presubmit.LuciContext.create_for_testing()

    def test_lucitrigger(self):  # pylint: disable=no-self-use
        _ = presubmit.LuciTrigger.create_for_testing()


if __name__ == '__main__':
    unittest.main()
