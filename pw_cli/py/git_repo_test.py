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

import os
from pathlib import Path
from subprocess import CompletedProcess
from typing import Dict
import re
import shlex
import unittest
from unittest import mock

from pw_cli.tool_runner import ToolRunner
from pw_cli.git_repo import GitRepo, GitRepoFinder
from pyfakefs import fake_filesystem_unittest


class FakeGitToolRunner(ToolRunner):
    def __init__(self, command_results: Dict[str, CompletedProcess]):
        self._results = command_results

    def _run_tool(self, tool: str, args, **kwargs) -> CompletedProcess:
        full_command = shlex.join((tool, *tuple(args)))
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

    EXPECTED_SUBMODULE_LIST_CMD = shlex.join(
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


def _resolve(path: str) -> str:
    """Needed to make Windows happy.

    Since resolved paths start with drive letters, any literal string
    paths in these tests need to be resolved so they are prefixed with `C:`.
    """
    # Avoid manipulation on other OSes since they don't strictly require it.
    if os.name != 'nt':
        return path
    return str(Path(path).resolve())


class TestGitRepoFinder(fake_filesystem_unittest.TestCase):
    """Tests for GitRepoFinder."""

    FAKE_ROOT = _resolve('/dev/null/fake/root')
    FAKE_NESTED_REPO = _resolve('/dev/null/fake/root/third_party/bogus')

    def setUp(self):
        self.setUpPyfakefs()
        self.fs.create_dir(self.FAKE_ROOT)
        os.chdir(self.FAKE_ROOT)

    def test_cwd_is_root(self):
        """Tests when cwd is the root of a repo."""
        expected_repo_query = shlex.join(
            (
                '-C',
                '.',
                'rev-parse',
                '--show-toplevel',
            )
        )
        runner = FakeGitToolRunner(
            {expected_repo_query: git_ok(expected_repo_query, self.FAKE_ROOT)}
        )
        finder = GitRepoFinder(runner)
        path_to_search = '.'
        maybe_repo = finder.find_git_repo(path_to_search)
        self.assertNotEqual(
            maybe_repo, None, f'Could not resolve {path_to_search}'
        )
        self.assertEqual(maybe_repo.root(), Path(self.FAKE_ROOT))

    def test_cwd_is_not_repo(self):
        """Tests when cwd is not tracked by a repo."""
        expected_repo_query = shlex.join(
            (
                '-C',
                '.',
                'rev-parse',
                '--show-toplevel',
            )
        )
        runner = FakeGitToolRunner(
            {expected_repo_query: git_err(expected_repo_query, self.FAKE_ROOT)}
        )
        finder = GitRepoFinder(runner)
        self.assertEqual(finder.find_git_repo('.'), None)

    def test_file(self):
        """Tests a file at the root of a repo."""
        expected_repo_query = shlex.join(
            (
                '-C',
                '.',
                'rev-parse',
                '--show-toplevel',
            )
        )
        runner = FakeGitToolRunner(
            {expected_repo_query: git_ok(expected_repo_query, self.FAKE_ROOT)}
        )
        finder = GitRepoFinder(runner)
        path_to_search = 'foo.txt'
        self.fs.create_file(path_to_search)
        maybe_repo = finder.find_git_repo(path_to_search)
        self.assertNotEqual(
            maybe_repo, None, f'Could not resolve {path_to_search}'
        )
        self.assertEqual(maybe_repo.root(), Path(self.FAKE_ROOT))

    def test_parents_memoized(self):
        """Tests multiple queries that are optimized via memoization."""
        expected_repo_query = shlex.join(
            (
                '-C',
                str(Path('subdir/nested')),
                'rev-parse',
                '--show-toplevel',
            )
        )
        runner = FakeGitToolRunner(
            {expected_repo_query: git_ok(expected_repo_query, self.FAKE_ROOT)}
        )
        finder = GitRepoFinder(runner)

        # Because of the ordering, only ONE call to git should be necessary.
        paths = [
            'subdir/nested/foo.txt',
            'subdir/bar.txt',
            'subdir/nested/baz.txt',
            'bleh.txt',
        ]
        for file_to_find in paths:
            self.fs.create_file(file_to_find)
            maybe_repo = finder.find_git_repo(file_to_find)
            self.assertNotEqual(
                maybe_repo, None, f'Could not resolve {file_to_find}'
            )
            self.assertEqual(maybe_repo.root(), Path(self.FAKE_ROOT))

    def test_absolute_path(self):
        """Test that absolute paths hit memoized paths."""
        expected_repo_query = shlex.join(
            (
                '-C',
                str(Path('subdir/nested')),
                'rev-parse',
                '--show-toplevel',
            )
        )
        runner = FakeGitToolRunner(
            {expected_repo_query: git_ok(expected_repo_query, self.FAKE_ROOT)}
        )
        finder = GitRepoFinder(runner)

        # Because of the ordering, only ONE call to git should be necessary.
        paths = [
            'subdir/nested/foo.txt',
            _resolve(f'{self.FAKE_ROOT}/subdir/bar.txt'),
        ]
        for file_to_find in paths:
            self.fs.create_file(file_to_find)
            maybe_repo = finder.find_git_repo(file_to_find)
            self.assertNotEqual(
                maybe_repo, None, f'Could not resolve {file_to_find}'
            )
            self.assertEqual(maybe_repo.root(), Path(self.FAKE_ROOT))

    def test_subdir(self):
        """Test that querying a dir properly memoizes things."""
        expected_repo_query = shlex.join(
            (
                '-C',
                'subdir',
                'rev-parse',
                '--show-toplevel',
            )
        )
        runner = FakeGitToolRunner(
            {expected_repo_query: git_ok(expected_repo_query, self.FAKE_ROOT)}
        )
        finder = GitRepoFinder(runner)

        dir_to_check = 'subdir'
        self.fs.create_dir(dir_to_check)
        maybe_repo = finder.find_git_repo(dir_to_check)
        self.assertNotEqual(
            maybe_repo, None, f'Could not resolve {dir_to_check}'
        )
        self.assertEqual(maybe_repo.root(), Path(self.FAKE_ROOT))

    def test_nested_repo(self):
        """Test a nested repo works as expected."""
        expected_inner_repo_query = shlex.join(
            (
                '-C',
                str(Path('third_party/bogus/test')),
                'rev-parse',
                '--show-toplevel',
            )
        )
        expected_outer_repo_query = shlex.join(
            (
                '-C',
                'test',
                'rev-parse',
                '--show-toplevel',
            )
        )
        runner = FakeGitToolRunner(
            {
                expected_inner_repo_query: git_ok(
                    expected_inner_repo_query, self.FAKE_NESTED_REPO
                ),
                expected_outer_repo_query: git_ok(
                    expected_outer_repo_query, self.FAKE_ROOT
                ),
            }
        )
        finder = GitRepoFinder(runner)

        inner_repo_file = "third_party/bogus/test/baz.txt"
        self.fs.create_file(inner_repo_file)
        maybe_repo = finder.find_git_repo(inner_repo_file)
        self.assertNotEqual(
            maybe_repo, None, f'Could not resolve {inner_repo_file}'
        )
        self.assertEqual(maybe_repo.root(), Path(self.FAKE_NESTED_REPO))

        outer_repo_file = "test/baz.txt"
        self.fs.create_file(outer_repo_file)
        maybe_repo = finder.find_git_repo(outer_repo_file)
        self.assertNotEqual(
            maybe_repo, None, f'Could not resolve {outer_repo_file}'
        )
        self.assertEqual(maybe_repo.root(), Path(self.FAKE_ROOT))

    def test_absolute_repo_not_under_cwd(self):
        """Test an absolute path that isn't a subdir of cwd works."""
        fake_parallel_repo = _resolve('/dev/null/fake/parallel')
        expected_repo_query = shlex.join(
            (
                '-C',
                _resolve('/dev/null/fake/parallel/yep'),
                'rev-parse',
                '--show-toplevel',
            )
        )
        runner = FakeGitToolRunner(
            {
                expected_repo_query: git_ok(
                    expected_repo_query, fake_parallel_repo
                )
            }
        )
        finder = GitRepoFinder(runner)
        path_to_search = _resolve('/dev/null/fake/parallel/yep/foo.txt')
        self.fs.create_file(path_to_search)
        maybe_repo = finder.find_git_repo(path_to_search)
        self.assertNotEqual(
            maybe_repo, None, f'Could not resolve {path_to_search}'
        )
        self.assertEqual(maybe_repo.root(), Path(fake_parallel_repo))

    def test_absolute_not_under_cwd(self):
        """Test files not tracked by a repo."""
        expected_repo_query = shlex.join(
            (
                '-C',
                _resolve('/dev/null/fake/parallel/yep'),
                'rev-parse',
                '--show-toplevel',
            )
        )
        runner = FakeGitToolRunner(
            {expected_repo_query: git_err(expected_repo_query, '')}
        )
        finder = GitRepoFinder(runner)
        # Because of the ordering, only ONE call to git should be necessary.
        paths = [
            _resolve('/dev/null/fake/parallel/yep/foo.txt'),
            _resolve('/dev/null/fake/bar.txt'),
            _resolve('/dev/null/fake/parallel/yep'),
        ]
        for file_to_find in paths:
            if file_to_find.endswith('.txt'):
                self.fs.create_file(file_to_find)
            self.assertEqual(finder.find_git_repo(file_to_find), None)

    def test_make_pathspec_relative(self):
        """Tests that pathspec relativization works."""
        expected_queries = (
            (
                shlex.join(
                    (
                        '-C',
                        str(Path('george/one')),
                        'rev-parse',
                        '--show-toplevel',
                    )
                ),
                self.FAKE_ROOT,
            ),
            (
                shlex.join(
                    (
                        '-C',
                        str(Path('third_party/bogus')),
                        'rev-parse',
                        '--show-toplevel',
                    )
                ),
                self.FAKE_NESTED_REPO,
            ),
            (
                shlex.join(
                    (
                        '-C',
                        str(Path('third_party/bogus/frob')),
                        'rev-parse',
                        '--show-toplevel',
                    )
                ),
                self.FAKE_NESTED_REPO,
            ),
        )
        runner = FakeGitToolRunner(
            {
                expected_args: git_ok(expected_args, repo)
                for expected_args, repo in expected_queries
            }
        )
        finder = GitRepoFinder(runner)

        files = [
            'george/one/two.txt',
            'third_party/bogus/sad.png',
        ]
        for file_to_find in files:
            self.fs.create_file(file_to_find)
        self.fs.create_dir('third_party/bogus/frob')

        pathspecs = {
            'george/one/two.txt': str(Path('george/one/two.txt')),
            'a/': 'a',
            'third_party/bogus/sad.png': 'sad.png',
            'third_party/bogus/': '.',
            'third_party/bogus/frob/j*': str(Path('frob/j*')),
        }
        for pathspec, expected in pathspecs.items():
            maybe_repo, relativized = finder.make_pathspec_relative(pathspec)
            self.assertNotEqual(
                maybe_repo, None, f'Could not resolve {pathspec}'
            )
            self.assertEqual(relativized, expected)

    def test_make_pathspec_relative_untracked(self):
        """Tests that untracked files work with relativization."""
        expected_repo_query = shlex.join(
            (
                '-C',
                str(Path('subdir/nested')),
                'rev-parse',
                '--show-toplevel',
            )
        )
        runner = FakeGitToolRunner(
            {expected_repo_query: git_err(expected_repo_query, '')}
        )
        finder = GitRepoFinder(runner)

        self.fs.create_file('george/one/two.txt')

        pathspecs = {
            'george/one/two.txt': 'george/one/two.txt',
        }
        for pathspec, expected in pathspecs.items():
            maybe_repo, relativized = finder.make_pathspec_relative(pathspec)
            self.assertEqual(
                maybe_repo, None, f'Unexpectedly resolved {pathspec}'
            )
            self.assertEqual(relativized, expected)

    def test_make_pathspec_relative_absolute(self):
        """Tests that absolute paths work with relativization."""
        expected_repo_query = shlex.join(
            (
                '-C',
                _resolve('/dev/null/fake/root/third_party/bogus/one'),
                'rev-parse',
                '--show-toplevel',
            )
        )
        runner = FakeGitToolRunner(
            {
                expected_repo_query: git_ok(
                    expected_repo_query, self.FAKE_NESTED_REPO
                )
            }
        )
        finder = GitRepoFinder(runner)

        self.fs.create_file('third_party/bogus/one/two.txt')

        pathspecs = {
            _resolve('/dev/null/fake/root/third_party/bogus/one/two.txt'): str(
                Path('one/two.txt')
            ),
        }
        for pathspec, expected in pathspecs.items():
            maybe_repo, relativized = finder.make_pathspec_relative(pathspec)
            self.assertNotEqual(
                maybe_repo, None, f'Could not resolve {pathspec}'
            )
            self.assertEqual(relativized, expected)


if __name__ == '__main__':
    unittest.main()
