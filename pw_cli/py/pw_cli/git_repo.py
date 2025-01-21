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

from datetime import datetime
import itertools
import logging
from pathlib import Path
import re
import shlex
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

    def __init__(
        self, args: Iterable[str], message: str, returncode: int
    ) -> None:
        super().__init__(f'`git {shlex.join(args)}` failed: {message}')
        self.returncode = returncode


class _GitTool:
    def __init__(self, tool_runner: ToolRunner, working_dir: Path) -> None:
        self._run_tool = tool_runner
        self._working_dir = working_dir

    def __call__(self, *args, **kwargs) -> str:
        cmd = ('-C', str(self._working_dir), *args)
        proc = self._run_tool(tool='git', args=cmd, **kwargs)

        if proc.returncode != 0:
            if not proc.stderr:
                err = '(no output)'
            else:
                err = proc.stderr.decode().strip()
            raise GitError((str(s) for s in cmd), err, proc.returncode)

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

    def current_branch(self) -> str | None:
        """Returns the current branch, or None if it cannot be determined."""
        try:
            return self._git('rev-parse', '--abbrev-ref', 'HEAD')
        except GitError:
            return None

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
                    pw_presubmit_ignore_dry_run=True,
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
                pw_presubmit_ignore_dry_run=True,
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

    def commit_date(self, commit: str = 'HEAD') -> datetime:
        """Returns the datetime of the specified commit.

        Defaults to ``HEAD`` if no commit specified.

        Returns:
            Commit datetime as a datetime object.
        """
        return datetime.fromisoformat(
            self._git('log', '--format=%aI', '-n1', commit)
        )

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

    def commit_change_id(self, commit: str = 'HEAD') -> str | None:
        """Returns the Gerrit Change-Id of the specified commit.

        Defaults to ``HEAD`` if no commit specified.

        Returns:
            Change-Id as a string, or ``None`` if it does not exist.
        """
        message = self.commit_message(commit)
        regex = re.compile(
            'Change-Id: (I[a-fA-F0-9]+)',
            re.MULTILINE,
        )
        match = regex.search(message)
        return match.group(1) if match else None

    def commit_parents(self, commit: str = 'HEAD') -> list[str]:
        args = ['log', '--pretty=%P', '-n', '1', commit]
        return self._git(*args).split()

    def diff(self, *args) -> str:
        return self._git('diff', *args)


class GitRepoFinder:
    """An efficient way to map files to the repo that tracks them (if any).

    This class is optimized to minimize subprocess calls to git so that many
    file paths can efficiently be mapped to their parent repo.
    """

    def __init__(self, tool_runner: ToolRunner):
        self.tool_runner = tool_runner
        # A dictionary mapping an absolute path to a directory to the
        # absolute path of the owning repo (if any).
        self._known_repo_roots: dict[Path, Path | None] = {}
        self.repos: dict[Path | None, GitRepo | None] = {None: None}

    def _add_known_repo_path(
        self, repo: Path | None, path_in_repo: Path
    ) -> None:
        path_to_add = (
            path_in_repo.resolve()
            if not repo
            else repo.joinpath(path_in_repo).resolve()
        )
        self._known_repo_roots[path_to_add] = repo

    def _repo_is_known(self, path: Path) -> bool:
        return path.resolve() in self._known_repo_roots

    def find_git_repo(self, path_in_repo: Path | str) -> GitRepo | None:
        """Finds the git repo that contains this pathspec.

        Returns:
            A GitRepo if the file is enclosed by a Git repository, otherwise
            returns None.
        """
        path = Path(path_in_repo)
        search_from = path if path.is_dir() else path.parent
        if not search_from.exists():
            raise ValueError(
                f"Can't find parent repo of `{path_in_repo}`, "
                "path does not exist"
            )

        if not self._repo_is_known(search_from):
            try:
                git_tool = _GitTool(
                    self.tool_runner,
                    search_from,
                )
                root = Path(
                    git_tool(
                        'rev-parse',
                        '--show-toplevel',
                    )
                )
                # Now that we found the absolute path root, we know every
                # directory between the repo root and the query are owned
                # by that repo. For example:
                #   query: bar/baz_subrepo/my_dir/nested/b.txt
                #   cwd: /dev/null/foo_repo/
                #   root: /dev/null/foo_repo/bar/baz_subrepo
                #   parents (relative to root):
                #     my_dir/nested
                #     my_dir
                #   new known git paths:
                #     /dev/null/foo_repo/bar/baz_subrepo/my_dir/nested
                #     /dev/null/foo_repo/bar/baz_subrepo/my_dir
                #     /dev/null/foo_repo/bar/baz_subrepo
                subpath = search_from.resolve().relative_to(root)
                for parent in itertools.chain([subpath], subpath.parents):
                    if self._repo_is_known(root.joinpath(parent)):
                        break
                    self._add_known_repo_path(root, root.joinpath(parent))

                if root not in self.repos:
                    self.repos[root] = GitRepo(root, self.tool_runner)

                return self.repos[root]

            except GitError:
                for parent in itertools.chain(
                    [search_from], search_from.parents
                ):
                    self._add_known_repo_path(None, search_from)

            return None

        return self.repos[self._known_repo_roots[search_from.resolve()]]

    def make_pathspec_relative(
        self, pathspec: Path | str
    ) -> tuple[GitRepo | None, str]:
        """Finds the root repo of a pathspec, and then relativizes the pathspec.

        Example: Assuming a repo at `external/foo_repo/` and a pathspec of
        `external/foo_repo/ba*`, returns a GitRepo at `external/foo_repo` and
        a relativized pathspec of `ba*`.

        Args:
            pathspec: The pathspec to relativize.
        Returns:
            The GitRepo of the pathspec and the pathspec relative to the parent
            repo's root as a tuple. If the pathspec is not tracked by a repo,
            the GitRepo is None and the pathspec is returned as-is.
        """
        repo = self.find_git_repo(pathspec)

        if repo is None:
            return None, str(pathspec)

        if Path(pathspec).is_absolute():
            relative_pattern = Path(pathspec).relative_to(repo.root())
        else:
            # Don't resolve(), we don't want to follow symlinks.
            logical_absolute = Path.cwd() / Path(pathspec)
            relative_pattern = Path(logical_absolute).relative_to(repo.root())

        # Sometimes the effective pathspec is empty because it matches the root
        # directory of a repo.
        if not relative_pattern:
            return repo, str(Path('.'))

        return repo, str(relative_pattern)


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
