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

from typing import List

import docutils

# pylint: disable=consider-using-from-import
import docutils.parsers.rst.directives as directives  # type: ignore

# pylint: enable=consider-using-from-import
from sphinx.application import Sphinx as SphinxApplication
from sphinx.util.docutils import SphinxDirective
from sphinx_design.badges_buttons import ButtonRefDirective  # type: ignore
from sphinx_design.cards import CardDirective  # type: ignore


def status_choice(arg):
    return directives.choice(arg, ('experimental', 'unstable', 'stable'))


def status_badge(module_status: str) -> str:
    role = ':bdg-primary:'
    return role + f'`{module_status.title()}`'


def cs_url(module_name: str):
    return f'https://cs.opensource.google/pigweed/pigweed/+/main:{module_name}/'


def concat_tags(*tag_lists: List[str]):
    all_tags = tag_lists[0]

    for tag_list in tag_lists[1:]:
        if len(tag_list) > 0:
            all_tags.append(':octicon:`dot-fill`')
            all_tags.extend(tag_list)

    return all_tags


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
        'get-started': directives.unchanged_required,
        'tutorials': directives.unchanged,
        'guides': directives.unchanged,
        'concepts': directives.unchanged,
        'design': directives.unchanged_required,
        'api': directives.unchanged,
    }

    def try_get_option(self, option: str):
        try:
            return self.options[option]
        except KeyError:
            raise self.error(f' :{option}: option is required')

    def maybe_get_option(self, option: str):
        try:
            return self.options[option]
        except KeyError:
            return None

    def create_section_button(self, title: str, ref: str):
        node = docutils.nodes.list_item(classes=['pw-module-section-button'])
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

    def register_metadata(self):
        module_name = self.try_get_option('name')

        if 'facade' in self.options:
            facade = self.options['facade']

            # Initialize the module relationship dict if needed
            if not hasattr(self.env, 'pw_module_relationships'):
                self.env.pw_module_relationships = {}

            # Initialize the backend list for this facade if needed
            if facade not in self.env.pw_module_relationships:
                self.env.pw_module_relationships[facade] = []

            # Add this module as a backend of the provided facade
            self.env.pw_module_relationships[facade].append(module_name)

        if 'is-deprecated' in self.options:
            # Initialize the deprecated modules list if needed
            if not hasattr(self.env, 'pw_modules_deprecated'):
                self.env.pw_modules_deprecated = []

            self.env.pw_modules_deprecated.append(module_name)

    def run(self):
        tagline = docutils.nodes.paragraph(
            text=self.try_get_option('tagline'),
            classes=['section-subtitle'],
        )

        status_tags: List[str] = [
            status_badge(self.try_get_option('status')),
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

        if code_size_text := self.maybe_get_option('code-size-impact'):
            code_size_impact.append(f'**Code Size Impact:** {code_size_text}')

        # Move the directive content into a section that we can render wherever
        # we want.
        content = docutils.nodes.paragraph()
        self.state.nested_parse(self.content, 0, content)

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

        # Create the top-level section buttons.
        section_buttons = docutils.nodes.bullet_list(
            classes=['pw-module-section-buttons']
        )

        # This is the pattern for required sections.
        section_buttons += self.create_section_button(
            'Get Started', self.try_get_option('get-started')
        )

        # This is the pattern for optional sections.
        if (tutorials_ref := self.maybe_get_option('tutorials')) is not None:
            section_buttons += self.create_section_button(
                'Tutorials', tutorials_ref
            )

        if (guides_ref := self.maybe_get_option('guides')) is not None:
            section_buttons += self.create_section_button('Guides', guides_ref)

        if (concepts_ref := self.maybe_get_option('concepts')) is not None:
            section_buttons += self.create_section_button(
                'Concepts', concepts_ref
            )

        section_buttons += self.create_section_button(
            'Design', self.try_get_option('design')
        )

        if (api_ref := self.maybe_get_option('api')) is not None:
            section_buttons += self.create_section_button(
                'API Reference', api_ref
            )

        return [tagline, section_buttons, content, card]


def build_backend_lists(app, _doctree, _fromdocname):
    env = app.builder.env

    if not hasattr(env, 'pw_module_relationships'):
        env.pw_module_relationships = {}


def setup(app: SphinxApplication):
    app.add_directive('pigweed-module', PigweedModuleDirective)

    # At this event, the documents and metadata have been generated, and now we
    # can modify the doctree to reflect the metadata.
    app.connect('doctree-resolved', build_backend_lists)

    return {
        'parallel_read_safe': True,
        'parallel_write_safe': True,
    }
