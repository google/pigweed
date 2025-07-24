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
"""Tests for pw_cli.collect_files."""

import os
import re
import subprocess
from pathlib import Path
import unittest
from unittest.mock import MagicMock, patch

from pyfakefs.fake_filesystem_unittest import TestCase
from pw_cli.collect_files import collect_files_in_current_repo
from pw_cli.git_repo import GitRepo
from pw_cli.tool_runner import ToolRunner


class FakeToolRunner(ToolRunner):
    """A fake ToolRunner for testing."""

    def __init__(self, return_values):
        super().__init__()
        self.return_values = return_values
        self.calls = []

    def _run_tool(self, tool, args, **kwargs):
        self.calls.append((tool, args, kwargs))
        command = tuple(args)
        if command in self.return_values:
            return subprocess.CompletedProcess(
                command, 0, stdout=self.return_values[command].encode()
            )
        return subprocess.CompletedProcess(
            command, 1, stderr=b'Command not found'
        )


def _resolve(path: str) -> str:
    """Needed to make Windows happy.

    Since resolved paths start with drive letters, any literal string
    paths in these tests need to be resolved so they are prefixed with `C:`.
    """
    # Avoid manipulation on other OSes since they don't strictly require it.
    if os.name != 'nt':
        return path
    return str(Path(path).resolve())


class TestCollectFilesInCurrentRepo(TestCase):
    """Tests for collect_files_in_current_repo."""

    def setUp(self) -> None:
        self.setUpPyfakefs()
        self.repo_root = Path('/repo')
        self.fs.create_dir(self.repo_root)
        os.chdir(self.repo_root)

        self.txt_file_at_root = self.repo_root / 'txt_file_at_root.txt'
        self.py_file = self.repo_root / 'dir' / 'py_file.py'
        self.cc_file = self.repo_root / 'dir' / 'subdir' / 'cc_file.cc'
        self.fs.create_file(self.txt_file_at_root)
        self.fs.create_file(self.py_file)
        self.fs.create_file(self.cc_file)

        self.git_files = [
            'txt_file_at_root.txt',
            'dir/py_file.py',
            'dir/subdir/cc_file.cc',
        ]
        self.tool_runner = FakeToolRunner(
            {
                ('-C', _resolve('/repo'), 'ls-files', '--'): '\n'.join(
                    self.git_files
                ),
                (
                    '-C',
                    _resolve('/repo'),
                    'diff',
                    '--name-only',
                    'HEAD',
                    '--',
                ): 'new_file.txt',
                ('rev-parse', '--abbrev-ref', 'HEAD'): 'main',
                (
                    'rev-parse',
                    '--symbolic-full-name',
                    'main@{u}',
                ): 'refs/remotes/origin/main',
                (
                    '-C',
                    _resolve('/repo'),
                    'rev-parse',
                    '--show-toplevel',
                ): str(self.repo_root),
                (
                    '-C',
                    _resolve('/repo/dir'),
                    'rev-parse',
                    '--show-toplevel',
                ): str(self.repo_root),
            }
        )

    @patch('pw_cli.collect_files.find_git_repo')
    def test_collect_all_files(self, mock_find_git_repo: MagicMock) -> None:
        """Test collecting all files."""
        mock_repo = GitRepo(self.repo_root, self.tool_runner)
        mock_find_git_repo.return_value = mock_repo

        files = collect_files_in_current_repo([], self.tool_runner, None)
        expected = [self.txt_file_at_root, self.py_file, self.cc_file]
        self.assertCountEqual(
            [f.resolve() for f in expected],
            [Path(f).resolve() for f in files],
        )

    @patch('pw_cli.collect_files.find_git_repo')
    def test_collect_with_pathspecs(
        self, mock_find_git_repo: MagicMock
    ) -> None:
        """Test collecting files with pathspecs."""
        mock_repo = GitRepo(self.repo_root, self.tool_runner)
        mock_find_git_repo.return_value = mock_repo

        # Update the tool runner to handle pathspecs
        self.tool_runner.return_values[
            (
                '-C',
                _resolve('/repo'),
                'ls-files',
                '--',
                '*.txt',
                'dir/*.py',
            )
        ] = 'txt_file_at_root.txt\ndir/py_file.py'

        files = collect_files_in_current_repo(
            ['*.txt', 'dir/*.py'], self.tool_runner, None
        )
        expected = [self.txt_file_at_root, self.py_file]
        self.assertCountEqual(
            [f.resolve() for f in expected],
            [Path(f).resolve() for f in files],
        )

    @patch('pw_cli.collect_files.find_git_repo')
    def test_exclude_files(self, mock_find_git_repo: MagicMock) -> None:
        """Test excluding files with regex."""
        mock_repo = GitRepo(self.repo_root, self.tool_runner)
        mock_find_git_repo.return_value = mock_repo

        exclude_patterns = [re.compile(r'.*\.txt'), re.compile(r'dir/.*\.py')]
        files = collect_files_in_current_repo(
            [], self.tool_runner, None, exclude_patterns=exclude_patterns
        )
        expected = [self.cc_file]
        self.assertCountEqual(
            [f.resolve() for f in expected], [Path(f).resolve() for f in files]
        )

    @patch('pw_cli.collect_files.find_git_repo')
    def test_exclude_files_relative_to_root(
        self, mock_find_git_repo: MagicMock
    ) -> None:
        """Test that exclude patterns match relative to the repo root."""
        mock_repo = GitRepo(self.repo_root, self.tool_runner)
        mock_find_git_repo.return_value = mock_repo
        os.chdir(self.repo_root / 'dir')

        exclude_patterns = [re.compile(r'^dir/py_file.py')]
        files = collect_files_in_current_repo(
            [], self.tool_runner, None, exclude_patterns=exclude_patterns
        )
        expected = [self.txt_file_at_root, self.cc_file]
        self.assertCountEqual(
            [f.resolve() for f in expected],
            [Path(f).resolve() for f in files],
        )

    def test_no_git_repo(self) -> None:
        """Test behavior when not in a git repo."""
        with self.fs.patcher:
            temp_dir = Path('/non_repo')
            self.fs.create_dir(temp_dir)
            os.chdir(temp_dir)
            file4 = temp_dir / 'file4.txt'
            file5 = temp_dir / 'dir1' / 'file5.log'
            self.fs.create_file(file4)
            self.fs.create_file(file5)

            files = collect_files_in_current_repo(
                [str(file4), str(file5)], self.tool_runner
            )
            expected = [file4, file5]
            self.assertCountEqual(
                [f.resolve() for f in expected],
                [Path(f).resolve() for f in files],
            )


if __name__ == '__main__':
    unittest.main()
