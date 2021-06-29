# Copyright 2021 The Pigweed Authors
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
"""Help window container class."""

import logging
import inspect
from pathlib import Path
from typing import Dict

from jinja2 import Template
from prompt_toolkit.document import Document
from prompt_toolkit.filters import Condition
from prompt_toolkit.key_binding import KeyBindings
from prompt_toolkit.layout import (
    ConditionalContainer,
    DynamicContainer,
    HSplit,
)
from prompt_toolkit.layout.dimension import Dimension
from prompt_toolkit.widgets import Box, Frame, TextArea

_LOG = logging.getLogger(__package__)

HELP_TEMPLATE_PATH = Path(__file__).parent / "templates" / "keybind_list.jinja"
with HELP_TEMPLATE_PATH.open() as tmpl:
    KEYBIND_TEMPLATE = tmpl.read()


class HelpWindow(ConditionalContainer):
    """Help window container for displaying keybindings."""
    def __init__(self, application, preamble='', additional_help_text=''):
        # Dict containing key = section title and value = list of key bindings.
        self.help_text_sections = {}
        self.max_description_width = 0
        self.max_key_list_width = 0
        self.max_line_length = 0

        # Generated keybinding text
        self.preamble = preamble
        self.additional_help_text = additional_help_text
        self.help_text = ''

        self.help_text_area = TextArea(
            focusable=True,
            scrollbar=True,
            style='class:help_window_content',
        )

        frame = Frame(
            body=Box(
                body=DynamicContainer(lambda: self.help_text_area),
                padding=Dimension(preferred=1, max=1),
                padding_bottom=0,
                padding_top=0,
                char=' ',
                style='class:frame.border',  # Same style used for Frame.
            ), )

        super().__init__(
            HSplit([frame]),
            filter=Condition(lambda: application.show_help_window),
        )

    def content_width(self) -> int:
        """Return total width of help window."""
        # Widths of UI elements
        frame_width = 1
        padding_width = 1
        left_side_frame_and_padding_width = frame_width + padding_width
        right_side_frame_and_padding_width = frame_width + padding_width
        scrollbar_padding = 1
        scrollbar_width = 1

        return self.max_line_length + (left_side_frame_and_padding_width +
                                       right_side_frame_and_padding_width +
                                       scrollbar_padding + scrollbar_width)

    def generate_help_text(self):
        """Generate help text based on added key bindings."""

        # pylint: disable=line-too-long
        template = Template(
            KEYBIND_TEMPLATE,
            trim_blocks=True,
            lstrip_blocks=True,
        )

        self.help_text = template.render(
            sections=self.help_text_sections,
            max_description_width=self.max_description_width,
            max_key_list_width=self.max_key_list_width,
            preamble=self.preamble,
            additional_help_text=self.additional_help_text,
        )

        # Find the longest line in the rendered template.
        self.max_line_length = 0
        for line in self.help_text.splitlines():
            if len(line) > self.max_line_length:
                self.max_line_length = len(line)

        # Replace the TextArea content.
        self.help_text_area.buffer.document = Document(text=self.help_text,
                                                       cursor_position=0)

        return self.help_text

    def add_custom_keybinds_help_text(self, section_name, key_bindings: Dict):
        """Add hand written key_bindings."""
        self.help_text_sections[section_name] = key_bindings

    def add_keybind_help_text(self, section_name, key_bindings: KeyBindings):
        """Append formatted key binding text to this help window."""

        # Create a new keybind section, erasing any old section with thesame
        # title.
        self.help_text_sections[section_name] = {}

        # Loop through passed in prompt_toolkit key_bindings.
        for binding in key_bindings.bindings:
            # Skip this keybind if the method name ends in _hidden.
            if binding.handler.__name__.endswith('_hidden'):
                continue

            # Get the key binding description from the function doctstring.
            docstring = binding.handler.__doc__
            if not docstring:
                docstring = ''
            description = inspect.cleandoc(docstring)
            description = description.replace('\n', ' ')

            # Save the length of the description.
            if len(description) > self.max_description_width:
                self.max_description_width = len(description)

            # Get the existing list of keys for this function or make a new one.
            key_list = self.help_text_sections[section_name].get(
                description, list())

            # Save the name of the key e.g. F1, q, ControlQ, ControlUp
            key_name = getattr(binding.keys[0], 'name', str(binding.keys[0]))
            key_list.append(key_name)

            key_list_width = len(', '.join(key_list))
            # Save the length of the key list.
            if key_list_width > self.max_key_list_width:
                self.max_key_list_width = key_list_width

            # Update this functions key_list
            self.help_text_sections[section_name][description] = key_list
