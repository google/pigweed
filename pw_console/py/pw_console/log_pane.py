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

import functools
from typing import Any, List, Optional

from prompt_toolkit.application.current import get_app
from prompt_toolkit.filters import (
    Condition,
    has_focus,
)
from prompt_toolkit.formatted_text import to_formatted_text
from prompt_toolkit.key_binding import KeyBindings, KeyPressEvent
from prompt_toolkit.layout import (
    ConditionalContainer,
    Dimension,
    Float,
    FloatContainer,
    FormattedTextControl,
    HSplit,
    ScrollOffsets,
    UIContent,
    VSplit,
    VerticalAlign,
    Window,
    WindowAlign,
)
from prompt_toolkit.layout.dimension import AnyDimension
from prompt_toolkit.mouse_events import MouseEvent, MouseEventType

import pw_console.widgets.checkbox
import pw_console.style
from pw_console.log_container import LogContainer


class LogPaneLineInfoBar(ConditionalContainer):
    """One line bar for showing current and total log lines."""
    @staticmethod
    def get_tokens(log_pane):
        """Return formatted text tokens for display."""
        tokens = ' Line {} / {} '.format(
            log_pane.log_container.get_current_line() + 1,
            log_pane.log_container.get_total_count(),
        )
        return [('', tokens)]

    def __init__(self, log_pane):
        info_bar_control = FormattedTextControl(
            functools.partial(LogPaneLineInfoBar.get_tokens, log_pane))
        info_bar_window = Window(content=info_bar_control,
                                 align=WindowAlign.RIGHT,
                                 dont_extend_width=True)

        super().__init__(
            VSplit([info_bar_window],
                   height=1,
                   style='class:toolbar_active',
                   align=WindowAlign.RIGHT),
            # Only show current/total line info if not auto-following
            # logs. Similar to tmux behavior.
            filter=Condition(lambda: not log_pane.log_container.follow))


class LogPaneTableToolbar(ConditionalContainer):
    """One line toolbar for showing table headers."""
    def __init__(self, log_pane):
        # FormattedText of the table column headers.
        table_header_bar_control = FormattedTextControl(
            log_pane.log_container.render_table_header)
        # Left justify the header content.
        table_header_bar_window = Window(content=table_header_bar_control,
                                         align=WindowAlign.LEFT,
                                         dont_extend_width=True)
        super().__init__(VSplit([table_header_bar_window],
                                height=1,
                                style=functools.partial(
                                    pw_console.style.get_toolbar_style,
                                    log_pane),
                                align=WindowAlign.LEFT),
                         filter=Condition(lambda: log_pane.table_view))


class LogPaneBottomToolbarBar(ConditionalContainer):
    """One line toolbar for display at the bottom of the LogPane."""
    TOOLBAR_HEIGHT = 1

    @staticmethod
    def mouse_handler_focus(log_pane, mouse_event: MouseEvent):
        """Focus this pane on click."""
        if mouse_event.event_type == MouseEventType.MOUSE_UP:
            log_pane.application.application.layout.focus(log_pane)
            return None
        return NotImplemented

    @staticmethod
    def mouse_handler_toggle_table_view(log_pane, mouse_event: MouseEvent):
        """Toggle table view on click."""
        if mouse_event.event_type == MouseEventType.MOUSE_UP:
            log_pane.toggle_table_view()
            return None
        return NotImplemented

    @staticmethod
    def mouse_handler_toggle_wrap_lines(log_pane, mouse_event: MouseEvent):
        """Toggle wrap lines on click."""
        if mouse_event.event_type == MouseEventType.MOUSE_UP:
            log_pane.toggle_wrap_lines()
            return None
        return NotImplemented

    @staticmethod
    def mouse_handler_clear_history(log_pane, mouse_event: MouseEvent):
        """Clear history on click."""
        if mouse_event.event_type == MouseEventType.MOUSE_UP:
            log_pane.clear_history()
            return None
        return NotImplemented

    @staticmethod
    def mouse_handler_toggle_follow(log_pane, mouse_event: MouseEvent):
        """Toggle follow on click."""
        if mouse_event.event_type == MouseEventType.MOUSE_UP:
            log_pane.toggle_follow()
            return None
        return NotImplemented

    @staticmethod
    def get_left_text_tokens(log_pane):
        """Return toolbar indicator and title."""

        title = ' {} '.format(log_pane.pane_title())
        mouse_handler = functools.partial(
            LogPaneBottomToolbarBar.mouse_handler_focus, log_pane)
        return pw_console.style.get_pane_indicator(log_pane, title,
                                                   mouse_handler)

    @staticmethod
    def get_center_text_tokens(log_pane):
        """Return formatted text tokens for display in the center part of the
        toolbar."""

        # Create mouse handler functions.
        focus = functools.partial(LogPaneBottomToolbarBar.mouse_handler_focus,
                                  log_pane)
        toggle_wrap_lines = functools.partial(
            LogPaneBottomToolbarBar.mouse_handler_toggle_wrap_lines, log_pane)
        clear_history = functools.partial(
            LogPaneBottomToolbarBar.mouse_handler_clear_history, log_pane)
        toggle_follow = functools.partial(
            LogPaneBottomToolbarBar.mouse_handler_toggle_follow, log_pane)
        toggle_table_view = functools.partial(
            LogPaneBottomToolbarBar.mouse_handler_toggle_table_view, log_pane)

        # FormattedTextTuple contents: (Style, Text, Mouse handler)
        separator_text = ('', '  ')  # 2 spaces of separaton between keybinds.

        return [
            separator_text,
            pw_console.widgets.checkbox.to_checkbox(log_pane.table_view,
                                                    toggle_table_view,
                                                    end=''),
            ('class:keyhelp', 'Table:', toggle_table_view),
            ('class:keybind', 't', toggle_table_view),
            separator_text,
            pw_console.widgets.checkbox.to_checkbox(log_pane.wrap_lines,
                                                    toggle_wrap_lines,
                                                    end=''),
            ('class:keyhelp', 'Wrap:', toggle_wrap_lines),
            ('class:keybind', 'w', toggle_wrap_lines),
            separator_text,
            pw_console.widgets.checkbox.to_checkbox(
                log_pane.log_container.follow, toggle_follow, end=''),
            ('class:keyhelp', 'Follow:', toggle_follow),
            ('class:keybind', 'f', toggle_follow),
            separator_text,
            ('class:keyhelp', 'Clear:', clear_history),
            ('class:keybind', 'C', clear_history),
            # Remaining whitespace should focus on click.
            ('class:keybind', ' ', focus),
        ]

    @staticmethod
    def get_right_text_tokens(log_pane):
        """Return formatted text tokens for display."""
        focus = functools.partial(LogPaneBottomToolbarBar.mouse_handler_focus,
                                  log_pane)
        fragments = []
        if not has_focus(log_pane.__pt_container__())():
            fragments.append(('class:keyhelp', '[click to focus] ', focus))
        fragments.append(('', ' {} '.format(log_pane.pane_subtitle())))
        return fragments

    def __init__(self, log_pane):
        title_section_window = Window(
            content=FormattedTextControl(
                # Callable to get formatted text tuples.
                functools.partial(LogPaneBottomToolbarBar.get_left_text_tokens,
                                  log_pane)),
            align=WindowAlign.LEFT,
            dont_extend_width=True,
        )

        keybind_section_window = Window(
            content=FormattedTextControl(
                # Callable to get formatted text tuples.
                functools.partial(
                    LogPaneBottomToolbarBar.get_center_text_tokens, log_pane)),
            align=WindowAlign.LEFT,
            dont_extend_width=False,
        )

        log_source_name = Window(
            content=FormattedTextControl(
                # Callable to get formatted text tuples.
                functools.partial(
                    LogPaneBottomToolbarBar.get_right_text_tokens, log_pane)),
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
            height=LogPaneBottomToolbarBar.TOOLBAR_HEIGHT,
            style=functools.partial(pw_console.style.get_toolbar_style,
                                    log_pane),
            align=WindowAlign.LEFT,
        )

        # ConditionalContainer init()
        super().__init__(
            # Contents
            toolbar_vsplit,
            filter=Condition(lambda: log_pane.show_bottom_toolbar),
        )


class LogContentControl(FormattedTextControl):
    """LogPane prompt_toolkit UIControl for displaying LogContainer lines."""
    @staticmethod
    def indent_wrapped_pw_log_format_line(log_pane, line_number, wrap_count):
        """Indent wrapped lines to match pw_cli timestamp & level formatter."""
        prefix_width = log_pane.log_container.get_line_wrap_prefix_width()

        # Return no prefix string if no wrapping is required. If the current log
        # window is smaller than the prefix width then don't indent when
        # wrapping lines.
        if wrap_count == 0 or log_pane.current_log_pane_width <= prefix_width:
            return None

        prefix_string = ' ' * prefix_width

        # If this line matches the selected log line, highlight it.
        if line_number == log_pane.log_container.get_cursor_position().y:
            return to_formatted_text(prefix_string,
                                     style='class:selected-log-line')

        return prefix_string

    def create_content(self, width: int, height: Optional[int]) -> UIContent:
        # Save redered height
        if height:
            self.log_pane.last_log_content_height += height
        return super().create_content(width, height)

    def __init__(self, log_pane, *args, **kwargs):
        self.log_pane = log_pane

        # Key bindings.
        key_bindings = KeyBindings()

        @key_bindings.add('w')
        def _toggle_wrap_lines(_event: KeyPressEvent) -> None:
            """Toggle log line wrapping."""
            self.log_pane.toggle_wrap_lines()

        @key_bindings.add('t')
        def _toggle_table_view(_event: KeyPressEvent) -> None:
            """Toggle table view."""
            self.log_pane.toggle_table_view()

        @key_bindings.add('C')
        def _clear_history(_event: KeyPressEvent) -> None:
            """Clear log pane history."""
            self.log_pane.clear_history()

        @key_bindings.add('g')
        def _scroll_to_top(_event: KeyPressEvent) -> None:
            """Scroll to top."""
            self.log_pane.log_container.scroll_to_top()

        @key_bindings.add('G')
        def _scroll_to_bottom(_event: KeyPressEvent) -> None:
            """Scroll to bottom."""
            self.log_pane.log_container.scroll_to_bottom()

        @key_bindings.add('f')
        def _toggle_follow(_event: KeyPressEvent) -> None:
            """Toggle log line following."""
            self.log_pane.toggle_follow()

        @key_bindings.add('up')
        @key_bindings.add('k')
        def _up(_event: KeyPressEvent) -> None:
            """Select previous log line."""
            self.log_pane.log_container.scroll_up()

        @key_bindings.add('down')
        @key_bindings.add('j')
        def _down(_event: KeyPressEvent) -> None:
            """Select next log line."""
            self.log_pane.log_container.scroll_down()

        @key_bindings.add('pageup')
        def _pageup(_event: KeyPressEvent) -> None:
            """Scroll the logs up by one page."""
            self.log_pane.log_container.scroll_up_one_page()

        @key_bindings.add('pagedown')
        def _pagedown(_event: KeyPressEvent) -> None:
            """Scroll the logs down by one page."""
            self.log_pane.log_container.scroll_down_one_page()

        super().__init__(*args, key_bindings=key_bindings, **kwargs)

    def mouse_handler(self, mouse_event: MouseEvent):
        """Mouse handler for this control."""
        mouse_position = mouse_event.position

        # Check for pane focus first.
        # If not in focus, change forus to the log pane and do nothing else.
        if not has_focus(self)():
            if mouse_event.event_type == MouseEventType.MOUSE_UP:
                # Focus buffer when clicked.
                get_app().layout.focus(self)
                # Mouse event handled, return None.
                return None

        if mouse_event.event_type == MouseEventType.MOUSE_UP:
            # Scroll to the line clicked.
            self.log_pane.log_container.scroll_to_position(mouse_position)
            # Mouse event handled, return None.
            return None

        if mouse_event.event_type == MouseEventType.SCROLL_DOWN:
            self.log_pane.log_container.scroll_down()
            # Mouse event handled, return None.
            return None

        if mouse_event.event_type == MouseEventType.SCROLL_UP:
            self.log_pane.log_container.scroll_up()
            # Mouse event handled, return None.
            return None

        # Mouse event not handled, return NotImplemented.
        return NotImplemented


class LogLineHSplit(HSplit):
    """PromptToolkit HSplit class with a write_to_screen function that saves the
    width and height of the container to be rendered.
    """
    def __init__(self, log_pane, *args, **kwargs):
        # Save a reference to the parent LogPane.
        self.log_pane = log_pane
        super().__init__(*args, **kwargs)

    def write_to_screen(
        self,
        screen,
        mouse_handlers,
        write_position,
        parent_style: str,
        erase_bg: bool,
        z_index: Optional[int],
    ) -> None:
        # Save the width and height for the current render pass. This will be
        # used by the log pane to render the correct amount of log lines.
        self.log_pane.update_log_pane_size(write_position.width,
                                           write_position.height)
        # Continue writing content to the screen.
        super().write_to_screen(screen, mouse_handlers, write_position,
                                parent_style, erase_bg, z_index)


class LogPane:
    """LogPane class."""

    # pylint: disable=too-many-instance-attributes
    def __init__(
        self,
        application: Any,
        pane_title: Optional[str] = None,
        # Default width and height to 50% of the screen
        height: Optional[AnyDimension] = None,
        width: Optional[AnyDimension] = None,
    ):
        self.application = application
        self.show_bottom_toolbar = True
        # TODO(tonymd): Read these settings from a project (or user) config.
        self.wrap_lines = False
        self.table_view = True
        self.height = height if height else Dimension(weight=50)
        self.width = width if width else Dimension(weight=50)
        self.show_pane = True
        self._pane_title = pane_title
        self._pane_subtitle = None

        # Create the log container which stores and handles incoming logs.
        self.log_container = LogContainer()
        self.log_container.set_log_pane(self)
        self.log_container.set_formatting()

        # Log pane size variables. These are updated just befor rendering the
        # pane by the LogLineHSplit class.
        self.current_log_pane_width = 0
        self.current_log_pane_height = 0
        self.last_log_pane_width = 0
        self.last_log_pane_height = 0
        self.last_log_content_height = 0

        # Create the bottom toolbar for the whole log pane.
        self.bottom_toolbar = LogPaneBottomToolbarBar(self)

        self.table_header_toolbar = LogPaneTableToolbar(self)

        self.log_content_control = LogContentControl(
            self,  # parent LogPane
            # FormattedTextControl args:
            self.log_container.render_content,
            # Hide the cursor, use cursorline=True in self.log_display_window to
            # indicate currently selected line.
            show_cursor=False,
            focusable=True,
            get_cursor_position=self.log_content_control_get_cursor_position,
        )

        self.log_display_window = Window(
            content=self.log_content_control,
            # TODO(tonymd): ScrollOffsets here causes jumpiness when lines are
            # wrapped.
            scroll_offsets=ScrollOffsets(top=0, bottom=0),
            allow_scroll_beyond_bottom=True,
            get_line_prefix=functools.partial(
                LogContentControl.indent_wrapped_pw_log_format_line, self),
            wrap_lines=Condition(lambda: self.wrap_lines),
            cursorline=False,
            # Don't make the window taller to fill the parent split container.
            # Window should match the height of the log line content. This will
            # also allow the parent HSplit to justify the content to the bottom
            dont_extend_height=True,
            # Window width should be extended to make backround highlighting
            # extend to the end of the container. Otherwise backround colors
            # will only appear until the end of the log line.
            dont_extend_width=False,
            # Needed for log lines ANSI sequences that don't specify foreground
            # or background colors.
            style=functools.partial(pw_console.style.get_pane_style, self),
        )

        # Root level container
        self.container = ConditionalContainer(
            FloatContainer(
                # Horizonal split containing the log lines and the toolbar.
                LogLineHSplit(
                    self,  # LogPane reference
                    [
                        self.table_header_toolbar,
                        self.log_display_window,
                        self.bottom_toolbar,
                    ],
                    # Align content with the bottom of the container.
                    align=VerticalAlign.BOTTOM,
                    height=lambda: self.height,
                    width=lambda: self.width,
                    style=functools.partial(pw_console.style.get_pane_style,
                                            self),
                ),
                floats=[
                    # Floating LogPaneLineInfoBar
                    Float(top=0,
                          right=0,
                          height=1,
                          content=LogPaneLineInfoBar(self)),
                ]),
            filter=Condition(lambda: self.show_pane))

    def pane_title(self):
        title = self._pane_title
        if not title:
            title = 'Logs'
        return title

    def append_pane_subtitle(self, text):
        if not self._pane_subtitle:
            self._pane_subtitle = text
        else:
            self._pane_subtitle = self._pane_subtitle + ', ' + text

    def pane_subtitle(self):
        if not self._pane_subtitle:
            return ', '.join(self.log_container.channel_counts.keys())
        logger_names = self._pane_subtitle.split(', ')
        additional_text = ''
        if len(logger_names) > 1:
            additional_text = ' + {} more'.format(len(logger_names))

        return logger_names[0] + additional_text

    def update_log_pane_size(self, width, height):
        """Save width and height of the log pane for the current UI render
        pass."""
        if width:
            self.last_log_pane_width = self.current_log_pane_width
            self.current_log_pane_width = width
        if height:
            # Subtract the height of the LogPaneBottomToolbarBar
            height -= LogPaneBottomToolbarBar.TOOLBAR_HEIGHT
            self.last_log_pane_height = self.current_log_pane_height
            self.current_log_pane_height = height

    def redraw_ui(self):
        """Trigger a prompt_toolkit UI redraw."""
        self.application.redraw_ui()

    def toggle_table_view(self):
        """Enable or disable table view."""
        self.table_view = not self.table_view
        self.redraw_ui()

    def toggle_wrap_lines(self):
        """Enable or disable line wraping/truncation."""
        self.wrap_lines = not self.wrap_lines
        self.redraw_ui()

    def toggle_follow(self):
        """Enable or disable following log lines."""
        self.log_container.toggle_follow()
        self.redraw_ui()

    def clear_history(self):
        """Erase stored log lines."""
        self.log_container.clear_logs()
        self.redraw_ui()

    def __pt_container__(self):
        """Return the prompt_toolkit root container for this log pane."""
        return self.container

    # pylint: disable=no-self-use
    def get_all_key_bindings(self) -> List:
        """Return all keybinds for this pane."""
        # Return log content control keybindings
        return [self.log_content_control.get_key_bindings()]

    def after_render_hook(self):
        """Run tasks after the last UI render."""
        self.reset_log_content_height()

    def reset_log_content_height(self):
        """Reset log line pane content height."""
        self.last_log_content_height = 0

    def log_content_control_get_cursor_position(self):
        return self.log_container.get_cursor_position()
