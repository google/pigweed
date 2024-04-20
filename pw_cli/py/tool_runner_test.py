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

import subprocess
import sys
import unittest
from typing import Any, Iterable

from pw_cli import tool_runner
from pw_cli.tool_runner import ToolRunner


class TestToolRunner(unittest.TestCase):
    """Tests for tool_runner.TestToolRunner."""

    def test_basic_subprocess_runner(self):
        runner = tool_runner.BasicSubprocessRunner()
        if sys.platform == 'win32':
            result = runner('python.exe', ('-c', 'print("hello world")'))
        else:
            result = runner('echo', ('hello', 'world'))
        self.assertEqual(result.returncode, 0)
        self.assertIn('hello world', result.stdout.decode())


class FakeTool(ToolRunner):
    def __init__(self) -> None:
        self.received_args: list[str] = []
        self.received_kwargs: dict[str, Any] = {}

    def _run_tool(
        self, tool: str, args, **kwargs
    ) -> subprocess.CompletedProcess:
        self.received_args = list(args)
        self.received_kwargs = kwargs

        full_command = ' '.join((tool, *tuple(args)))

        return subprocess.CompletedProcess(
            args=full_command,
            returncode=0xFF,
            stderr=f'I do not know how to `{full_command}`'.encode(),
            stdout=b'Failed to execute command',
        )


class FakeToolWithCustomArgs(FakeTool):
    @staticmethod
    def _custom_args() -> Iterable[str]:
        return ['pw_custom_arg', 'pw_2_custom_2_arg']


class ToolRunnerCallTest(unittest.TestCase):
    """Tests argument forwarding to ToolRunner implementations."""

    def test_fake_tool_without_custom_args(self):
        tool = FakeTool()

        tool(
            'rm',
            ('-rf', '/'),
            capture_output=True,
            pw_custom_arg='should not be forwarded',
        )

        self.assertEqual(tool.received_args, ['-rf', '/'])
        self.assertEqual(
            tool.received_kwargs,
            {
                'capture_output': True,
                'stdout': subprocess.PIPE,
                'stderr': subprocess.PIPE,
            },
        )

    def test_fake_tool_with_custom_args(self):
        tool = FakeToolWithCustomArgs()

        tool(
            'rm',
            ('-rf', '/'),
            capture_output=True,
            pw_custom_arg='should be forwarded',
            pw_2_custom_2_arg='this one too',
            pw_foo='but not this',
        )

        self.assertEqual(tool.received_args, ['-rf', '/'])
        self.assertEqual(
            tool.received_kwargs,
            {
                'capture_output': True,
                'stdout': subprocess.PIPE,
                'stderr': subprocess.PIPE,
                'pw_custom_arg': 'should be forwarded',
                'pw_2_custom_2_arg': 'this one too',
            },
        )


if __name__ == '__main__':
    unittest.main()
