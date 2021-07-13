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
"""LogPane class."""

from __future__ import annotations
import functools
from typing import TYPE_CHECKING

from prompt_toolkit.buffer import Buffer
from prompt_toolkit.filters import (
    Condition,
    has_focus,
)
from prompt_toolkit.key_binding import KeyBindings, KeyPressEvent
from prompt_toolkit.layout import (
    ConditionalContainer,
    FormattedTextControl,
    VSplit,
    Window,
    WindowAlign,
    HorizontalAlign,
)
from prompt_toolkit.mouse_events import MouseEvent, MouseEventType
from prompt_toolkit.widgets import TextArea
from prompt_toolkit.data_structures import Point

import pw_console.widgets.checkbox
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
                   style='class:toolbar_active',
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
        super().__init__(VSplit([table_header_bar_window],
                                height=1,
                                style=functools.partial(
                                    pw_console.style.get_toolbar_style,
                                    log_pane,
                                    dim=True),
                                align=HorizontalAlign.LEFT),
                         filter=Condition(lambda: log_pane.table_view))


class BottomToolbarBar(ConditionalContainer):
    """One line toolbar for display at the bottom of the LogPane."""
    TOOLBAR_HEIGHT = 1

    @staticmethod
    def mouse_handler_focus(log_pane: 'LogPane', mouse_event: MouseEvent):
        """Focus this pane on click."""
        if mouse_event.event_type == MouseEventType.MOUSE_UP:
            log_pane.application.application.layout.focus(log_pane)
            return None
        return NotImplemented

    @staticmethod
    def mouse_handler_toggle_table_view(log_pane: 'LogPane',
                                        mouse_event: MouseEvent):
        """Toggle table view on click."""
        if mouse_event.event_type == MouseEventType.MOUSE_UP:
            log_pane.toggle_table_view()
            return None
        return NotImplemented

    @staticmethod
    def mouse_handler_toggle_wrap_lines(log_pane: 'LogPane',
                                        mouse_event: MouseEvent):
        """Toggle wrap lines on click."""
        if mouse_event.event_type == MouseEventType.MOUSE_UP:
            log_pane.toggle_wrap_lines()
            return None
        return NotImplemented

    @staticmethod
    def mouse_handler_clear_history(log_pane: 'LogPane',
                                    mouse_event: MouseEvent):
        """Clear history on click."""
        if mouse_event.event_type == MouseEventType.MOUSE_UP:
            log_pane.clear_history()
            return None
        return NotImplemented

    @staticmethod
    def mouse_handler_toggle_follow(log_pane: 'LogPane',
                                    mouse_event: MouseEvent):
        """Toggle follow on click."""
        if mouse_event.event_type == MouseEventType.MOUSE_UP:
            log_pane.toggle_follow()
            return None
        return NotImplemented

    @staticmethod
    def get_left_text_tokens(log_pane: 'LogPane'):
        """Return toolbar indicator and title."""

        title = ' {} '.format(log_pane.pane_title())
        mouse_handler = functools.partial(BottomToolbarBar.mouse_handler_focus,
                                          log_pane)
        return pw_console.style.get_pane_indicator(log_pane, title,
                                                   mouse_handler)

    @staticmethod
    def get_center_text_tokens(log_pane: 'LogPane'):
        """Return formatted text tokens for display in the center part of the
        toolbar."""

        # Create mouse handler functions.
        focus = functools.partial(BottomToolbarBar.mouse_handler_focus,
                                  log_pane)
        toggle_wrap_lines = functools.partial(
            BottomToolbarBar.mouse_handler_toggle_wrap_lines, log_pane)
        clear_history = functools.partial(
            BottomToolbarBar.mouse_handler_clear_history, log_pane)
        toggle_follow = functools.partial(
            BottomToolbarBar.mouse_handler_toggle_follow, log_pane)
        toggle_table_view = functools.partial(
            BottomToolbarBar.mouse_handler_toggle_table_view, log_pane)

        # FormattedTextTuple contents: (Style, Text, Mouse handler)
        separator_text = ('', '  ')  # 2 spaces of separaton between keybinds.

        return [
            separator_text,
            pw_console.widgets.checkbox.to_checkbox(log_pane.table_view,
                                                    toggle_table_view),
            ('class:keybind', 't', toggle_table_view),
            ('class:keyhelp', ': Table', toggle_table_view),
            separator_text,
            pw_console.widgets.checkbox.to_checkbox(log_pane.wrap_lines,
                                                    toggle_wrap_lines),
            ('class:keybind', 'w', toggle_wrap_lines),
            ('class:keyhelp', ': Wrap', toggle_wrap_lines),
            separator_text,
            pw_console.widgets.checkbox.to_checkbox(log_pane.log_view.follow,
                                                    toggle_follow),
            ('class:keybind', 'f', toggle_follow),
            ('class:keyhelp', ': Follow', toggle_follow),
            separator_text,
            ('class:keybind', 'C', clear_history),
            ('class:keyhelp', ': Clear', clear_history),

            # Remaining whitespace should focus on click.
            ('class:keybind', ' ', focus),
        ]

    @staticmethod
    def get_right_text_tokens(log_pane: 'LogPane'):
        """Return formatted text tokens for display."""
        focus = functools.partial(BottomToolbarBar.mouse_handler_focus,
                                  log_pane)
        fragments = []
        if not has_focus(log_pane.__pt_container__())():
            fragments.append(('class:keyhelp', '[click to focus] ', focus))
        fragments.append(('', ' {} '.format(log_pane.pane_subtitle()), focus))
        return fragments

    def __init__(self, log_pane: 'LogPane'):
        title_section_window = Window(
            content=FormattedTextControl(
                # Callable to get formatted text tuples.
                functools.partial(BottomToolbarBar.get_left_text_tokens,
                                  log_pane)),
            align=WindowAlign.LEFT,
            dont_extend_width=True,
        )

        keybind_section_window = Window(
            content=FormattedTextControl(
                # Callable to get formatted text tuples.
                functools.partial(BottomToolbarBar.get_center_text_tokens,
                                  log_pane)),
            align=WindowAlign.LEFT,
            dont_extend_width=False,
        )

        log_source_name = Window(
            content=FormattedTextControl(
                # Callable to get formatted text tuples.
                functools.partial(BottomToolbarBar.get_right_text_tokens,
                                  log_pane)),
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


class SearchToolbar(ConditionalContainer):
    """One line toolbar for entering search text."""

    TOOLBAR_HEIGHT = 1

    def close_search_bar(self):
        """Close search bar."""
        self.log_pane.search_bar_active = False
        # Focus on the log_pane.
        self.log_pane.application.focus_on_container(self.log_pane)
        self.log_pane.redraw_ui()

    def __init__(self, log_pane: 'LogPane'):
        self.log_pane = log_pane

        # FormattedText of the search column headers.
        self.input_field = TextArea(
            prompt=[('class:logo', '/')],
            focusable=True,
            scrollbar=False,
            multiline=False,
            height=1,
            dont_extend_height=True,
            dont_extend_width=False,
            accept_handler=self._search_accept_handler,
        )

        # Additional keybindings for the text area.
        key_bindings = KeyBindings()

        @key_bindings.add('escape')
        @key_bindings.add('c-c')
        @key_bindings.add('c-d')
        @key_bindings.add('c-g')
        def _close_search_bar(_event: KeyPressEvent) -> None:
            """Close search bar."""
            self.close_search_bar()

        @key_bindings.add('c-f')
        def _apply_filter(_event: KeyPressEvent) -> None:
            """Apply search as a filter."""
            self.log_pane.apply_filter()

        self.input_field.control.key_bindings = key_bindings

        super().__init__(VSplit([self.input_field],
                                height=1,
                                style=functools.partial(
                                    pw_console.style.get_toolbar_style,
                                    log_pane),
                                align=HorizontalAlign.LEFT),
                         filter=Condition(lambda: log_pane.search_bar_active))

    def _search_accept_handler(self, buff: Buffer) -> bool:
        """Function run when hitting Enter in the search bar."""
        # Always close the search bar.
        self.close_search_bar()

        if len(buff.text) == 0:
            # Don't apply an empty search.
            return False

        self.log_pane.apply_search(buff.text)
        # Erase existing search text.
        return False
