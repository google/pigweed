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
from functools import partial
from pathlib import Path

from jinja2 import Template
from prompt_toolkit.filters import Condition
from prompt_toolkit.layout import (
    ConditionalContainer,
    FormattedTextControl,
    Window,
)
from prompt_toolkit.widgets import (Box, Frame)

_LOG = logging.getLogger(__package__)

HELP_TEMPLATE_PATH = Path(__file__).parent / "templates" / "keybind_list.jinja"
with HELP_TEMPLATE_PATH.open() as tmpl:
    KEYBIND_TEMPLATE = tmpl.read()


class HelpWindow(ConditionalContainer):
    """Help window container for displaying keybindings."""
    def get_tokens(self, application):
        """Get all text for the help window."""
        help_window_content = (
            # Style
            'class:help_window_content',
            # Text
            self.help_text,
        )

        if application.show_help_window:
            return [help_window_content]
        return []

    def __init__(self, application):
        # Dict containing key = section title and value = list of key bindings.
        self.help_text_sections = {}
        self.max_description_width = 0
        # Generated keybinding text
        self.help_text = ''

        help_text_window = Window(
            FormattedTextControl(partial(self.get_tokens, application)),
            style='class:help_window_content',
        )

        help_text_window_with_padding = Box(
            body=help_text_window,
            padding=1,
            char=' ',
        )

        super().__init__(
            # Add a text border frame.
            Frame(body=help_text_window_with_padding),
            filter=Condition(lambda: application.show_help_window),
        )

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
        )

        return self.help_text

    def add_keybind_help_text(self, section_name, key_bindings):
        """Append formatted key binding text to this help window."""

        # Create a new keybind section
        if section_name not in self.help_text_sections:
            self.help_text_sections[section_name] = {}

        # Loop through passed in prompt_toolkit key_bindings.
        for binding in key_bindings.bindings:
            # Get the key binding description from the function doctstring.
            description = inspect.cleandoc(binding.handler.__doc__)

            # Save the length of the description.
            if len(description) > self.max_description_width:
                self.max_description_width = len(description)

            # Get the existing list of keys for this function or make a new one.
            key_list = self.help_text_sections[section_name].get(
                description, list())

            # Save the name of the key e.g. F1, q, ControlQ, ControlUp
            key_name = getattr(binding.keys[0], 'name', str(binding.keys[0]))
            key_list.append(key_name)

            # Update this functions key_list
            self.help_text_sections[section_name][description] = key_list
