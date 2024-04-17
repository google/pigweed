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
"""Helpful commands for working with a Git repository."""

import logging
from pathlib import Path
import subprocess
from typing import Collection, Iterable, Pattern

from pw_cli.plural import plural
from pw_cli.tool_runner import ToolRunner

_LOG = logging.getLogger(__name__)

TRACKING_BRANCH_ALIAS = '@{upstream}'
_TRACKING_BRANCH_ALIASES = TRACKING_BRANCH_ALIAS, '@{u}'
_NON_TRACKING_FALLBACK = 'HEAD~10'


class GitError(Exception):
    """A Git-raised exception."""

    def __init__(self, message, returncode):
        super().__init__(message)
        self.returncode = returncode


class _GitTool:
    def __init__(self, tool_runner: ToolRunner, working_dir: Path):
        self._run_tool = tool_runner
        self._working_dir = working_dir

    def __call__(self, *args, **kwargs) -> str:
        proc = self._run_tool(
            tool='git',
            args=(
                '-C',
                str(self._working_dir),
                *args,
            ),
            **kwargs,
        )

        if proc.returncode != 0:
            if not proc.stderr:
                err = '(no output)'
            else:
                err = proc.stderr.decode().strip()
            raise GitError(err, proc.returncode)

        return '' if not proc.stdout else proc.stdout.decode().strip()


class GitRepo:
    """Represents a checked out Git repository that may be queried for info."""

    def __init__(self, root: Path, tool_runner: ToolRunner):
        self._root = root.resolve()
        self._git = _GitTool(tool_runner, self._root)

    def tracking_branch(
        self,
        fallback: str | None = None,
    ) -> str | None:
        """Returns the tracking branch of the current branch.

        Since most callers of this function can safely handle a return value of
        None, suppress exceptions and return None if there is no tracking
        branch.

        Returns:
          the remote tracking branch name or None if there is none
        """

        # This command should only error out if there's no upstream branch set.
        try:
            return self._git(
                'rev-parse',
                '--abbrev-ref',
                '--symbolic-full-name',
                TRACKING_BRANCH_ALIAS,
            )

        except GitError:
            return fallback

    def _ls_files(self, pathspecs: Collection[Path | str]) -> Iterable[Path]:
        """Returns results of git ls-files as absolute paths."""
        for file in self._git('ls-files', '--', *pathspecs).splitlines():
            full_path = self._root / file
            # Modified submodules will show up as directories and should be
            # ignored.
            if full_path.is_file():
                yield full_path

    def _diff_names(
        self, commit: str, pathspecs: Collection[Path | str]
    ) -> Iterable[Path]:
        """Returns paths of files changed since the specified commit.

        All returned paths are absolute file paths.
        """
        for file in self._git(
            'diff',
            '--name-only',
            '--diff-filter=d',
            commit,
            '--',
            *pathspecs,
        ).splitlines():
            full_path = self._root / file
            # Modified submodules will show up as directories and should be
            # ignored.
            if full_path.is_file():
                yield full_path

    def list_files(
        self,
        commit: str | None = None,
        pathspecs: Collection[Path | str] = (),
    ) -> list[Path]:
        """Lists files modified since the specified commit.

        If ``commit`` is not found in the current repo, all files in the
        repository are listed.

        Arugments:
            commit: The Git hash to start from when listing modified files
            pathspecs: Git pathspecs use when filtering results

        Returns:
            A sorted list of absolute paths.
        """

        if commit in _TRACKING_BRANCH_ALIASES:
            commit = self.tracking_branch(fallback=_NON_TRACKING_FALLBACK)

        if commit:
            try:
                return sorted(self._diff_names(commit, pathspecs))
            except GitError:
                _LOG.warning(
                    'Error comparing with base revision %s of %s, listing all '
                    'files instead of just changed files',
                    commit,
                    self._root,
                )

        return sorted(self._ls_files(pathspecs))

    def has_uncommitted_changes(self) -> bool:
        """Returns True if this Git repo has uncommitted changes in it.

        Note: This does not check for untracked files.

        Returns:
            True if the Git repo has uncommitted changes in it.
        """

        # Refresh the Git index so that the diff-index command will be accurate.
        # The `git update-index` command isn't reliable when run in parallel
        # with other processes that may touch files in the repo directory, so
        # retry a few times before giving up. The hallmark of this failure mode
        # is the lack of an error message on stderr, so if we see something
        # there we can assume it's some other issue and raise.
        retries = 6
        for i in range(retries):
            try:
                self._git(
                    'update-index',
                    '-q',
                    '--refresh',
                    ignore_dry_run=True,  # Relevant for pw_presubmit.
                )
            except subprocess.CalledProcessError as err:
                if err.stderr or i == retries - 1:
                    raise
                continue

        try:
            self._git(
                'diff-index',
                '--quiet',
                'HEAD',
                '--',
                ignore_dry_run=True,  # Relevant for pw_presubmit.
            )
        except GitError as err:
            # diff-index exits with 1 if there are uncommitted changes.
            if err.returncode == 1:
                return True

            # Unexpected error.
            raise

        return False

    def root(self) -> Path:
        """The root file path of this Git repository.

        Returns:
            The repository root as an absolute path.
        """
        return self._root

    def list_submodules(
        self, excluded_paths: Collection[Pattern | str] = ()
    ) -> list[Path]:
        """Query Git and return a list of submodules in the current project.

        Arguments:
            excluded_paths: Pattern or string that match submodules that should
                not be returned. All matches are done on posix-style paths
                relative to the project root.

        Returns:
            List of "Path"s which were found but not excluded. All paths are
            absolute.
        """
        discovery_report = self._git(
            'submodule',
            'foreach',
            '--quiet',
            '--recursive',
            'echo $toplevel/$sm_path',
        )
        module_dirs = [Path(line) for line in discovery_report.split()]

        for exclude in excluded_paths:
            if isinstance(exclude, Pattern):
                for module_dir in reversed(module_dirs):
                    if exclude.fullmatch(
                        module_dir.relative_to(self._root).as_posix()
                    ):
                        module_dirs.remove(module_dir)
            else:
                for module_dir in reversed(module_dirs):
                    print(f'not regex: {exclude}')
                    if exclude == module_dir.relative_to(self._root).as_posix():
                        module_dirs.remove(module_dir)

        return module_dirs

    def commit_message(self, commit: str = 'HEAD') -> str:
        """Returns the commit message of the specified commit.

        Defaults to ``HEAD`` if no commit specified.

        Returns:
            Commit message contents as a string.
        """
        return self._git('log', '--format=%B', '-n1', commit)

    def commit_author(self, commit: str = 'HEAD') -> str:
        """Returns the author of the specified commit.

        Defaults to ``HEAD`` if no commit specified.

        Returns:
            Commit author as a string.
        """
        return self._git('log', '--format=%ae', '-n1', commit)

    def commit_hash(
        self,
        commit: str = 'HEAD',
        short: bool = True,
    ) -> str:
        """Returns the hash associated with the specified commit.

        Defaults to ``HEAD`` if no commit specified.

        Returns:
            Commit hash as a string.
        """
        args = ['rev-parse']
        if short:
            args += ['--short']
        args += [commit]
        return self._git(*args)


def find_git_repo(path_in_repo: Path, tool_runner: ToolRunner) -> GitRepo:
    """Tries to find the root of the Git repo that owns ``path_in_repo``.

    Raises:
        GitError: The specified path does not live in a Git repository.

    Returns:
        A GitRepo representing the the enclosing repository that tracks the
        specified file or folder.
    """
    git_tool = _GitTool(
        tool_runner,
        path_in_repo if path_in_repo.is_dir() else path_in_repo.parent,
    )
    root = Path(
        git_tool(
            'rev-parse',
            '--show-toplevel',
        )
    )

    return GitRepo(root, tool_runner)


def is_in_git_repo(p: Path, tool_runner: ToolRunner) -> bool:
    """Returns true if the specified path is tracked by a Git repository.

    Returns:
        True if the specified file or folder is tracked by a Git repository.
    """
    try:
        find_git_repo(p, tool_runner)
    except GitError:
        return False

    return True


def _describe_constraints(
    repo: GitRepo,
    working_dir: Path,
    commit: str | None,
    pathspecs: Collection[Path | str],
    exclude: Collection[Pattern[str]],
) -> Iterable[str]:
    if not repo.root().samefile(working_dir):
        yield (
            'under the '
            f'{working_dir.resolve().relative_to(repo.root().resolve())}'
            ' subdirectory'
        )

    if commit in _TRACKING_BRANCH_ALIASES:
        commit = repo.tracking_branch()
        if commit is None:
            _LOG.warning(
                'Attempted to list files changed since the remote tracking '
                'branch, but the repo is not tracking a branch'
            )

    if commit:
        yield f'that have changed since {commit}'

    if pathspecs:
        paths_str = ', '.join(str(p) for p in pathspecs)
        yield f'that match {plural(pathspecs, "pathspec")} ({paths_str})'

    if exclude:
        yield (
            f'that do not match {plural(exclude, "pattern")} ('
            + ', '.join(p.pattern for p in exclude)
            + ')'
        )


def describe_git_pattern(
    working_dir: Path,
    commit: str | None,
    pathspecs: Collection[Path | str],
    exclude: Collection[Pattern],
    tool_runner: ToolRunner,
    project_root: Path | None = None,
) -> str:
    """Provides a description for a set of files in a Git repo.

    Example:

        files in the pigweed repo
        - that have changed since origin/main..HEAD
        - that do not match 7 patterns (...)

    The unit tests for this function are the source of truth for the expected
    output.

    Returns:
        A multi-line string with descriptive information about the provided
        Git pathspecs.
    """
    repo = find_git_repo(working_dir, tool_runner)
    constraints = list(
        _describe_constraints(repo, working_dir, commit, pathspecs, exclude)
    )

    name = repo.root().name
    if project_root and project_root != repo.root():
        name = str(repo.root().relative_to(project_root))

    if not constraints:
        return f'all files in the {name} repo'

    msg = f'files in the {name} repo'
    if len(constraints) == 1:
        return f'{msg} {constraints[0]}'

    return msg + ''.join(f'\n    - {line}' for line in constraints)
