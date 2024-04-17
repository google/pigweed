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
"""Tests for pw_cli.tool_runner."""

import unittest

from pw_cli import tool_runner


class TestToolRunner(unittest.TestCase):
    """Tests for tool_runner.TestToolRunner."""

    def test_basic_subprocess_runner(self):
        runner = tool_runner.BasicSubprocessRunner()
        result = runner('echo', ('hello', 'world'))
        self.assertEqual(result.returncode, 0)
        self.assertIn('hello world', result.stdout.decode())
