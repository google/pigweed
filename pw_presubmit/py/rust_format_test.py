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
"""Tests for the rustfmt code formatter."""

import importlib.resources
import shutil
import tempfile
import unittest
from pathlib import Path

from pw_build.runfiles_manager import RunfilesManager
from format_testing_utils import CapturingToolRunner
from pw_presubmit.format.rust import RustfmtFormatter

# Setup paths to test data
_TEST_DATA_DIR = importlib.resources.files('pw_presubmit.format.test_data')
_RUST_TEST_FILE = _TEST_DATA_DIR / 'rust_test_data.rs'
_RUST_GOLDEN_FILE = _TEST_DATA_DIR / 'rust_test_data_golden.rs'
_MALFORMED_FILE = _TEST_DATA_DIR / 'malformed_file.txt'
_RUSTFMT_CONFIG_FILE = _TEST_DATA_DIR / 'rustfmt.toml'


class TestRustfmtFormatter(unittest.TestCase):
    """Tests for the RustfmtFormatter."""

    maxDiff = None

    def setUp(self):
        self.runfiles = RunfilesManager()
        self.runfiles.add_bazel_tool(
            'rustfmt',
            'pw_presubmit.py.rustfmt_runfiles',
        )
        self.runfiles.add_bootstrapped_tool(
            'rustfmt',
            'rustfmt',
            from_shell_path=True,
        )
        self.tool_runner = CapturingToolRunner(
            {
                'rustfmt': self.runfiles['rustfmt'],
            }
        )

    def _rustfmt_path(self) -> str:
        return str(self.runfiles['rustfmt'])

    def test_check_file(self):
        """Test checking a single file in memory."""
        formatter = RustfmtFormatter()
        formatter.run_tool = self.tool_runner

        result = formatter.format_file_in_memory(
            _RUST_TEST_FILE, _RUST_TEST_FILE.read_bytes()
        )

        self.assertTrue(result.ok)
        self.assertIsNone(result.error_message)
        self.assertMultiLineEqual(
            result.formatted_file_contents.decode(),
            _RUST_GOLDEN_FILE.read_text(),
        )
        self.assertEqual(
            self.tool_runner.command_history.pop(0),
            ' '.join(
                (
                    self._rustfmt_path(),
                    '--config-path',
                    str(_RUSTFMT_CONFIG_FILE.resolve()),
                )
            ),
        )

    def test_check_file_error(self):
        """Test checking a malformed file."""
        formatter = RustfmtFormatter()
        formatter.run_tool = self.tool_runner

        result = formatter.format_file_in_memory(
            _MALFORMED_FILE, _MALFORMED_FILE.read_bytes()
        )

        self.assertFalse(result.ok)
        self.assertEqual(result.formatted_file_contents, b'')
        self.assertIn('error:', result.error_message or '')

        self.assertEqual(
            self.tool_runner.command_history.pop(0),
            ' '.join(
                (
                    self._rustfmt_path(),
                    '--config-path',
                    str(_RUSTFMT_CONFIG_FILE.resolve()),
                )
            ),
        )

    def test_fix_files_with_fallback(self):
        """Test fixing multiple files with one malformed file."""
        formatter = RustfmtFormatter()
        formatter.run_tool = self.tool_runner

        with tempfile.TemporaryDirectory() as temp_dir:
            temp_dir_path = Path(temp_dir)
            file_to_fix = temp_dir_path / _RUST_TEST_FILE.name
            shutil.copy(_RUST_TEST_FILE, file_to_fix)

            malformed_file = temp_dir_path / _MALFORMED_FILE.name
            shutil.copy(_MALFORMED_FILE, malformed_file)

            config_file = temp_dir_path / _RUSTFMT_CONFIG_FILE.name
            shutil.copy(_RUSTFMT_CONFIG_FILE, config_file)

            files_to_format = [file_to_fix, malformed_file]
            errors = list(formatter.format_files(files_to_format))

            # Should see three separate commands, one where we try to format
            # both files together, and then the fallback logic that formats
            # them individually to isolate errors.
            self.assertEqual(
                self.tool_runner.command_history.pop(0),
                ' '.join(
                    (
                        self._rustfmt_path(),
                        str(file_to_fix),
                        str(malformed_file),
                    )
                ),
            )

            self.assertEqual(
                self.tool_runner.command_history.pop(0),
                ' '.join(
                    (
                        self._rustfmt_path(),
                        str(file_to_fix),
                    )
                ),
            )

            self.assertEqual(
                self.tool_runner.command_history.pop(0),
                ' '.join(
                    (
                        self._rustfmt_path(),
                        str(malformed_file),
                    )
                ),
            )

            # Check good build file.
            self.assertMultiLineEqual(
                file_to_fix.read_text(), _RUST_GOLDEN_FILE.read_text()
            )

            # Check malformed file.
            self.assertEqual(len(errors), 1)
            malformed_files = [malformed_file]
            for file_path, error in errors:
                self.assertIn(file_path, malformed_files)
                self.assertFalse(error.ok)
                self.assertTrue(error.error_message.startswith('error:'))


if __name__ == '__main__':
    unittest.main()
