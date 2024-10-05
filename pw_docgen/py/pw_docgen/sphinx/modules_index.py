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
"""Generates the modules index table on //docs/modules.rst."""

import json
import os
import sys

from sphinx.application import Sphinx


try:  # Bazel location for the data
    from python.runfiles import runfiles  # type: ignore

    r = runfiles.Create()
    modules_file = r.Rlocation('pigweed/PIGWEED_MODULES')
    r = runfiles.Create()
    metadata_file = r.Rlocation('pigweed/docs/module_metadata.json')
except ImportError:  # GN location for the data
    modules_file = f'{os.environ["PW_ROOT"]}/PIGWEED_MODULES'
    metadata_file = f'{os.environ["PW_ROOT"]}/docs/module_metadata.json'
with open(modules_file, 'r') as f:
    # The complete, authoritative list of modules.
    complete_pigweed_modules_list = f.read().splitlines()
with open(metadata_file, 'r') as f:
    # Module metadata such as supported languages and status.
    metadata = json.load(f)


def build_status_badge(status):
    """Styles the module status as a clickable badge."""
    # This default value should never get used but it's a valid
    # neutral styling in case something goes wrong and it leaks through.
    badge_type = 'info'
    if status == 'stable':
        badge_type = 'primary'
    elif status == 'unstable':
        badge_type = 'secondary'
    elif status == 'experimental':
        badge_type = 'warning'
    elif status == 'deprecated':
        badge_type = 'danger'
    else:
        msg = f'[modules_index.py] error: invalid module status ("{status}")'
        sys.exit(msg)
    # Use bdg-ref-* to make the badge link to the glossary definition for
    # "stable", "experimental", etc.
    # https://sphinx-design.readthedocs.io/en/latest/badges_buttons.html
    return f':bdg-{badge_type}:`{status}`'


def build_row(module_name: str):
    """Builds a row of data for the table. Each module gets a row."""
    ref = f':ref:`module-{module_name}`'
    if module_name not in metadata:
        return f'   "{ref}", "", "", ""\n'
    tagline = metadata[module_name]['tagline']
    status = build_status_badge(metadata[module_name]['status'])
    if 'languages' in metadata[module_name]:
        languages = ', '.join(metadata[module_name]['languages'])
    else:
        languages = ''
    return f'   "{ref}", "{status}", "{tagline}", "{languages}"\n'


def generate_modules_index(_, docname: str, source: list[str]) -> None:
    """Inserts the metadata table into //docs/modules.rst."""
    if docname != 'modules':  # Only run this logic on //docs/modules.rst.
        return
    skip = ['docker']  # Items in //PIGWEED_MODULES that should be skipped.
    # Transform //docs/module_metadata.json into a csv-table reST directive.
    # https://docutils.sourceforge.io/docs/ref/rst/directives.html#csv-table-1
    content = '\n\n.. csv-table::\n'
    content += '   :header: "Name", "Status", "Description", "Languages"\n\n'
    # Loop through the complete, authoritative list (as opposed to the metadata)
    # to guarantee that every module is listed on the modules index page.
    for module in complete_pigweed_modules_list:
        if module in skip:
            continue
        content += build_row(module)
    # Modify the reST of //docs/modules.rst in-place. The auto-generated table
    # is just appended to the reST source text.
    source[0] += content


def setup(app: Sphinx) -> dict[str, bool]:
    """Hooks this extension into Sphinx."""
    app.connect('source-read', generate_modules_index)
    return {
        'parallel_read_safe': True,
        'parallel_write_safe': True,
    }
