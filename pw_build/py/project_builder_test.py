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
"""Tests for the pw_build.project_builder module."""

import unittest

from pw_build.project_builder import check_ansi_codes


class TestCheckAnsiCodes(unittest.TestCase):
    """Tests for project_builder.check_ansi_codes."""

    def test_empty(self):
        """Tests setting and getting the GN target name and type."""
        result = check_ansi_codes('')
        self.assertEqual(result, None)

    def test_code_reset(self):
        result = check_ansi_codes('\x1b[11mHello\x1b[0m')
        self.assertEqual(result, None)

    def test_code_not_reset(self):
        result = check_ansi_codes('\x1b[11mHello')
        self.assertEqual(result, ['\x1b[11m'])

    def test_code_code_no_reset(self):
        result = check_ansi_codes('\x1b[30;47mHello\x1b[1;31mWorld')
        self.assertEqual(result, ['\x1b[30;47m', '\x1b[1;31m'])

    def test_code_code_reset(self):
        result = check_ansi_codes('\x1b[30;47mHello\x1b[1;31mWorld\x1b[0m')
        self.assertEqual(result, None)

    def test_reset_only(self):
        result = check_ansi_codes('\x1b[30;47mHello World\x1b[9999m\x1b[m')
        self.assertEqual(result, None)
        result = check_ansi_codes('Hello World\x1b[0m')
        self.assertEqual(result, None)
        result = check_ansi_codes('Hello World\x1b[m')
        self.assertEqual(result, None)

    def test_code_reset_code(self):
        result = check_ansi_codes('\x1b[30;47mHello\x1b[0m\x1b[1;31mWorld')
        self.assertEqual(result, ['\x1b[1;31m'])

    def test_code_reset_code_reset(self):
        result = check_ansi_codes(
            '\x1b[30;47mHello\x1b[0m\x1b[1;31mWorld\x1b[0m'
        )
        self.assertEqual(result, None)


if __name__ == '__main__':
    unittest.main()
