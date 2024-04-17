#!/usr/bin/env python3
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
"""git repo module tests"""

from pathlib import Path
from subprocess import CompletedProcess
from typing import Dict
import re
import unittest
from unittest import mock

from pw_cli.tool_runner import ToolRunner
from pw_cli.git_repo import GitRepo


class FakeGitToolRunner(ToolRunner):
    def __init__(self, command_results: Dict[str, CompletedProcess]):
        self._results = command_results

    def _run_tool(self, tool: str, args, **kwargs) -> CompletedProcess:
        full_command = ' '.join((tool, *tuple(args)))
        for cmd, result in self._results.items():
            if cmd in full_command:
                return result

        return CompletedProcess(
            args=full_command,
            returncode=0xFF,
            stderr=f'I do not know how to `{full_command}`'.encode(),
            stdout=b'Failed to execute command',
        )


def git_ok(cmd: str, stdout: str) -> CompletedProcess:
    return CompletedProcess(
        args=cmd,
        returncode=0,
        stderr='',
        stdout=stdout.encode(),
    )


def git_err(cmd: str, stderr: str, returncode: int = 1) -> CompletedProcess:
    return CompletedProcess(
        args=cmd,
        returncode=returncode,
        stderr=stderr.encode(),
        stdout='',
    )


class TestGitRepo(unittest.TestCase):
    """Tests for git_repo.py"""

    GIT_ROOT = Path("/dev/null/test").resolve()
    SUBMODULES = [
        Path("/dev/null/test/third_party/pigweed").resolve(),
        Path("/dev/null/test/vendor/anycom/p1").resolve(),
        Path("/dev/null/test/vendor/anycom/p2").resolve(),
    ]
    GIT_SUBMODULES_OUT = "\n".join([str(x) for x in SUBMODULES])

    EXPECTED_SUBMODULE_LIST_CMD = ' '.join(
        (
            'submodule',
            'foreach',
            '--quiet',
            '--recursive',
            'echo $toplevel/$sm_path',
        )
    )

    def make_fake_git_repo(self, cmds):
        return GitRepo(self.GIT_ROOT, FakeGitToolRunner(cmds))

    def test_mock_root(self):
        """Ensure our mock works since so many of our tests depend upon it."""
        cmds = {}
        repo = self.make_fake_git_repo(cmds)
        self.assertEqual(repo.root(), self.GIT_ROOT)

    def test_list_submodules_1(self):
        """Ensures the root git repo appears in the submodule list."""
        cmds = {
            self.EXPECTED_SUBMODULE_LIST_CMD: git_ok(
                self.EXPECTED_SUBMODULE_LIST_CMD, self.GIT_SUBMODULES_OUT
            )
        }
        repo = self.make_fake_git_repo(cmds)
        paths = repo.list_submodules()
        self.assertNotIn(self.GIT_ROOT, paths)

    def test_list_submodules_2(self):
        cmds = {
            self.EXPECTED_SUBMODULE_LIST_CMD: git_ok(
                self.EXPECTED_SUBMODULE_LIST_CMD, self.GIT_SUBMODULES_OUT
            )
        }
        repo = self.make_fake_git_repo(cmds)
        paths = repo.list_submodules()
        self.assertIn(self.SUBMODULES[2], paths)

    def test_list_submodules_with_exclude_str(self):
        cmds = {
            self.EXPECTED_SUBMODULE_LIST_CMD: git_ok(
                self.EXPECTED_SUBMODULE_LIST_CMD, self.GIT_SUBMODULES_OUT
            )
        }
        repo = self.make_fake_git_repo(cmds)
        paths = repo.list_submodules(
            excluded_paths=(self.GIT_ROOT.as_posix(),),
        )
        self.assertNotIn(self.GIT_ROOT, paths)

    def test_list_submodules_with_exclude_regex(self):
        cmds = {
            self.EXPECTED_SUBMODULE_LIST_CMD: git_ok(
                self.EXPECTED_SUBMODULE_LIST_CMD, self.GIT_SUBMODULES_OUT
            )
        }
        repo = self.make_fake_git_repo(cmds)
        paths = repo.list_submodules(
            excluded_paths=(re.compile("third_party/.*"),),
        )
        self.assertNotIn(self.SUBMODULES[0], paths)

    def test_list_submodules_with_exclude_str_miss(self):
        cmds = {
            self.EXPECTED_SUBMODULE_LIST_CMD: git_ok(
                self.EXPECTED_SUBMODULE_LIST_CMD, self.GIT_SUBMODULES_OUT
            )
        }
        repo = self.make_fake_git_repo(cmds)
        paths = repo.list_submodules(
            excluded_paths=(re.compile("pigweed"),),
        )
        self.assertIn(self.SUBMODULES[-1], paths)

    def test_list_submodules_with_exclude_regex_miss_1(self):
        cmds = {
            self.EXPECTED_SUBMODULE_LIST_CMD: git_ok(
                self.EXPECTED_SUBMODULE_LIST_CMD, self.GIT_SUBMODULES_OUT
            )
        }
        repo = self.make_fake_git_repo(cmds)
        paths = repo.list_submodules(
            excluded_paths=(re.compile("foo/.*"),),
        )
        self.assertNotIn(self.GIT_ROOT, paths)
        for module in self.SUBMODULES:
            self.assertIn(module, paths)

    def test_list_submodules_with_exclude_regex_miss_2(self):
        cmds = {
            self.EXPECTED_SUBMODULE_LIST_CMD: git_ok(
                self.EXPECTED_SUBMODULE_LIST_CMD, self.GIT_SUBMODULES_OUT
            )
        }
        repo = self.make_fake_git_repo(cmds)
        paths = repo.list_submodules(
            excluded_paths=(re.compile("pigweed"),),
        )
        self.assertNotIn(self.GIT_ROOT, paths)
        for module in self.SUBMODULES:
            self.assertIn(module, paths)

    def test_list_files_unknown_hash(self):
        bad_cmd = "diff --name-only --diff-filter=d 'something' --"
        good_cmd = 'ls-files --'
        fake_path = 'path/to/foo.h'
        cmds = {
            bad_cmd: git_err(bad_cmd, "fatal: bad revision 'something'"),
            good_cmd: git_ok(good_cmd, fake_path + '\n'),
        }

        expected_file_path = self.GIT_ROOT / Path(fake_path)
        repo = self.make_fake_git_repo(cmds)

        # This function needs to be mocked because it does a `is_file()` check
        # on returned paths. Since we're not using real files, nothing will
        # be yielded.
        repo._ls_files = mock.MagicMock(  # pylint: disable=protected-access
            return_value=[expected_file_path]
        )
        paths = repo.list_files(commit='something')
        self.assertIn(expected_file_path, paths)

    def test_fake_uncommitted_changes(self):
        index_update = 'update-index -q --refresh'
        diff_index = 'diff-index --quiet HEAD --'
        cmds = {
            index_update: git_ok(index_update, ''),
            diff_index: git_err(diff_index, '', returncode=1),
        }
        repo = self.make_fake_git_repo(cmds)
        self.assertTrue(repo.has_uncommitted_changes())


if __name__ == '__main__':
    unittest.main()
