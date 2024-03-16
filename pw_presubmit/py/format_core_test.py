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
"""Tests for the formatter core."""

from pathlib import Path
from tempfile import TemporaryDirectory
from typing import Dict, List
import unittest

from pw_presubmit.format.core import (
    FileChecker,
    FormattedDiff,
    FormattedFileContents,
)


class FakeFileChecker(FileChecker):
    FORMAT_MAP = {
        'foo': 'bar',
        'bar': 'bar',
        'baz': '\nbaz\n',
        'new\n': 'newer\n',
    }

    def format_file_in_memory(
        self, file_path: Path, file_contents: bytes
    ) -> FormattedFileContents:
        error = ''
        formatted = self.FORMAT_MAP.get(file_contents.decode(), None)
        if formatted is None:
            error = f'I do not know how to "{file_contents.decode()}".'
        return FormattedFileContents(
            ok=not error,
            formatted_file_contents=formatted.encode()
            if formatted is not None
            else b'',
            error_message=error,
        )


def _check_files(
    formatter: FileChecker, file_contents: Dict[str, str], dry_run=False
) -> List[FormattedDiff]:
    with TemporaryDirectory() as tmp:
        paths = []
        for f in file_contents.keys():
            file_path = Path(tmp) / f
            file_path.write_bytes(file_contents[f].encode())
            paths.append(file_path)

        return list(formatter.get_formatting_diffs(paths, dry_run))


class TestFormatCore(unittest.TestCase):
    """Tests for the format core."""

    def setUp(self) -> None:
        self.formatter = FakeFileChecker()

    def test_check_files(self):
        """Tests that check_files() produces diffs as intended."""
        file_contents = {
            'foo.txt': 'foo',
            'bar.txt': 'bar',
            'baz.txt': 'baz',
            'yep.txt': 'new\n',
        }
        expected_diffs = {
            'foo.txt': '\n'.join(
                (
                    '-foo',
                    '+bar',
                    ' No newline at end of file',
                )
            ),
            'baz.txt': '\n'.join(
                (
                    '+',
                    ' baz',
                    '-No newline at end of file',
                )
            ),
            'yep.txt': '\n'.join(
                (
                    '-new',
                    '+newer',
                )
            ),
        }

        for result in _check_files(self.formatter, file_contents):
            filename = result.file_path.name
            self.assertIn(filename, expected_diffs)
            self.assertTrue(result.ok)
            lines = result.diff.splitlines()
            self.assertEqual(
                lines.pop(0), f'--- {result.file_path}  (original)'
            )
            self.assertEqual(
                lines.pop(0), f'+++ {result.file_path}  (reformatted)'
            )
            self.assertTrue(lines.pop(0).startswith('@@'))

            self.assertMultiLineEqual(
                '\n'.join(lines), expected_diffs[filename]
            )
            expected_diffs.pop(filename)

        self.assertFalse(expected_diffs)

    def test_check_files_error(self):
        """Tests that check_files() propagates error messages."""
        file_contents = {
            'foo.txt': 'broken',
            'bar.txt': 'bar',
        }
        expected_errors = {
            'foo.txt': '\n'.join(('I do not know how to "broken".',)),
        }
        for result in _check_files(self.formatter, file_contents):
            filename = result.file_path.name
            self.assertIn(filename, expected_errors)
            self.assertFalse(result.ok)
            self.assertEqual(result.diff, '')
            self.assertEqual(result.error_message, expected_errors[filename])
            expected_errors.pop(filename)

        self.assertFalse(expected_errors)

    def test_check_files_dry_run(self):
        """Tests that check_files() dry run produces no delta."""
        file_contents = {
            'foo.txt': 'foo',
            'bar.txt': 'bar',
            'baz.txt': 'baz',
            'yep.txt': 'new\n',
        }
        result = _check_files(self.formatter, file_contents, dry_run=True)
        self.assertFalse(result)


if __name__ == '__main__':
    unittest.main()
