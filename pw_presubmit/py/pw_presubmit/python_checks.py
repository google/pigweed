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
import re
import sys
from typing import List

try:
    import pw_presubmit
except ImportError:
    # Append the pw_presubmit package path to the module search path to allow
    # running this module without installing the pw_presubmit package.
    sys.path.append(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    import pw_presubmit

from pw_presubmit import call, filter_paths, git_repo

_LOG = logging.getLogger(__name__)


def run_module(*args, **kwargs):
    return call('python', '-m', *args, **kwargs)


@filter_paths(endswith='.py')
def test_python_packages(ctx: pw_presubmit.PresubmitContext):
    packages: List[Path] = []
    for repo in ctx.repos:
        packages += git_repo.find_python_packages(ctx.paths, repo=repo)

    if not packages:
        _LOG.info('No Python packages were found.')
        return

    for package in packages:
        call('python', package / 'setup.py', 'test', cwd=package)


@filter_paths(endswith='.py')
def pylint(ctx: pw_presubmit.PresubmitContext):
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


_SETUP_PY = re.compile(r'(?:.+/)?setup\.py')


@filter_paths(endswith='.py')
def mypy(ctx: pw_presubmit.PresubmitContext):
    env = os.environ.copy()
    # Use this environment variable to force mypy to colorize output.
    # See https://github.com/python/mypy/issues/7771
    env['MYPY_FORCE_COLOR'] = '1'

    run_module(
        'mypy',
        *(p for p in ctx.paths if not _SETUP_PY.fullmatch(p.as_posix())),
        '--pretty',
        '--color-output',
        # TODO(pwbug/146): Some imports from installed packages fail. These
        # imports should be fixed and this option removed.
        '--ignore-missing-imports',
        env=env)


_ALL_CHECKS = (
    test_python_packages,
    pylint,
    mypy,
)


def all_checks(endswith='.py', **filter_paths_args):
    return tuple(
        filter_paths(endswith=endswith, **filter_paths_args)(function)
        for function in _ALL_CHECKS)
