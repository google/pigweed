# Copyright 2023 The Pigweed Authors
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
"""Tests for pw_cli.process."""

import unittest
import sys
import textwrap

from pw_cli import process

import psutil  # type: ignore


FAST_TIMEOUT_SECONDS = 0.1
KILL_SIGNALS = set({-9, 137})
PYTHON = sys.executable


class RunTest(unittest.TestCase):
    """Tests for process.run."""

    def test_returns_output(self) -> None:
        echo_str = 'foobar'
        print_str = f'print("{echo_str}")'
        result = process.run(PYTHON, '-c', print_str)
        self.assertEqual(result.output, b'foobar\n')

    def test_timeout_kills_process(self) -> None:
        sleep_100_seconds = 'import time; time.sleep(100)'
        result = process.run(
            PYTHON, '-c', sleep_100_seconds, timeout=FAST_TIMEOUT_SECONDS
        )
        self.assertIn(result.returncode, KILL_SIGNALS)

    def test_timeout_kills_subprocess(self) -> None:
        # Spawn a subprocess which waits for 100 seconds, print its pid,
        # then wait for 100 seconds.
        sleep_in_subprocess = textwrap.dedent(
            f"""
        import subprocess
        import time

        child = subprocess.Popen(
          ['{PYTHON}', '-c', 'import time; print("booh"); time.sleep(100)']
        )
        print(child.pid, flush=True)
        time.sleep(100)
        """
        )
        result = process.run(
            PYTHON, '-c', sleep_in_subprocess, timeout=FAST_TIMEOUT_SECONDS
        )
        self.assertIn(result.returncode, KILL_SIGNALS)
        # THe first line of the output is the PID of the child sleep process.
        child_pid_str, sep, remainder = result.output.partition(b'\n')
        del sep, remainder
        child_pid = int(child_pid_str)
        self.assertFalse(psutil.pid_exists(child_pid))


if __name__ == '__main__':
    unittest.main()
