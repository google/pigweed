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
"""Tests for the json formatter."""

import importlib.resources
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest

from pw_presubmit.format.json import JsonFormatter

_TEST_DATA_FILES = importlib.resources.files('pw_presubmit.format.test_data')

_TEST_SRC_FILE = _TEST_DATA_FILES / 'json_test_data.json'
_TEST_GOLDEN = _TEST_DATA_FILES / 'json_test_data_golden.json'
_MALFORMED_FILE = _TEST_DATA_FILES / 'malformed_file.txt'


class TestJsonFormatter(unittest.TestCase):
    """Tests for the JsonFormatter."""

    def test_check_file(self):
        formatter = JsonFormatter()

        result = formatter.format_file_in_memory(
            _TEST_SRC_FILE, _TEST_SRC_FILE.read_bytes()
        )
        self.assertTrue(result.ok)
        self.assertEqual(result.error_message, None)

        self.assertMultiLineEqual(
            result.formatted_file_contents.decode(), _TEST_GOLDEN.read_text()
        )

    def test_check_file_malformed(self):
        formatter = JsonFormatter()

        result = formatter.format_file_in_memory(
            _MALFORMED_FILE, _MALFORMED_FILE.read_bytes()
        )
        self.assertFalse(result.ok)
        self.assertIn('Expecting value', result.error_message)

    def test_fix_file(self):
        formatter = JsonFormatter()

        with TemporaryDirectory() as temp_dir:
            file_to_fix = Path(temp_dir) / _TEST_SRC_FILE.name
            file_to_fix.write_bytes(_TEST_SRC_FILE.read_bytes())

            status = formatter.format_file(file_to_fix)

            self.assertTrue(status.ok)
            self.assertIsNone(status.error_message)

            self.assertMultiLineEqual(
                file_to_fix.read_text(), _TEST_GOLDEN.read_text()
            )

    def test_fix_file_malformed(self):
        formatter = JsonFormatter()

        with TemporaryDirectory() as temp_dir:
            file_to_fix = Path(temp_dir) / _MALFORMED_FILE.name
            file_to_fix.write_bytes(_MALFORMED_FILE.read_bytes())

            status = formatter.format_file(file_to_fix)

            self.assertFalse(status.ok)
            self.assertIn('Expecting value', status.error_message)


if __name__ == '__main__':
    unittest.main()
