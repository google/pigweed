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

import json
import logging
import os
import sys
from typing import Optional

try:
    import pw_presubmit
except ImportError:
    # Append the pw_presubmit package path to the module search path to allow
    # running this module without installing the pw_presubmit package.
    sys.path.append(os.path.dirname(os.path.dirname(
        os.path.abspath(__file__))))
    import pw_presubmit

from pw_env_setup import python_packages
from pw_presubmit import (
    build,
    Check,
    filter_paths,
    PresubmitContext,
    PresubmitFailure,
)

_LOG = logging.getLogger(__name__)

_PYTHON_EXTENSIONS = ('.py', '.gn', '.gni')


@filter_paths(endswith=_PYTHON_EXTENSIONS)
def gn_python_check(ctx: PresubmitContext):
    build.gn_gen(ctx.root, ctx.output_dir)
    build.ninja(ctx.output_dir, 'python.tests', 'python.lint')


@filter_paths(endswith=_PYTHON_EXTENSIONS)
def gn_python_lint(ctx: pw_presubmit.PresubmitContext) -> None:
    build.gn_gen(ctx.root, ctx.output_dir)
    build.ninja(ctx.output_dir, 'python.lint')


@Check
def check_python_versions(ctx: PresubmitContext):
    """Checks that the list of installed packages is as expected."""

    build.gn_gen(ctx.root, ctx.output_dir)
    constraint_file: Optional[str] = None
    try:
        for arg in build.get_gn_args(ctx.output_dir):
            if arg['name'] == 'pw_build_PIP_CONSTRAINTS':
                constraint_file = json.loads(
                    arg['current']['value'])[0].strip('/')
    except json.JSONDecodeError:
        _LOG.warning('failed to parse GN args json')
        return

    if not constraint_file:
        _LOG.warning('could not find pw_build_PIP_CONSTRAINTS GN arg')
        return

    with (ctx.root / constraint_file).open('r') as ins:
        if python_packages.diff(ins) != 0:
            raise PresubmitFailure
