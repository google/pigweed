# Copyright 2025 The Pigweed Authors
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
"""Tests for the TrailingSpaceFormatter."""

from pathlib import Path
import tempfile
import unittest

from pw_presubmit.format.whitespace import TrailingSpaceFormatter


class TrailingSpaceFormatterTest(unittest.TestCase):
    """Tests for the TrailingSpaceFormatter."""

    def setUp(self):
        self.formatter = TrailingSpaceFormatter()

    def test_format_file_in_memory_no_changes(self):
        """Tests formatting a file with no trailing whitespace."""
        content = b'hello world\nno trailing space here\n'
        result = self.formatter.format_file_in_memory(Path('fake.txt'), content)
        self.assertEqual(result.formatted_file_contents, content)
        self.assertTrue(result.ok)

    def test_format_file_in_memory_with_trailing_space(self):
        """Tests formatting a file with trailing whitespace."""
        content = b'hello world  \nwith trailing space here\t\n'
        expected = b'hello world\nwith trailing space here\n'
        result = self.formatter.format_file_in_memory(Path('fake.txt'), content)
        self.assertEqual(result.formatted_file_contents, expected)
        self.assertTrue(result.ok)

    def test_format_file_in_memory_mixed_content(self):
        """Tests formatting a file with mixed lines."""
        content = b'line one\nline two  \nline three\t\nline four\n'
        expected = b'line one\nline two\nline three\nline four\n'
        result = self.formatter.format_file_in_memory(Path('fake.txt'), content)
        self.assertEqual(result.formatted_file_contents, expected)
        self.assertTrue(result.ok)

    def test_format_file_in_memory_empty_file(self):
        """Tests formatting an empty file."""
        content = b''
        result = self.formatter.format_file_in_memory(Path('fake.txt'), content)
        self.assertEqual(result.formatted_file_contents, b'')
        self.assertTrue(result.ok)

    def test_format_file_in_memory_lines_with_only_whitespace(self):
        """Tests formatting a file with lines containing only whitespace."""
        content = b'  \n\t\n'
        expected = b'\n\n'
        result = self.formatter.format_file_in_memory(Path('fake.txt'), content)
        self.assertEqual(result.formatted_file_contents, expected)
        self.assertTrue(result.ok)

    def test_format_file_no_changes(self):
        """Tests formatting a file on disk with no trailing whitespace."""
        with tempfile.TemporaryDirectory() as tempdir:
            file_path = Path(tempdir) / 'test.txt'
            original_content = b'hello\nworld\n'
            file_path.write_bytes(original_content)

            status = self.formatter.format_file(file_path)
            self.assertTrue(status.ok)
            self.assertIsNone(status.error_message)

            new_content = file_path.read_bytes()
            self.assertEqual(new_content, original_content)

    def test_format_file_with_changes(self):
        """Tests formatting a file on disk with trailing whitespace."""
        with tempfile.TemporaryDirectory() as tempdir:
            file_path = Path(tempdir) / 'test.txt'
            original_content = b'hello  \nworld\t\n'
            file_path.write_bytes(original_content)

            status = self.formatter.format_file(file_path)
            self.assertTrue(status.ok)
            self.assertIsNone(status.error_message)

            new_content = file_path.read_bytes()
            expected_content = b'hello\nworld\n'
            self.assertEqual(new_content, expected_content)


if __name__ == '__main__':
    unittest.main()
