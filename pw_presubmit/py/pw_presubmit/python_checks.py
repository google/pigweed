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
"""Preconfigured checks for Python code.

These checks assume that they are running in a preconfigured Python environment.
"""

import logging
import os
from pathlib import Path
import sys
from typing import Callable, Iterable, List, Set, Tuple

try:
    import pw_presubmit
except ImportError:
    # Append the pw_presubmit package path to the module search path to allow
    # running this module without installing the pw_presubmit package.
    sys.path.append(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    import pw_presubmit

from pw_presubmit import call, filter_paths
from pw_presubmit.git_repo import python_packages_containing, list_files
from pw_presubmit.git_repo import PythonPackage

_LOG = logging.getLogger(__name__)


def run_module(*args, **kwargs):
    return call('python', '-m', *args, **kwargs)


TEST_PATTERNS = ('*_test.py', )


@filter_paths(endswith='.py')
def test_python_packages(ctx: pw_presubmit.PresubmitContext,
                         patterns: Iterable[str] = TEST_PATTERNS) -> None:
    """Finds and runs test files in Python package directories.

    Finds the Python packages containing the affected paths, then searches
    within that package for test files. All files matching the provided patterns
    are executed with Python.
    """
    packages: List[PythonPackage] = []
    for repo in ctx.repos:
        packages += python_packages_containing(ctx.paths, repo=repo)[0]

    if not packages:
        _LOG.info('No Python packages were found.')
        return

    for package in packages:
        for test in list_files(pathspecs=tuple(patterns),
                               repo_path=package.root):
            call('python', test)


@filter_paths(endswith='.py')
def pylint(ctx: pw_presubmit.PresubmitContext) -> None:
    disable_checkers = [
        # BUG(pwbug/22): Hanging indent check conflicts with YAPF 0.29. For
        # now, use YAPF's version even if Pylint is doing the correct thing
        # just to keep operations simpler. When YAPF upstream fixes the issue,
        # delete this code.
        #
        # See also: https://github.com/google/yapf/issues/781
        'bad-continuation',
    ]
    run_module(
        'pylint',
        '--jobs=0',
        f'--disable={",".join(disable_checkers)}',
        *ctx.paths,
        cwd=ctx.root,
    )


@filter_paths(endswith='.py')
def mypy(ctx: pw_presubmit.PresubmitContext) -> None:
    """Runs mypy on all paths and their packages."""
    packages: List[PythonPackage] = []
    other_files: List[Path] = []

    for repo, paths in ctx.paths_by_repo().items():
        new_packages, files = python_packages_containing(paths, repo=repo)
        packages += new_packages
        other_files += files

        for package in new_packages:
            other_files += package.other_files

    # Under some circumstances, mypy cannot check multiple Python files with the
    # same module name. Group filenames so that no duplicates occur in the same
    # mypy invocation. Also, omit setup.py from mypy checks.
    filename_sets: List[Set[str]] = [set()]
    path_sets: List[List[Path]] = [list(p.package for p in packages)]

    for path in (p for p in other_files if p.name != 'setup.py'):
        for filenames, paths in zip(filename_sets, path_sets):
            if path.name not in filenames:
                paths.append(path)
                filenames.add(path.name)
                break
        else:
            path_sets.append([path])
            filename_sets.append({path.name})

    env = os.environ.copy()
    # Use this environment variable to force mypy to colorize output.
    # See https://github.com/python/mypy/issues/7771
    env['MYPY_FORCE_COLOR'] = '1'

    for paths in path_sets:
        run_module(
            'mypy',
            *paths,
            '--pretty',
            '--color-output',
            '--show-error-codes',
            # TODO(pwbug/146): Some imports from installed packages fail. These
            # imports should be fixed and this option removed. See
            # https://mypy.readthedocs.io/en/stable/installed_packages.html
            '--ignore-missing-imports',
            env=env)


_ALL_CHECKS = (
    test_python_packages,
    pylint,
    mypy,
)


def all_checks(endswith: str = '.py',
               **filter_paths_args) -> Tuple[Callable, ...]:
    return tuple(
        filter_paths(endswith=endswith, **filter_paths_args)(function)
        for function in _ALL_CHECKS)
