#!/usr/bin/env python3
# Copyright 2020 The Pigweed Authors
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
"""Tests the python_packages module."""

import collections
import io
import unittest
from unittest import mock

from pw_env_setup import python_packages


def _subprocess_run_stdout(stdout=b'foo==1.0\nbar==2.0\npw-foo @ file:...\n'):
    def subprocess_run(*unused_args, **unused_kwargs):
        CompletedProcess = collections.namedtuple('CompletedProcess', 'stdout')
        return CompletedProcess(stdout=stdout)

    return subprocess_run


class TestPythonPackages(unittest.TestCase):
    """Tests the python_packages module."""
    @mock.patch('pw_env_setup.python_packages.subprocess.run',
                side_effect=_subprocess_run_stdout())
    def test_list(self, unused_mock):
        buf = io.StringIO()
        python_packages.ls(buf)
        self.assertIn('foo==1.0', buf.getvalue())
        self.assertIn('bar==2.0', buf.getvalue())
        self.assertNotIn('pw-foo', buf.getvalue())

    @mock.patch('pw_env_setup.python_packages.subprocess.run',
                side_effect=_subprocess_run_stdout())
    @mock.patch('pw_env_setup.python_packages._stderr')
    def test_diff_removed(self, stderr_mock, unused_mock):
        expected = io.StringIO('foo==1.0\nbar==2.0\nbaz==3.0\n')
        expected.name = 'test.name'
        self.assertFalse(python_packages.diff(expected))

        stderr_mock.assert_any_call('Removed packages')
        stderr_mock.assert_any_call('  baz==3.0')

    @mock.patch('pw_env_setup.python_packages.subprocess.run',
                side_effect=_subprocess_run_stdout())
    @mock.patch('pw_env_setup.python_packages._stderr')
    def test_diff_updated(self, stderr_mock, unused_mock):
        expected = io.StringIO('foo==1.0\nbar==1.9\n')
        expected.name = 'test.name'
        self.assertTrue(python_packages.diff(expected))

        stderr_mock.assert_any_call('Updated packages')
        stderr_mock.assert_any_call('  bar==2.0 (from 1.9)')
        stderr_mock.assert_any_call("Package versions don't match!")

    @mock.patch('pw_env_setup.python_packages.subprocess.run',
                side_effect=_subprocess_run_stdout())
    @mock.patch('pw_env_setup.python_packages._stderr')
    def test_diff_new(self, stderr_mock, unused_mock):
        expected = io.StringIO('foo==1.0\n')
        expected.name = 'test.name'
        self.assertTrue(python_packages.diff(expected))

        stderr_mock.assert_any_call('New packages')
        stderr_mock.assert_any_call('  bar==2.0')
        stderr_mock.assert_any_call("Package versions don't match!")


if __name__ == '__main__':
    unittest.main()
