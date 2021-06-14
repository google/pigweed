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

from functools import partial
from typing import Any, List, Optional

from prompt_toolkit.filters import (
    Condition,
    has_focus,
)
from prompt_toolkit.layout import (
    ConditionalContainer,
    Dimension,
    Float,
    FloatContainer,
    FormattedTextControl,
    HSplit,
    VSplit,
    Window,
    WindowAlign,
    VerticalAlign,
)
from prompt_toolkit.layout.dimension import AnyDimension
from prompt_toolkit.mouse_events import MouseEvent, MouseEventType


class LogPaneLineInfoBar(ConditionalContainer):
    """One line bar for showing current and total log lines."""
    @staticmethod
    def get_tokens(unused_log_pane):
        """Return formatted text tokens for display."""
        tokens = ' Line {} / {} '.format(
            # TODO: Replace fake counts with the current line (1) and total
            # lines (10).
            1,
            10)
        return [('', tokens)]

    def __init__(self, log_pane):
        info_bar_control = FormattedTextControl(
            partial(LogPaneLineInfoBar.get_tokens, log_pane))
        info_bar_window = Window(content=info_bar_control,
                                 align=WindowAlign.RIGHT,
                                 dont_extend_width=True)

        super().__init__(
            VSplit([info_bar_window],
                   height=1,
                   style='class:bottom_toolbar',
                   align=WindowAlign.RIGHT),
            # Only show current/total line info if not auto-following
            # logs. Similar to tmux behavior.
            filter=Condition(
                lambda: True
                # TODO: replace True with log follow status.
                # not log_pane.log_container.follow
            ))


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
    def get_center_text_tokens(log_pane):
        """Return formatted text tokens for display in the center part of the
        toolbar."""

        # Create mouse handler functions.
        focus = partial(LogPaneBottomToolbarBar.mouse_handler_focus, log_pane)
        toggle_wrap_lines = partial(
            LogPaneBottomToolbarBar.mouse_handler_toggle_wrap_lines, log_pane)
        clear_history = partial(
            LogPaneBottomToolbarBar.mouse_handler_clear_history, log_pane)
        toggle_follow = partial(
            LogPaneBottomToolbarBar.mouse_handler_toggle_follow, log_pane)

        # FormattedTextTuple contents: (Style, Text, Mouse handler)
        separator_text = ('', ' ')  # 1 space of separaton between keybinds.

        # If the log_pane is in focus, show keybinds in the toolbar.
        if has_focus(log_pane.__pt_container__())():
            return [
                ('', ' [FOCUSED]'),
                separator_text,
                # TODO: Indicate toggle status with a checkbox?
                ('class:keybind', 'w', toggle_wrap_lines),
                ('class:keyhelp', ':Wrap', toggle_wrap_lines),
                separator_text,
                ('class:keybind', 'f', toggle_follow),
                ('class:keyhelp', ':Follow', toggle_follow),
                separator_text,
                ('class:keybind', 'C', clear_history),
                ('class:keyhelp', ':Clear', clear_history),
            ]
        # Show the click to focus button if log pane isn't in focus.
        return [
            ('class:keyhelp', ' [click to focus] ', focus),
        ]

    def __init__(self, log_pane):
        title_section_text = FormattedTextControl([(
            # Style
            'class:logo',
            # Text
            ' Logs ',
            # Mouse handler
            partial(LogPaneBottomToolbarBar.mouse_handler_focus, log_pane),
        )])

        keybind_section_text = FormattedTextControl(
            # Callable to get formatted text tuples.
            partial(LogPaneBottomToolbarBar.get_center_text_tokens, log_pane))

        title_section_window = Window(
            content=title_section_text,
            align=WindowAlign.LEFT,
            dont_extend_width=True,
        )

        keybind_section_window = Window(
            content=keybind_section_text,
            align=WindowAlign.LEFT,
            dont_extend_width=False,
        )

        toolbar_vsplit = VSplit(
            [
                title_section_window,
                keybind_section_window,
            ],
            height=LogPaneBottomToolbarBar.TOOLBAR_HEIGHT,
            style='class:bottom_toolbar',
            align=WindowAlign.LEFT,
        )

        # ConditionalContainer init()
        super().__init__(
            # Contents
            toolbar_vsplit,
            filter=Condition(lambda: log_pane.show_bottom_toolbar),
        )


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
            show_bottom_toolbar=True,
            show_line_info=True,
            wrap_lines=True,
            # Default width and height to 50% of the screen
            height: Optional[AnyDimension] = Dimension(weight=50),
            width: Optional[AnyDimension] = Dimension(weight=50),
    ):
        self.application = application
        self.show_bottom_toolbar = show_bottom_toolbar
        self.show_line_info = show_line_info
        self.wrap_lines = wrap_lines
        self.height = height
        self.width = width

        # Log pane size variables. These are updated just befor rendering the
        # pane by the LogLineHSplit class.
        self.current_log_pane_width = 0
        self.current_log_pane_height = 0
        self.last_log_pane_width = 0
        self.last_log_pane_height = 0

        # Create the bottom toolbar for the whole log pane.
        self.bottom_toolbar = LogPaneBottomToolbarBar(self)

        # TODO: Render logs here
        self.log_content_control = FormattedTextControl(
            [('', 'Logs appear here')],
            focusable=True,
        )

        self.log_display_window = Window(
            content=self.log_content_control,
            allow_scroll_beyond_bottom=True,
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
        )

        # Root level container
        self.container = FloatContainer(
            # Horizonal split containing the log lines and the toolbar.
            LogLineHSplit(
                self,  # LogPane reference
                [
                    self.log_display_window,
                    self.bottom_toolbar,
                ],
                # Align content with the bottom of the container.
                align=VerticalAlign.BOTTOM,
                height=self.height,
                width=self.width,
            ),
            floats=[
                # Floating LogPaneLineInfoBar
                Float(top=0,
                      right=0,
                      height=1,
                      content=LogPaneLineInfoBar(self)),
            ])

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

    def toggle_wrap_lines(self):
        """Toggle line wraping/truncation."""
        self.wrap_lines = not self.wrap_lines
        self.redraw_ui()

    def toggle_follow(self):
        """Toggle following log lines."""
        # TODO: self.log_container.toggle_follow()
        self.redraw_ui()

    def clear_history(self):
        """Erase stored log lines."""
        # TODO: self.log_container.clear_logs()
        self.redraw_ui()

    def __pt_container__(self):
        """Return the prompt_toolkit root container for this log pane."""
        return self.container

    # pylint: disable=no-self-use
    def get_all_key_bindings(self) -> List:
        """Return all keybinds for this pane."""
        # TODO: return log content control keybindings
        return []

    def after_render_hook(self):
        """Run tasks after the last UI render."""
