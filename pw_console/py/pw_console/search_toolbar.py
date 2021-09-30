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
"""SearchToolbar class used by LogPanes."""

from __future__ import annotations
import functools
from typing import TYPE_CHECKING

from prompt_toolkit.buffer import Buffer
from prompt_toolkit.filters import (
    Condition, )
from prompt_toolkit.key_binding import KeyBindings, KeyPressEvent
from prompt_toolkit.layout import (
    ConditionalContainer,
    FormattedTextControl,
    HSplit,
    Window,
    WindowAlign,
)
from prompt_toolkit.widgets import TextArea
from prompt_toolkit.validation import DynamicValidator

from pw_console.log_view import RegexValidator, SearchMatcher
# import pw_console.widgets.checkbox
import pw_console.widgets.mouse_handlers

if TYPE_CHECKING:
    from pw_console.log_pane import LogPane


class SearchToolbar(ConditionalContainer):
    """One line toolbar for entering search text."""

    TOOLBAR_HEIGHT = 3

    def focus_self(self):
        self.log_pane.application.application.layout.focus(self)

    def close_search_bar(self):
        """Close search bar."""
        # Reset invert setting for the next search
        self._search_invert = False
        # Hide the search bar
        self.log_pane.search_bar_active = False
        # Focus on the log_pane.
        self.log_pane.application.focus_on_container(self.log_pane)
        self.log_pane.redraw_ui()

    def _start_search(self):
        self.input_field.buffer.validate_and_handle()

    def _invert_search(self):
        self._search_invert = not self._search_invert

    def _next_field(self):
        fields = self.log_pane.log_view.log_store.table.all_column_names()
        fields.append(None)
        current_index = fields.index(self._search_field)
        next_index = (current_index + 1) % len(fields)
        self._search_field = fields[next_index]

    def create_filter(self):
        self._start_search()
        if self._search_successful:
            self.log_pane.log_view.apply_filter()

    def get_search_help_fragments(self):
        """Return FormattedText with search general help keybinds."""
        focus = functools.partial(pw_console.widgets.mouse_handlers.on_click,
                                  self.focus_self)
        start_search = functools.partial(
            pw_console.widgets.mouse_handlers.on_click, self._start_search)
        add_filter = functools.partial(
            pw_console.widgets.mouse_handlers.on_click, self.create_filter)
        clear_filters = functools.partial(
            pw_console.widgets.mouse_handlers.on_click,
            self.log_pane.log_view.clear_filters)
        close_search = functools.partial(
            pw_console.widgets.mouse_handlers.on_click, self.close_search_bar)

        # Search toolbar is darker than pane toolbars, use the darker button
        # style here.
        button_style = 'class:toolbar-button-inactive'

        separator_text = [('', '  ', focus)]

        # Empty text matching the width of the search bar title.
        fragments = [
            ('', '        ', focus),
        ]
        fragments.extend(separator_text)

        fragments.extend(
            pw_console.widgets.checkbox.to_keybind_indicator(
                'Enter', 'Search', start_search, base_style=button_style))
        fragments.extend(separator_text)

        fragments.extend(
            pw_console.widgets.checkbox.to_keybind_indicator(
                'Ctrl-Alt-f',
                'Add Filter',
                add_filter,
                base_style=button_style))
        fragments.extend(separator_text)

        fragments.extend(
            pw_console.widgets.checkbox.to_keybind_indicator(
                'Ctrl-Alt-r',
                'Clear Filters',
                clear_filters,
                base_style=button_style))
        fragments.extend(separator_text)

        fragments.extend(
            pw_console.widgets.checkbox.to_keybind_indicator(
                'Ctrl-c', 'Close', close_search, base_style=button_style))

        fragments.extend(separator_text)
        return fragments

    def get_search_settings_fragments(self):
        """Return FormattedText with current search settings and keybinds."""
        focus = functools.partial(pw_console.widgets.mouse_handlers.on_click,
                                  self.focus_self)
        next_field = functools.partial(
            pw_console.widgets.mouse_handlers.on_click, self._next_field)
        toggle_invert = functools.partial(
            pw_console.widgets.mouse_handlers.on_click, self._invert_search)
        next_matcher = functools.partial(
            pw_console.widgets.mouse_handlers.on_click,
            self.log_pane.log_view.select_next_search_matcher)

        separator_text = [('', '  ', focus)]

        # Search toolbar is darker than pane toolbars, use the darker button
        # style here.
        button_style = 'class:toolbar-button-inactive'

        fragments = [
            # Title
            ('class:search-bar-title', ' Search ', focus),
        ]
        fragments.extend(separator_text)

        selected_column_text = [
            (button_style + ' class:search-bar-setting',
             (self._search_field.title() if self._search_field else 'All'),
             next_field),
        ]
        fragments.extend(
            pw_console.widgets.checkbox.to_keybind_indicator(
                'Ctrl-t',
                'Column:',
                next_field,
                middle_fragments=selected_column_text,
                base_style=button_style,
            ))
        fragments.extend(separator_text)

        fragments.extend(
            pw_console.widgets.checkbox.to_checkbox_with_keybind_indicator(
                self._search_invert,
                'Ctrl-v',
                'Invert',
                toggle_invert,
                base_style=button_style))
        fragments.extend(separator_text)

        # Matching Method
        current_matcher_text = [
            (button_style + ' class:search-bar-setting',
             str(self.log_pane.log_view.search_matcher.name), next_matcher)
        ]
        fragments.extend(
            pw_console.widgets.checkbox.to_keybind_indicator(
                'Ctrl-n',
                'Matcher:',
                next_matcher,
                middle_fragments=current_matcher_text,
                base_style=button_style,
            ))
        fragments.extend(separator_text)

        return fragments

    def get_search_matcher(self):
        if self.log_pane.log_view.search_matcher == SearchMatcher.REGEX:
            return self.log_pane.log_view.search_validator
        return False

    def __init__(self, log_pane: 'LogPane'):
        self.log_pane = log_pane
        self.search_validator = RegexValidator()
        self._search_successful = False
        self._search_invert = False
        self._search_field = None

        # FormattedText of the search column headers.
        self.input_field = TextArea(
            prompt=[
                ('class:search-bar-setting', '/',
                 functools.partial(pw_console.widgets.mouse_handlers.on_click,
                                   self.focus_self))
            ],
            focusable=True,
            focus_on_click=True,
            scrollbar=False,
            multiline=False,
            height=1,
            dont_extend_height=True,
            dont_extend_width=False,
            accept_handler=self._search_accept_handler,
            validator=DynamicValidator(self.get_search_matcher),
            history=self.log_pane.application.search_history,
        )

        search_help_bar_control = FormattedTextControl(
            self.get_search_help_fragments)
        search_help_bar_window = Window(content=search_help_bar_control,
                                        height=1,
                                        align=WindowAlign.LEFT,
                                        dont_extend_width=False)

        search_settings_bar_control = FormattedTextControl(
            self.get_search_settings_fragments)
        search_settings_bar_window = Window(
            content=search_settings_bar_control,
            height=1,
            align=WindowAlign.LEFT,
            dont_extend_width=False)

        # Additional keybindings for the text area.
        key_bindings = KeyBindings()

        @key_bindings.add('escape')
        @key_bindings.add('c-c')
        @key_bindings.add('c-d')
        def _close_search_bar(_event: KeyPressEvent) -> None:
            """Close search bar."""
            self.close_search_bar()

        @key_bindings.add('c-n')
        def _select_next_search_matcher(_event: KeyPressEvent) -> None:
            """Select the next search matcher."""
            self.log_pane.log_view.select_next_search_matcher()

        @key_bindings.add('escape', 'c-f')  # Alt-Ctrl-f
        def _create_filter(_event: KeyPressEvent) -> None:
            """Create a filter."""
            self.create_filter()

        @key_bindings.add('c-v')
        def _toggle_search_invert(_event: KeyPressEvent) -> None:
            """Toggle inverted search matching."""
            self._invert_search()

        @key_bindings.add('c-t')
        def _select_next_field(_event: KeyPressEvent) -> None:
            """Select next search field/column."""
            self._next_field()

        # Clear filter keybind is handled by the parent log_pane.

        self.input_field.control.key_bindings = key_bindings

        super().__init__(
            HSplit(
                [
                    search_help_bar_window,
                    search_settings_bar_window,
                    self.input_field,
                ],
                height=SearchToolbar.TOOLBAR_HEIGHT,
                style='class:search-bar',
            ),
            filter=Condition(lambda: log_pane.search_bar_active),
        )

    def _search_accept_handler(self, buff: Buffer) -> bool:
        """Function run when hitting Enter in the search bar."""
        self._search_successful = False
        if len(buff.text) == 0:
            self.close_search_bar()
            # Don't apply an empty search.
            return False

        if self.log_pane.log_view.new_search(buff.text,
                                             invert=self._search_invert,
                                             field=self._search_field):
            self._search_successful = True
            self.close_search_bar()
            # Erase existing search text.
            return False

        # Keep existing text if regex error
        return True
