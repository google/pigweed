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
"""Implementation of the custom pw-status-codes reStructuredText directive.

The main goal of this directive is to improve the consistency of the
presentation of our API references. All C++ functions and methods that return
a set of pw::Status codes use this directive.

This directive is mainly used within the Doxygen comments of C++ header files.
Usage example:

/// @returns @rst
///
/// .. pw-status-codes::
///
///    OK: The bulk read was successful.
///
///    DEADLINE_EXCEEDED: Can't acquire
///    bus access. See :ref:`x`.
///
/// @endrst

The word before the colon must be a valid pw_status code. The text after the
colon is a description of what the status code means in this particular
scenario. Descriptions may span multiple lines and can contain nested
reStructuredText.
"""
import sys
from typing import List, Tuple

from docutils import nodes
from docutils.parsers.rst import Directive
from docutils.statemachine import ViewList
from pw_status import Status
from sphinx.application import Sphinx


help_url = 'https://pigweed.dev/docs/style/doxygen.html#pw-status-codes'


class PwStatusCodesDirective(Directive):
    """Renders `pw-status-codes` directives."""

    has_content = True

    # The main event handler for the directive.
    # https://docutils.sourceforge.io/docs/howto/rst-directives.html
    def run(self) -> list[nodes.Node]:
        merged_data = self._merge()
        transformed_data = self._transform(merged_data)
        return self._render(transformed_data)

    def _merge(self) -> List[str]:
        """Merges the content into a format that's easier to work with.

        Docutils provides content line-by-line, with empty strings
        representing newlines:

        [
            'OK: The bulk read was successful.',
            '',
            'DEADLINE_EXCEEDED: Can't acquire',
            'bus access. See :ref:`x`.',
        ]

        _merge() consolidates each code and its accompanying description into
        a single string representing the complete key-value pair (KVP) and
        removes the empty strings:

        [
            'OK: The bulk read was successful.',
            'DEADLINE_EXCEEDED: Can't acquire bus access. See :ref:`x`.'
        ]

        We also need to handle the scenario of getting a single line of
        content:

        [
            'OK: The operation was successful.'
        ]
        """
        kvps = []  # key-value pairs
        index = 0
        kvps.append('')
        content: ViewList = self.content
        for line in content:
            # An empty line in the source content means that we're about to
            # encounter a new kvp. See the empty line after 'OK: The bulk read
            # was successful.' for an example.
            if line == '':
                # Make sure that the current kvp has content before starting
                # a new kvp.
                if kvps[index] != '':
                    index += 1
                    kvps.append('')
            else:  # The line has content we need.
                # If the current line isn't empty and our current kvp also
                # isn't empty then we're dealing with the multi-line scenario.
                # See the DEADLINE_EXCEEDED example. We just need to add back
                # the whitespace that was stripped out.
                if kvps[index] != '':
                    kvps[index] += ' '
            # All edge cases have been handled and it's now safe to always add
            # the current line to the current kvp.
            kvps[index] += line
        return kvps

    def _transform(self, items: List[str]) -> List[Tuple[str, str]]:
        """Transforms the data into normalized and annotated tuples.

        _normalize() gives us data like this:

        [
            'OK: The bulk read was successful.',
        ]

        _transform() changes it to a list of tuples where the first element
        is the status code converted into a cross-reference and the second
        element is the description.

        [
            (':c:enumerator:`OK`', 'The bulk read was successful.'),
        ]

        This function exits if any provided status code is invalid.
        """
        data = []
        valid_codes = [member.name for member in Status]
        for item in items:
            code = item.split(':', 1)[0]
            desc = item.split(':', 1)[1].strip()
            if code not in valid_codes:
                docname = self.state.document.settings.env.docname
                error = (
                    f'[error] pw-status-codes found invalid code: {code}\n'
                    f'        offending content: {item}\n'
                    f'        included in following file: {docname}.rst\n'
                    f'        help: {help_url}\n'
                )
                sys.exit(error)
            link = f':c:enumerator:`{code}`'
            data.append((link, desc))
        return data

    def _render(self, data: List[Tuple[str, str]]) -> List[nodes.Node]:
        """Renders the content as a table.

        _transform() gives us data like this:

        [
            (':c:enumerator:`OK`', 'The bulk read was successful.'),
        ]

        _render() structures it as a csv-table:

        .. csv-table:
           :header: "Code", "Description"
           :align: left

           ":c:enumerator:`OK`", "The bulk read was successful."

        And then lets Sphinx/Docutils do the rendering. This has the added
        bonus of making it possible to use inline reStructuredText elements
        like code formatting and cross-references in the descriptions.
        """
        table = [
            '.. csv-table::',
            '   :header: "Code", "Description"',
            '   :align: left',
            '',
        ]
        for item in data:
            table.append(f'   "{item[0]}", "{item[1]}"')
        container = nodes.container()
        container['classes'] = ['pw-status-codes']
        # Docutils doesn't accept normal lists; it must be a ViewList.
        # inclusive-language: disable
        # https://www.sphinx-doc.org/en/master/extdev/markupapi.html#viewlists
        # inclusive-language: enable
        self.state.nested_parse(ViewList(table), self.content_offset, container)
        return [container]


def setup(app: Sphinx) -> dict[str, bool]:
    """Registers the pw-status-codes directive in the Sphinx build system."""
    app.add_directive('pw-status-codes', PwStatusCodesDirective)
    return {
        'parallel_read_safe': True,
        'parallel_write_safe': True,
    }
