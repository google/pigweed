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
"""Test for pw_ide.bazel.merger."""

import io
import json
import os
import unittest
from pathlib import Path
from unittest import mock
from contextlib import redirect_stderr

# pylint: disable=unused-import

# Mocked imports.
import subprocess
import sys

# pylint: enable=unused-import

from pyfakefs import fake_filesystem_unittest

from pw_ide.bazel import merger

_FRAGMENT_SUFFIX = '.pw_aspect.compile_commands.json'


def _create_fragment(
    fs, output_path: Path, base_name: str, platform: str, content: list
):
    fragment_path = output_path / f'{base_name}.{platform}{_FRAGMENT_SUFFIX}'
    fs.create_file(fragment_path, contents=json.dumps(content))


class MergerTest(fake_filesystem_unittest.TestCase):
    """Tests for the compile commands fragment merger."""

    def setUp(self):
        self.setUpPyfakefs()
        self.workspace_root = Path('/workspace')
        self.output_base = Path(
            '/home/somebody/.cache/bazel/_bazel_somebody/123abc'
        )
        execution_root = self.output_base / 'execroot' / '_main'
        self.output_path = execution_root / 'bazel-out'

        self.fs.create_dir(self.workspace_root)
        self.fs.create_dir(self.output_path)

        self.mock_environ = self.enterContext(
            mock.patch.dict(
                os.environ,
                {'BUILD_WORKSPACE_DIRECTORY': str(self.workspace_root)},
            )
        )

        self.mock_subprocess = self.enterContext(
            mock.patch('subprocess.check_output')
        )

        def check_output_side_effect(
            args,
            # pylint: disable=unused-argument
            **kwargs,
            # pylint: enable=unused-argument
        ):
            if args == ['bazel', 'info', 'output_path']:
                return str(self.output_path)
            if args == ['bazel', 'info', 'output_base']:
                return str(self.output_base)
            raise AssertionError('Unhandled Bazel request')

        self.mock_subprocess.side_effect = check_output_side_effect

    def test_no_fragments_found(self):
        with io.StringIO() as buf, redirect_stderr(buf):
            self.assertEqual(merger.main(), 1)
            self.assertIn(
                'Could not find any generated fragments', buf.getvalue()
            )

    def test_basic_merge(self):
        """Test that a single compile command produces a database."""
        _create_fragment(
            self.fs,
            self.output_path,
            'target1',
            'linux',
            [
                {
                    'file': 'a.cc',
                    'directory': '__WORKSPACE_ROOT__',
                    'arguments': ['c', 'd'],
                }
            ],
        )
        self.assertEqual(merger.main(), 0)
        merged_path = (
            self.workspace_root
            / '.compile_commands'
            / 'linux'
            / 'compile_commands.json'
        )
        self.assertTrue(merged_path.exists())
        with open(merged_path, 'r') as f:
            data = json.load(f)
        self.assertEqual(len(data), 1)
        self.assertEqual(data[0]['file'], 'a.cc')
        self.assertEqual(data[0]['directory'], str(self.workspace_root))

    def test_merge_multiple_platforms(self):
        """Test that multiple platforms are correctly separated."""
        _create_fragment(
            self.fs,
            self.output_path,
            't1',
            'linux',
            [
                {
                    'file': 'a.cc',
                    'directory': '__WORKSPACE_ROOT__',
                    'arguments': [],
                }
            ],
        )
        _create_fragment(
            self.fs,
            self.output_path,
            't2',
            'mac',
            [
                {
                    'file': 'b.cc',
                    'directory': '__WORKSPACE_ROOT__',
                    'arguments': [],
                }
            ],
        )
        self.assertEqual(merger.main(), 0)
        linux_path = (
            self.workspace_root
            / '.compile_commands'
            / 'linux'
            / 'compile_commands.json'
        )
        mac_path = (
            self.workspace_root
            / '.compile_commands'
            / 'mac'
            / 'compile_commands.json'
        )
        self.assertTrue(linux_path.exists())
        self.assertTrue(mac_path.exists())

    def test_merge_with_json_error(self):
        """Test corrupt compile command fragments."""
        fragment_path = self.output_path / f'bad.linux{_FRAGMENT_SUFFIX}'
        self.fs.create_file(fragment_path, contents='not json')
        _create_fragment(
            self.fs,
            self.output_path,
            'good',
            'linux',
            [
                {
                    'file': 'a.cc',
                    'directory': '__WORKSPACE_ROOT__',
                    'arguments': [],
                }
            ],
        )

        with io.StringIO() as buf, redirect_stderr(buf):
            self.assertEqual(merger.main(), 0)
            self.assertIn(f'Could not parse {fragment_path}', buf.getvalue())

        merged_path = (
            self.workspace_root
            / '.compile_commands'
            / 'linux'
            / 'compile_commands.json'
        )
        with open(merged_path, 'r') as f:
            data = json.load(f)
        self.assertEqual(len(data), 1)

    def test_filter_unsupported_march(self):
        """Ensures an unsupported -march value is removed."""
        _create_fragment(
            self.fs,
            self.output_path,
            't1',
            'linux',
            [
                {
                    'file': 'a.cc',
                    'directory': '__WORKSPACE_ROOT__',
                    'arguments': ['-march=unsupported', '-march=x86-64'],
                }
            ],
        )
        self.assertEqual(merger.main(), 0)
        merged_path = (
            self.workspace_root
            / '.compile_commands'
            / 'linux'
            / 'compile_commands.json'
        )
        with open(merged_path, 'r') as f:
            data = json.load(f)
        self.assertEqual(data[0]['arguments'], ['-march=x86-64'])

    def test_resolve_bazel_out_paths(self):
        """Test that generated files are remapped to their absolute path."""
        _create_fragment(
            self.fs,
            self.output_path,
            't1',
            'linux',
            [
                {
                    'file': 'bazel-out/k8-fastbuild/bin/a.cc',
                    'directory': '__WORKSPACE_ROOT__',
                    'arguments': ['-Ibazel-out/k8-fastbuild/genfiles'],
                }
            ],
        )
        self.assertEqual(merger.main(), 0)
        merged_path = (
            self.workspace_root
            / '.compile_commands'
            / 'linux'
            / 'compile_commands.json'
        )
        with open(merged_path, 'r') as f:
            data = json.load(f)
        expected_file = str(self.output_path / 'k8-fastbuild/bin/a.cc')
        expected_arg = '-I' + str(self.output_path / 'k8-fastbuild/genfiles')
        self.assertEqual(data[0]['file'], expected_file)
        self.assertEqual(data[0]['arguments'], [expected_arg])

    def test_external_repo_paths(self):
        """Test that files in external repos are remapped to their real path."""
        _create_fragment(
            self.fs,
            self.output_path,
            't1',
            'linux',
            [
                {
                    'file': 'external/my_repo/a.cc',
                    'directory': '__WORKSPACE_ROOT__',
                    'arguments': [
                        '-Iexternal/my_repo/include',
                        '-iquote',
                        'external/+_repo_rules8+my_external_thing',
                    ],
                }
            ],
        )
        self.assertEqual(merger.main(), 0)
        merged_path = (
            self.workspace_root
            / '.compile_commands'
            / 'linux'
            / 'compile_commands.json'
        )
        with open(merged_path, 'r') as f:
            data = json.load(f)
        expected_file = str(self.output_base / 'external/my_repo/a.cc')
        expected_args = [
            '-I' + str(self.output_base / 'external/my_repo/include'),
            '-iquote',
            str(self.output_base / 'external/+_repo_rules8+my_external_thing'),
        ]
        self.assertEqual(data[0]['file'], expected_file)
        self.assertEqual(data[0]['arguments'], expected_args)

    def test_empty_fragment_file(self):
        """Test that an empty fragment file doesn't cause issues."""
        _create_fragment(self.fs, self.output_path, 'empty', 'linux', [])
        _create_fragment(
            self.fs,
            self.output_path,
            'good',
            'linux',
            [
                {
                    'file': 'a.cc',
                    'directory': '__WORKSPACE_ROOT__',
                    'arguments': [],
                }
            ],
        )
        self.assertEqual(merger.main(), 0)
        merged_path = (
            self.workspace_root
            / '.compile_commands'
            / 'linux'
            / 'compile_commands.json'
        )
        with open(merged_path, 'r') as f:
            data = json.load(f)
        self.assertEqual(len(data), 1)

    def test_workspace_root_not_set(self):
        """Ensure BUILD_WORKSPACE_DIRECTORY checking traps."""
        del os.environ['BUILD_WORKSPACE_DIRECTORY']
        with io.StringIO() as buf, redirect_stderr(buf):
            self.assertEqual(merger.main(), 1)
            self.assertIn("must be run with 'bazel run'", buf.getvalue())

    def test_output_path_does_not_exist(self):
        """Ensure an error occurs if the Bazel output path does not exist."""
        self.fs.remove_object(str(self.output_path))
        with io.StringIO() as buf, redirect_stderr(buf):
            self.assertEqual(merger.main(), 1)
            self.assertIn('not found', buf.getvalue())


if __name__ == '__main__':
    unittest.main()
