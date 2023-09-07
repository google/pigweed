# Copyright 2023 The Pigweed Authors
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
"""Bazel output support."""

from typing import Any, List, Optional

import pathlib

try:
    from pw_build_mcuxpresso.components import Project
except ImportError:
    # Load from this directory if pw_build_mcuxpresso is not available.
    from components import Project  # type: ignore


def _bazel_str(val: Any) -> str:
    """Returns string in Bazel format with correct escaping."""
    return str(val).replace('"', r'\"').replace('$', r'\$')


def _bazel_str_out(name: str, val: Any, indent: int = 0) -> None:
    """Outputs string in Bazel format with correct escaping."""
    print('    ' * indent + f'{name} = "{_bazel_str(val)}",')


def _bazel_str_list_out(name: str, vals: List[Any], indent: int = 0) -> None:
    """Outputs list of strings in Bazel format with correct escaping."""
    if not vals:
        return

    print('    ' * indent + f'{name} = [')
    for val in vals:
        print('    ' * (indent + 1) + f'"{_bazel_str(val)}",')
    print('    ' * indent + '],')


def _bazel_path_list_out(
    name: str,
    vals: List[pathlib.Path],
    path_prefix: Optional[str] = None,
    indent: int = 0,
) -> None:
    """Outputs list of paths in Bazel format with common prefix."""
    if path_prefix is not None:
        str_vals = [f'{path_prefix}{str(val)}' for val in vals]
    else:
        str_vals = [str(f) for f in vals]

    _bazel_str_list_out(name, sorted(set(str_vals)), indent=indent)


def bazel_output(
    project: Project, name: str, path_prefix: Optional[str] = None
):
    """Output Bazel target for a project with the specified components.

    Args:
        project: MCUXpresso project to output.
        name: target name to output.
        path_prefix: string prefix to prepend to all paths.
    """
    print('cc_library(')
    _bazel_str_out('name', name, indent=1)
    _bazel_path_list_out(
        'srcs',
        project.sources + project.libs,
        path_prefix=path_prefix,
        indent=1,
    )
    _bazel_path_list_out(
        'hdrs', project.headers, path_prefix=path_prefix, indent=1
    )
    _bazel_str_list_out('defines', project.defines, indent=1)
    _bazel_path_list_out(
        'includes', project.include_dirs, path_prefix=path_prefix, indent=1
    )

    print(')')
