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
"""Tests for file filtering."""

import dataclasses
import re
import unittest

from pw_cli.file_filter import FileFilter


class TestFileFilter(unittest.TestCase):
    """Test FileFilter class"""

    @dataclasses.dataclass
    class TestData:
        filter: FileFilter
        value: str
        expected: bool

    test_scenarios = (
        TestData(FileFilter(endswith=('bar', 'foo')), 'foo', True),
        TestData(FileFilter(endswith=('bar', 'boo')), 'foo', False),
        TestData(
            FileFilter(exclude=(re.compile('a/.+'),), name=('foo',)),
            '/a/b/c/foo',
            False,
        ),
        TestData(
            FileFilter(exclude=(re.compile('x/.+'),), name=('foo',)),
            '/a/b/c/foo',
            True,
        ),
        TestData(
            FileFilter(exclude=(re.compile('a+'), re.compile('b+'))),
            'cccc',
            True,
        ),
        TestData(FileFilter(name=('foo',)), 'foo', True),
        TestData(FileFilter(name=('foo',)), 'food', False),
        TestData(FileFilter(name=(re.compile('foo'),)), 'foo', True),
        TestData(FileFilter(name=(re.compile('foo'),)), 'food', False),
        TestData(FileFilter(name=(re.compile('fo+'),)), 'foo', True),
        TestData(FileFilter(name=(re.compile('fo+'),)), 'fd', False),
        TestData(FileFilter(suffix=('.exe',)), 'a/b.py/foo.exe', True),
        TestData(FileFilter(suffix=('.py',)), 'a/b.py/foo.exe', False),
        TestData(FileFilter(suffix=('.exe',)), 'a/b.py/foo.py.exe', True),
        TestData(FileFilter(suffix=('.py',)), 'a/b.py/foo.py.exe', False),
        TestData(FileFilter(suffix=('.a', '.b')), 'foo.b', True),
        TestData(FileFilter(suffix=('.a', '.b')), 'foo.c', False),
    )

    def test_matches(self):
        for test_num, test_data in enumerate(self.test_scenarios):
            with self.subTest(i=test_num):
                self.assertEqual(
                    test_data.filter.matches(test_data.value),
                    test_data.expected,
                )


if __name__ == '__main__':
    unittest.main()
