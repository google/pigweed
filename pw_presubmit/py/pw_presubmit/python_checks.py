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
import sys
from typing import Callable, Tuple

try:
    import pw_presubmit
except ImportError:
    # Append the pw_presubmit package path to the module search path to allow
    # running this module without installing the pw_presubmit package.
    sys.path.append(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    import pw_presubmit

from pw_presubmit import build, filter_paths, PresubmitContext

_LOG = logging.getLogger(__name__)

_PYTHON_EXTENSIONS = ('.py', '.gn', '.gni')


# TODO(mohrr) Remove gn_check=False when it passes for all downstream projects.
@filter_paths(endswith=_PYTHON_EXTENSIONS)
def gn_python_check(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir, gn_check=False)
    build.ninja(ctx.output_dir, 'python.tests', 'python.lint')


# TODO(mohrr) Remove gn_check=False when it passes for all downstream projects.
@filter_paths(endswith=_PYTHON_EXTENSIONS)
def gn_lint(ctx: pw_presubmit.PresubmitContext) -> None:
    build.gn_gen(ctx.root, ctx.output_dir, gn_check=False)
    build.ninja(ctx.output_dir, 'python.lint')


_LINT_CHECKS = (gn_lint, )
_ALL_CHECKS = (gn_python_check, )


# TODO(pwbug/454) Remove after downstream projects switch to using functions
# directly.
def lint_checks(endswith: str = '.py',
                **filter_paths_args) -> Tuple[Callable, ...]:
    return tuple(
        filter_paths(endswith=endswith, **filter_paths_args)(function)
        for function in _LINT_CHECKS)


# TODO(pwbug/454) Remove after downstream projects switch to using functions
# directly.
def all_checks(endswith: str = '.py',
               **filter_paths_args) -> Tuple[Callable, ...]:
    return tuple(
        filter_paths(endswith=endswith, **filter_paths_args)(function)
        for function in _ALL_CHECKS)
