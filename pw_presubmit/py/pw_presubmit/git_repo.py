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
"""Helpful commands for working with a Git repository."""

import collections
from pathlib import Path
import subprocess
from typing import Collection, Dict, Iterable, List, Optional
from typing import Pattern, Union

from pw_presubmit.tools import log_run, plural

PathOrStr = Union[Path, str]


def git_stdout(*args: PathOrStr,
               show_stderr=False,
               repo: PathOrStr = '.') -> str:
    return log_run(['git', '-C', repo, *args],
                   stdout=subprocess.PIPE,
                   stderr=None if show_stderr else subprocess.DEVNULL,
                   check=True).stdout.decode().strip()


def _ls_files(args: Collection[PathOrStr], repo: Path) -> Iterable[Path]:
    """Returns results of git ls-files as absolute paths."""
    git_root = repo.resolve()
    for file in git_stdout('ls-files', '--', *args, repo=repo).splitlines():
        yield git_root / file


def _diff_names(commit: str, pathspecs: Collection[PathOrStr],
                repo: Path) -> Iterable[Path]:
    """Returns absolute paths of files changed since the specified commit."""
    git_root = root(repo)
    for file in git_stdout('diff',
                           '--name-only',
                           '--diff-filter=d',
                           commit,
                           '--',
                           *pathspecs,
                           repo=repo).splitlines():
        yield git_root / file


def list_files(commit: Optional[str] = None,
               pathspecs: Collection[PathOrStr] = (),
               repo_path: Optional[Path] = None) -> List[Path]:
    """Lists files with git ls-files or git diff --name-only.

    Args:
      commit: commit to use as a base for git diff
      pathspecs: Git pathspecs to use in git ls-files or diff
      repo_path: repo path from which to run commands; defaults to Path.cwd()

    Returns:
      A sorted list of absolute paths
    """
    if repo_path is None:
        repo_path = Path.cwd()

    if commit:
        return sorted(_diff_names(commit, pathspecs, repo_path))

    return sorted(_ls_files(pathspecs, repo_path))


def has_uncommitted_changes(repo: Optional[Path] = None) -> bool:
    """Returns True if the Git repo has uncommitted changes in it.

    This does not check for untracked files.
    """
    if repo is None:
        repo = Path.cwd()

    # Refresh the Git index so that the diff-index command will be accurate.
    log_run(['git', '-C', repo, 'update-index', '-q', '--refresh'], check=True)

    # diff-index exits with 1 if there are uncommitted changes.
    return log_run(['git', '-C', repo, 'diff-index', '--quiet', 'HEAD',
                    '--']).returncode == 1


def _describe_constraints(git_root: Path, repo_path: Path,
                          commit: Optional[str],
                          pathspecs: Collection[PathOrStr],
                          exclude: Collection[Pattern[str]]) -> Iterable[str]:
    if not git_root.samefile(repo_path):
        yield (
            f'under the {repo_path.resolve().relative_to(git_root.resolve())} '
            'subdirectory')

    if commit:
        yield f'that have changed since {commit}'

    if pathspecs:
        paths_str = ', '.join(str(p) for p in pathspecs)
        yield f'that match {plural(pathspecs, "pathspec")} ({paths_str})'

    if exclude:
        yield (f'that do not match {plural(exclude, "pattern")} (' +
               ', '.join(p.pattern for p in exclude) + ')')


def describe_files(git_root: Path, repo_path: Path, commit: Optional[str],
                   pathspecs: Collection[PathOrStr],
                   exclude: Collection[Pattern]) -> str:
    """Completes 'Doing something to ...' for a set of files in a Git repo."""
    constraints = list(
        _describe_constraints(git_root, repo_path, commit, pathspecs, exclude))
    if not constraints:
        return f'all files in the {git_root.name} repo'

    msg = f'files in the {git_root.name} repo'
    if len(constraints) == 1:
        return f'{msg} {constraints[0]}'

    return msg + ''.join(f'\n    - {line}' for line in constraints)


def root(repo_path: PathOrStr = '.', *, show_stderr: bool = True) -> Path:
    """Returns the repository root as an absolute path.

    Raises:
      FileNotFoundError: the path does not exist
      subprocess.CalledProcessError: the path is not in a Git repo
    """
    repo_path = Path(repo_path)
    if not repo_path.exists():
        raise FileNotFoundError(f'{repo_path} does not exist')

    return Path(
        git_stdout('rev-parse',
                   '--show-toplevel',
                   repo=repo_path,
                   show_stderr=show_stderr))


def within_repo(repo_path: PathOrStr = '.') -> Optional[Path]:
    """Similar to root(repo_path), returns None if the path is not in a repo."""
    try:
        return root(repo_path, show_stderr=False)
    except subprocess.CalledProcessError:
        return None


def is_repo(repo_path: PathOrStr = '.') -> bool:
    """True if the path is tracked by a Git repo."""
    return within_repo(repo_path) is not None


def path(repo_path: PathOrStr,
         *additional_repo_paths: PathOrStr,
         repo: PathOrStr = '.') -> Path:
    """Returns a path relative to a Git repository's root."""
    return root(repo).joinpath(repo_path, *additional_repo_paths)


def find_python_packages(python_paths: Iterable[PathOrStr],
                         repo: PathOrStr = '.') -> Dict[Path, List[Path]]:
    """Returns Python package directories for the files in python_paths."""
    setup_pys = [
        file.parent.as_posix()
        for file in _ls_files(['setup.py', '*/setup.py'], Path(repo))
    ]

    package_dirs: Dict[Path, List[Path]] = collections.defaultdict(list)

    for python_path in (Path(p).resolve().as_posix() for p in python_paths):
        try:
            setup_dir = max(setup for setup in setup_pys
                            if python_path.startswith(setup))
            package_dirs[Path(setup_dir).resolve()].append(Path(python_path))
        except ValueError:
            continue

    return package_dirs
