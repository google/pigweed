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
"""LogPane Info Toolbar classes."""

from __future__ import annotations
import functools
from typing import TYPE_CHECKING

from prompt_toolkit.filters import (
    Condition,
    has_focus,
)
from prompt_toolkit.layout import (
    ConditionalContainer,
    FormattedTextControl,
    VSplit,
    Window,
    WindowAlign,
    HorizontalAlign,
)
from prompt_toolkit.data_structures import Point

import pw_console.widgets.checkbox
import pw_console.widgets.mouse_handlers
import pw_console.style

if TYPE_CHECKING:
    from pw_console.log_pane import LogPane


class LineInfoBar(ConditionalContainer):
    """One line bar for showing current and total log lines."""
    @staticmethod
    def get_tokens(log_pane: 'LogPane'):
        """Return formatted text tokens for display."""
        tokens = ' Line {} / {} '.format(
            log_pane.log_view.get_current_line() + 1,
            log_pane.log_view.get_total_count(),
        )
        return [('', tokens)]

    def __init__(self, log_pane: 'LogPane'):
        info_bar_control = FormattedTextControl(
            functools.partial(LineInfoBar.get_tokens, log_pane))
        info_bar_window = Window(content=info_bar_control,
                                 align=WindowAlign.RIGHT,
                                 dont_extend_width=True)

        super().__init__(
            VSplit([info_bar_window],
                   height=1,
                   style=functools.partial(pw_console.style.get_toolbar_style,
                                           log_pane,
                                           dim=True),
                   align=HorizontalAlign.RIGHT),
            # Only show current/total line info if not auto-following
            # logs. Similar to tmux behavior.
            filter=Condition(lambda: not log_pane.log_view.follow))


class TableToolbar(ConditionalContainer):
    """One line toolbar for showing table headers."""
    TOOLBAR_HEIGHT = 1

    def __init__(self, log_pane: 'LogPane'):
        # FormattedText of the table column headers.
        table_header_bar_control = FormattedTextControl(
            log_pane.log_view.render_table_header,
            get_cursor_position=lambda: Point(
                log_pane.get_horizontal_scroll_amount(), 0))
        # Left justify the header content.
        table_header_bar_window = Window(
            content=table_header_bar_control,
            align=WindowAlign.LEFT,
            dont_extend_width=False,
            get_horizontal_scroll=log_pane.get_horizontal_scroll_amount,
        )
        super().__init__(
            VSplit([table_header_bar_window],
                   height=1,
                   style=functools.partial(pw_console.style.get_toolbar_style,
                                           log_pane,
                                           dim=True),
                   align=HorizontalAlign.LEFT),
            filter=Condition(lambda: log_pane.table_view and log_pane.log_view.
                             get_total_count() > 0))


class BottomToolbarBar(ConditionalContainer):
    """One line toolbar for display at the bottom of the LogPane."""
    TOOLBAR_HEIGHT = 1

    def get_left_text_tokens(self):
        """Return toolbar indicator and title."""

        title = ' {} '.format(self.log_pane.pane_title())
        focus = functools.partial(pw_console.widgets.mouse_handlers.on_click,
                                  self.log_pane.focus_self)
        return pw_console.style.get_pane_indicator(self.log_pane, title, focus)

    def get_center_text_tokens(self):
        """Return formatted text tokens for display in the center part of the
        toolbar."""

        # Create mouse handler functions.
        focus = functools.partial(pw_console.widgets.mouse_handlers.on_click,
                                  self.log_pane.focus_self)
        toggle_wrap_lines = functools.partial(
            pw_console.widgets.mouse_handlers.on_click,
            self.log_pane.toggle_wrap_lines)
        clear_history = functools.partial(
            pw_console.widgets.mouse_handlers.on_click,
            self.log_pane.clear_history)
        toggle_follow = functools.partial(
            pw_console.widgets.mouse_handlers.on_click,
            self.log_pane.toggle_follow)
        toggle_table_view = functools.partial(
            pw_console.widgets.mouse_handlers.on_click,
            self.log_pane.toggle_table_view)
        start_search = functools.partial(
            pw_console.widgets.mouse_handlers.on_click,
            self.log_pane.start_search)
        copy_lines = functools.partial(
            pw_console.widgets.mouse_handlers.on_click,
            self.log_pane.log_view.copy_visible_lines)

        button_style = pw_console.style.get_button_style(self.log_pane)

        # FormattedTextTuple contents: (Style, Text, Mouse handler)
        separator_text = [('', '  ', focus)
                          ]  # 2 spaces of separaton between keybinds.

        fragments = []
        fragments.extend(separator_text)

        fragments.extend(
            pw_console.widgets.checkbox.to_keybind_indicator(
                '/', 'Search', start_search, base_style=button_style))
        fragments.extend(separator_text)

        fragments.extend(
            pw_console.widgets.checkbox.to_keybind_indicator(
                'Ctrl-c', 'Copy Lines', copy_lines, base_style=button_style))
        fragments.extend(separator_text)

        fragments.extend(
            pw_console.widgets.checkbox.to_checkbox_with_keybind_indicator(
                self.log_pane.log_view.follow,
                'f',
                'Follow',
                toggle_follow,
                base_style=button_style))
        fragments.extend(separator_text)

        fragments.extend(
            pw_console.widgets.checkbox.to_checkbox_with_keybind_indicator(
                self.log_pane.table_view,
                't',
                'Table',
                toggle_table_view,
                base_style=button_style))
        fragments.extend(separator_text)

        fragments.extend(
            pw_console.widgets.checkbox.to_checkbox_with_keybind_indicator(
                self.log_pane.wrap_lines,
                'w',
                'Wrap',
                toggle_wrap_lines,
                base_style=button_style))
        fragments.extend(separator_text)

        fragments.extend(
            pw_console.widgets.checkbox.to_keybind_indicator(
                'C', 'Clear', clear_history, base_style=button_style))
        fragments.extend(separator_text)

        # Remaining whitespace should focus on click.
        fragments.append(('', ' ', focus))

        return fragments

    def get_right_text_tokens(self):
        """Return formatted text tokens for display."""
        focus = functools.partial(pw_console.widgets.mouse_handlers.on_click,
                                  self.log_pane.focus_self)
        fragments = []
        if not has_focus(self.log_pane.__pt_container__())():
            fragments.append((
                'class:toolbar-button-inactive class:toolbar-button-decoration',
                ' ', focus))
            fragments.append(('class:toolbar-button-inactive class:keyhelp',
                              'click to focus', focus))
            fragments.append((
                'class:toolbar-button-inactive class:toolbar-button-decoration',
                ' ', focus))
        fragments.append(
            ('', '  {} '.format(self.log_pane.pane_subtitle()), focus))
        return fragments

    def __init__(self, log_pane: 'LogPane'):
        self.log_pane = log_pane

        title_section_window = Window(
            content=FormattedTextControl(
                # Callable to get formatted text tuples.
                self.get_left_text_tokens),
            align=WindowAlign.LEFT,
            dont_extend_width=True,
        )

        keybind_section_window = Window(
            content=FormattedTextControl(
                # Callable to get formatted text tuples.
                self.get_center_text_tokens),
            align=WindowAlign.LEFT,
            dont_extend_width=False,
        )

        log_source_name = Window(
            content=FormattedTextControl(
                # Callable to get formatted text tuples.
                self.get_right_text_tokens),
            # Right side text should appear at the far right of the toolbar
            align=WindowAlign.RIGHT,
            dont_extend_width=True,
        )

        toolbar_vsplit = VSplit(
            [
                title_section_window,
                keybind_section_window,
                log_source_name,
            ],
            height=BottomToolbarBar.TOOLBAR_HEIGHT,
            style=functools.partial(pw_console.style.get_toolbar_style,
                                    log_pane),
        )

        # ConditionalContainer init()
        super().__init__(
            # Contents
            toolbar_vsplit,
            filter=Condition(lambda: log_pane.show_bottom_toolbar),
        )
