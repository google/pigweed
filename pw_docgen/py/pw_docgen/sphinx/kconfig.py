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
"""Auto-generate the Kconfig reference in //docs/os/zephyr/kconfig.rst"""


import os
import re
from typing import Iterable

import docutils
from docutils.core import publish_doctree
from sphinx.application import Sphinx
from sphinx.addnodes import document


try:
    import kconfiglib  # type: ignore

    KCONFIGLIB_AVAILABLE = True
except ImportError:
    KCONFIGLIB_AVAILABLE = False


def rst_to_doctree(rst: str) -> Iterable[docutils.nodes.Node]:
    """Convert raw reStructuredText into doctree nodes."""
    # TODO: b/288127315 - Properly resolve references within the rst so that
    # links are generated more robustly.
    while ':ref:`module-' in rst:
        rst = re.sub(
            r':ref:`module-(.*?)`', r'`\1 <https://pigweed.dev/\1>`_', rst
        )
    doctree = publish_doctree(rst)
    return doctree.children


def create_source_paragraph(name_and_loc: str) -> Iterable[docutils.nodes.Node]:
    """Convert kconfiglib's name and location string into a source code link."""
    start = name_and_loc.index('pw_')
    end = name_and_loc.index(':')
    file_path = name_and_loc[start:end]
    url = f'https://cs.opensource.google/pigweed/pigweed/+/main:{file_path}'
    link = f'`//{file_path} <{url}>`_'
    return rst_to_doctree(f'Source: {link}')


def process_node(
    node: kconfiglib.MenuNode, parent: docutils.nodes.Node
) -> None:
    """Recursively generate documentation for the Kconfig nodes."""
    while node:
        if node.item == kconfiglib.MENU:
            name = node.prompt[0]
            # All auto-generated sections must have an ID or else the
            # get_secnumber() function in Sphinx's HTML5 writer throws an
            # IndexError.
            menu_section = docutils.nodes.section(ids=[name])
            menu_section += docutils.nodes.title(text=f'{name} options')
            if node.list:
                process_node(node.list, menu_section)
            parent += menu_section
        elif isinstance(node.item, kconfiglib.Symbol):
            name = f'CONFIG_{node.item.name}'
            symbol_section = docutils.nodes.section(ids=[name])
            symbol_section += docutils.nodes.title(text=name)
            symbol_section += docutils.nodes.paragraph(
                text=f'Type: {kconfiglib.TYPE_TO_STR[node.item.type]}'
            )
            if node.item.defaults:
                try:
                    default_value = node.item.defaults[0][0].str_value
                    symbol_section += docutils.nodes.paragraph(
                        text=f'Default value: {default_value}'
                    )
                # If the data wasn't found, just contine trying to process
                # rest of the documentation for the node.
                except IndexError:
                    pass
            if node.item.ranges:
                try:
                    low = node.item.ranges[0][0].str_value
                    high = node.item.ranges[0][1].str_value
                    symbol_section += docutils.nodes.paragraph(
                        text=f'Range of valid values: {low} to {high}'
                    )
                except IndexError:
                    pass
            if node.prompt:
                try:
                    symbol_section += docutils.nodes.paragraph(
                        text=f'Description: {node.prompt[0]}'
                    )
                except IndexError:
                    pass
            if node.help:
                symbol_section += rst_to_doctree(node.help)
            if node.list:
                process_node(node.list, symbol_section)
            symbol_section += create_source_paragraph(node.item.name_and_loc)
            parent += symbol_section
        # TODO: b/288127315 - Render choices?
        # elif isinstance(node.item, kconfiglib.Choice):
        node = node.next


def generate_kconfig_reference(_, doctree: document, docname: str) -> None:
    """Parse the Kconfig and kick off the doc generation process."""
    if 'docs/os/zephyr/kconfig' not in docname:
        return
    # Assume that the new content should be appended to the last section
    # in the doctree.
    for child in doctree.children:
        if isinstance(child, docutils.nodes.section):
            root = child
    pw_root = os.environ['PW_ROOT']
    file_path = f'{pw_root}/Kconfig.zephyr'
    kconfig = kconfiglib.Kconfig(file_path)
    # There's no need to render kconfig.top_node (the main menu) or
    # kconfig.top_node.list (ZEPHYR_PIGWEED_MODULE).
    process_node(kconfig.top_node.list.next, root)


def setup(app: Sphinx) -> dict[str, bool]:
    """Initialize the Sphinx extension."""
    if KCONFIGLIB_AVAILABLE:
        app.connect('doctree-resolved', generate_kconfig_reference)
    return {'parallel_read_safe': True, 'parallel_write_safe': True}
