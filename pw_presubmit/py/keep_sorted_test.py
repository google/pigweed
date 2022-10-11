#!/usr/bin/env python3
# Copyright 2022 The Pigweed Authors
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
"""Tests for keep_sorted."""

from pathlib import Path
import tempfile
import unittest
from unittest.mock import MagicMock

from pw_presubmit import keep_sorted

# Only include these literals here so keep_sorted doesn't try to reorder later
# test lines.
START = keep_sorted.START
END = keep_sorted.END

# pylint: disable=attribute-defined-outside-init


class TestKeepSorted(unittest.TestCase):
    """Test KeepSorted class"""
    def _run(self, contents: str) -> None:
        self.ctx = MagicMock()
        self.ctx.fail = MagicMock()

        with tempfile.TemporaryDirectory() as tempdir:
            path = Path(tempdir) / 'foo'

            with path.open('w') as outs:
                outs.write(contents)

            # pylint: disable=protected-access
            self.sorter = keep_sorted._FileSorter(self.ctx, path)

            # pylint: enable=protected-access

            self.sorter.sort()

            # Truncate the file so it's obvious whether write() changed
            # anything.
            with path.open('w') as outs:
                outs.write('')

            self.sorter.write(path)
            with path.open() as ins:
                self.contents = ins.read()

    def test_missing_end(self) -> None:
        with self.assertRaises(keep_sorted.KeepSortedParsingError):
            self._run(f'{START}\n')

    def test_missing_start(self) -> None:
        with self.assertRaises(keep_sorted.KeepSortedParsingError):
            self._run(f'{END}: end\n')

    def test_repeated_start(self) -> None:
        with self.assertRaises(keep_sorted.KeepSortedParsingError):
            self._run(f'{START}\n{START}\n')

    def test_unrecognized_directive(self) -> None:
        with self.assertRaises(keep_sorted.KeepSortedParsingError):
            self._run(f'{START} foo bar baz\n2\n1\n{END}\n')

    def test_repeated_valid_directive(self) -> None:
        with self.assertRaises(keep_sorted.KeepSortedParsingError):
            self._run(f'{START} ignore-case ignore-case\n2\n1\n{END}\n')

    def test_already_sorted(self) -> None:
        self._run(f'{START}\n1\n2\n3\n4\n{END}\n')
        self.ctx.fail.assert_not_called()
        self.assertEqual(self.contents, '')

    def test_not_sorted(self) -> None:
        self._run(f'{START}\n4\n3\n2\n1\n{END}\n')
        self.ctx.fail.assert_called()
        self.assertEqual(self.contents, f'{START}\n1\n2\n3\n4\n{END}\n')

    def test_prefix_sorted(self) -> None:
        self._run(f'foo\nbar\n{START}\n1\n2\n{END}\n')
        self.ctx.fail.assert_not_called()
        self.assertEqual(self.contents, '')

    def test_prefix_not_sorted(self) -> None:
        self._run(f'foo\nbar\n{START}\n2\n1\n{END}\n')
        self.ctx.fail.assert_called()
        self.assertEqual(self.contents, f'foo\nbar\n{START}\n1\n2\n{END}\n')

    def test_suffix_sorted(self) -> None:
        self._run(f'{START}\n1\n2\n{END}\nfoo\nbar\n')
        self.ctx.fail.assert_not_called()
        self.assertEqual(self.contents, '')

    def test_suffix_not_sorted(self) -> None:
        self._run(f'{START}\n2\n1\n{END}\nfoo\nbar\n')
        self.ctx.fail.assert_called()
        self.assertEqual(self.contents, f'{START}\n1\n2\n{END}\nfoo\nbar\n')

    def test_not_sorted_case_sensitive(self) -> None:
        self._run(f'{START}\na\nD\nB\nc\n{END}\n')
        self.ctx.fail.assert_called()
        self.assertEqual(self.contents, f'{START}\nB\nD\na\nc\n{END}\n')

    def test_not_sorted_case_insensitive(self) -> None:
        self._run(f'{START} ignore-case\na\nD\nB\nc\n{END}\n')
        self.ctx.fail.assert_called()
        self.assertEqual(self.contents,
                         f'{START} ignore-case\na\nB\nc\nD\n{END}\n')


if __name__ == '__main__':
    unittest.main()
