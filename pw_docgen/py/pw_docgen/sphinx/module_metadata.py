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
"""Sphinx directives for Pigweed module metadata"""

from typing import cast, Dict, List

import docutils
from docutils import nodes
import docutils.statemachine

# pylint: disable=consider-using-from-import
import docutils.parsers.rst.directives as directives  # type: ignore

# pylint: enable=consider-using-from-import
from sphinx.application import Sphinx as SphinxApplication
from sphinx.util.docutils import SphinxDirective
from sphinx_design.badges_buttons import ButtonRefDirective
from sphinx_design.cards import CardDirective


def status_choice(arg):
    return directives.choice(arg, ('experimental', 'unstable', 'stable'))


def status_badge(module_status: str) -> str:
    """Given a module status, return the status badge for rendering."""
    role = ':bdg-primary:'
    return role + f'`{module_status.title()}`'


def cs_url(module_name: str) -> str:
    """Return the codesearch URL for the given module."""
    return f'https://cs.opensource.google/pigweed/pigweed/+/main:{module_name}/'


def concat_tags(*tag_lists: List[str]) -> List[str]:
    """Given a list of tag lists, return them concat'ed and ready for render."""

    all_tags = tag_lists[0]

    for tag_list in tag_lists[1:]:
        if len(tag_list) > 0:
            all_tags.append(':octicon:`dot-fill`')
            all_tags.extend(tag_list)

    return all_tags


def parse_nav(nav_option: str) -> Dict[str, str]:
    """Parse the `nav` option.

    The value for this option must contain key-value pairs separated by ':',
    with each pair on a separate line. The value is a Sphinx ref pointing to
    the destination of the nav button link, and the key is the name of the nav
    button (see :func:`nav_name`).
    """
    try:
        return {
            line.split(':')[0].strip(): line.split(':')[1].strip()
            for line in nav_option.split('\n')
        }
    except IndexError:
        raise ValueError(
            'Nav parsing failure. Ensure each line has a single pair in the '
            f'form `name: ref`. You provided this:\n{nav_option}'
        )


def nav_name(name: str) -> str:
    """Return the display name for a particular nav button name.

    By default, the returned display name be the provided name with the words
    capitalized. However, this can be overridden by adding an entry to the
    canonical names list.
    """
    canonical_names: Dict[str, str] = {
        'api': 'API Reference',
        'cli': 'CLI Reference',
        'gui': 'GUI Reference',
    }

    if name in canonical_names:
        return canonical_names[name]

    return ' '.join(
        [name_part.lower().capitalize() for name_part in name.split()]
    )


class PigweedModuleDirective(SphinxDirective):
    """Directive registering module metadata, rendering title & info card."""

    required_arguments = 0
    final_argument_whitespace = True
    has_content = True
    option_spec = {
        'name': directives.unchanged_required,
        'tagline': directives.unchanged_required,
        'status': status_choice,
        'is-deprecated': directives.flag,
        'languages': directives.unchanged,
        'code-size-impact': directives.unchanged,
        'facade': directives.unchanged,
        'nav': directives.unchanged_required,
    }

    def _try_get_option(self, option: str):
        """Try to get an option by name and raise on failure."""

        try:
            return self.options[option]
        except KeyError:
            raise self.error(f' :{option}: option is required')

    def _maybe_get_option(self, option: str):
        """Try to get an option by name and return None on failure."""
        return self.options.get(option, None)

    def create_nav_button(self, title: str, ref: str) -> nodes.list_item:
        """Generate a renderable node for a nav button."""

        node = nodes.list_item(classes=['pw-module-section-button'])
        node += ButtonRefDirective(
            name='',
            arguments=[ref],
            options={'color': 'primary'},
            content=[title],
            lineno=0,
            content_offset=0,
            block_text='',
            state=self.state,
            state_machine=self.state_machine,
        ).run()

        return node

    def run(self) -> List[nodes.Node]:
        tagline = nodes.paragraph(
            classes=['section-subtitle'],
            text=self._try_get_option('tagline'),
        )

        status_tags: List[str] = [
            status_badge(self._try_get_option('status')),
        ]

        if 'is-deprecated' in self.options:
            status_tags.append(':bdg-danger:`Deprecated`')

        language_tags = []

        if 'languages' in self.options:
            languages = self.options['languages'].split(',')

            if len(languages) > 0:
                for language in languages:
                    language_tags.append(f':bdg-info:`{language.strip()}`')

        code_size_impact = []

        if code_size_text := self._maybe_get_option('code-size-impact'):
            code_size_impact.append(f'**Code Size Impact:** {code_size_text}')

        # Move the directive content into a section that we can render wherever
        # we want.
        raw_content = cast(List[str], self.content)  # type: ignore
        content = nodes.paragraph()
        self.state.nested_parse(raw_content, 0, content)

        # The card inherits its content from this node's content, which we've
        # already pulled out. So we can replace this node's content with the
        # content we need in the card.
        self.content = docutils.statemachine.StringList(
            concat_tags(status_tags, language_tags, code_size_impact)
        )

        card = CardDirective.create_card(
            inst=self,
            arguments=[],
            options={},
        )

        try:
            nav = parse_nav(self._try_get_option('nav'))
        except ValueError as err:
            raise self.error(err)

        # Create the top-level section buttons.
        section_buttons = nodes.bullet_list(
            classes=['pw-module-section-buttons']
        )

        for name, ref in nav.items():
            nav_button = self.create_nav_button(nav_name(name), ref)
            section_buttons += nav_button

        return [
            tagline,
            section_buttons,
            content,
            card,
        ]


class PigweedModuleSubpageDirective(PigweedModuleDirective):
    """Directive registering module metadata, rendering title & info card."""

    required_arguments = 0
    final_argument_whitespace = True
    has_content = True
    option_spec = {
        'name': directives.unchanged_required,
        'tagline': directives.unchanged_required,
        'nav': directives.unchanged_required,
    }

    def run(self) -> List[nodes.Node]:
        tagline = nodes.paragraph(
            classes=['section-subtitle'],
            text=self._try_get_option('tagline'),
        )

        # Move the directive content into a section that we can render wherever
        # we want.
        raw_content = cast(List[str], self.content)  # type: ignore
        content = nodes.paragraph()
        self.state.nested_parse(raw_content, 0, content)

        card = CardDirective.create_card(
            inst=self,
            arguments=[],
            options={},
        )

        try:
            nav = parse_nav(self._try_get_option('nav'))
        except ValueError as err:
            raise self.error(err)

        # Create the top-level section buttons.
        section_buttons = nodes.bullet_list(
            classes=['pw-module-section-buttons']
        )

        nav_buttons: List[nodes.list_item] = []

        for name, ref in nav.items():
            nav_button = self.create_nav_button(nav_name(name), ref)
            section_buttons += nav_button
            nav_buttons.append(nav_button)

        return [
            tagline,
            section_buttons,
            content,
            card,
        ]


def setup(app: SphinxApplication):
    app.add_directive('pigweed-module', PigweedModuleDirective)
    app.add_directive('pigweed-module-subpage', PigweedModuleSubpageDirective)

    return {
        'parallel_read_safe': True,
        'parallel_write_safe': True,
    }
