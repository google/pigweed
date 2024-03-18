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
"""GN output support."""

from typing import Any, Optional

import pathlib

try:
    from pw_build_mcuxpresso.components import Project
except ImportError:
    # Load from this directory if pw_build_mcuxpresso is not available.
    from components import Project  # type: ignore


def _gn_list_str_out(name: str, val: list[Any]):
    """Outputs list of strings in GN format with correct escaping."""
    list_str = ','.join(
        '"' + str(x).replace('"', r'\"').replace('$', r'\$') + '"' for x in val
    )
    print(f'{name} = [{list_str}]')


def _gn_list_path_out(
    name: str, val: list[pathlib.Path], path_prefix: Optional[str] = None
):
    """Outputs list of paths in GN format with common prefix."""
    if path_prefix is not None:
        str_val = list(f'{path_prefix}/{str(d)}' for d in val)
    else:
        str_val = list(str(d) for d in val)
    _gn_list_str_out(name, str_val)


def gn_output(project: Project, path_prefix: Optional[str] = None):
    """Output GN scope for a project with the specified components.

    Args:
        project: MCUXpresso project to output..
        path_prefix: string prefix to prepend to all paths.
    """
    _gn_list_str_out('defines', project.defines)
    _gn_list_path_out(
        'include_dirs', project.include_dirs, path_prefix=path_prefix
    )
    _gn_list_path_out('public', project.headers, path_prefix=path_prefix)
    _gn_list_path_out('sources', project.sources, path_prefix=path_prefix)
    _gn_list_path_out('libs', project.libs, path_prefix=path_prefix)
