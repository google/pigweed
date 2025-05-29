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
import logging
import functools
import time
from typing import TYPE_CHECKING

from prompt_toolkit.data_structures import Point
from prompt_toolkit.filters import Condition, has_focus
from prompt_toolkit.layout import (
    ConditionalContainer,
    FormattedTextControl,
    VSplit,
    Window,
    WindowAlign,
    HorizontalAlign,
)
from prompt_toolkit.mouse_events import MouseEvent, MouseEventType, MouseButton

from pw_console.style import get_toolbar_style

if TYPE_CHECKING:
    from pw_console.log_pane import LogPane

_LOG = logging.getLogger(__package__)


class LineInfoBar(ConditionalContainer):
    """One line bar for showing current and total log lines."""

    def get_tokens(self):
        """Return formatted text tokens for display."""
        tokens = ' {} / {} '.format(
            self.log_pane.log_view.get_current_line() + 1,
            self.log_pane.log_view.get_total_count(),
        )
        return [('', tokens)]

    def __init__(self, log_pane: LogPane):
        self.log_pane = log_pane
        info_bar_control = FormattedTextControl(self.get_tokens)
        info_bar_window = Window(
            content=info_bar_control,
            align=WindowAlign.RIGHT,
            dont_extend_width=True,
        )

        super().__init__(
            VSplit(
                [info_bar_window],
                height=1,
                style=functools.partial(
                    get_toolbar_style, self.log_pane, dim=True
                ),
                align=HorizontalAlign.RIGHT,
            ),
            # Only show current/total line info if not auto-following
            # logs. Similar to tmux behavior.
            filter=Condition(lambda: not self.log_pane.log_view.follow),
        )


class TableHeaderControl(FormattedTextControl):
    """Table header prompt_toolkit UIControl."""

    def __init__(self, log_pane: LogPane, *args, **kwargs) -> None:
        self.log_pane = log_pane
        self.log_view = log_pane.log_view

        # Column resizing variables
        self.column_drag_start = False
        self.column_drag_stop = False
        self.column_drag_event_time: float = time.monotonic()
        super().__init__(*args, **kwargs)

    def mouse_handler(self, mouse_event: MouseEvent):
        """Mouse handler for this control."""
        mouse_position = mouse_event.position

        if (
            mouse_event.event_type == MouseEventType.MOUSE_DOWN
            and mouse_event.button == MouseButton.LEFT
        ):
            _LOG.debug('TableHeaderControl %s DOWN', mouse_position)

            if not has_focus(self.log_pane)():
                # Focus on the log pane content.
                self.log_pane.application.application.layout.focus(
                    self.log_pane
                )
                return None

            self.column_drag_start = True
            self.log_view.table.set_column_to_resize(mouse_position.x)

        if not has_focus(self.log_pane)():
            return NotImplemented

        # Left mouse button release should:
        # 1. check if a mouse drag just completed.
        if (
            mouse_event.event_type == MouseEventType.MOUSE_UP
            and mouse_event.button == MouseButton.LEFT
        ):
            # If a drag was in progress and this is the first mouse release
            # press, set the stop flag.
            if self.column_drag_start and not self.column_drag_stop:
                self.handle_mouse_up_event(mouse_position)

                # Mouse event handled, return None.
                return None

        if (
            mouse_event.event_type == MouseEventType.MOUSE_MOVE
            and mouse_event.button == MouseButton.LEFT
        ):
            _LOG.debug('TableHeaderControl %s MOVE', mouse_position)
            self.handle_mouse_drag_event(mouse_position)

            # Mouse event handled, return None.
            return None

        # Mouse event not handled, return NotImplemented.
        return NotImplemented

    def column_drag_in_progress(self) -> bool:
        return (
            self.column_drag_start
            and not self.column_drag_stop
            and (time.monotonic() < self.column_drag_event_time + 3.0)
        )

    def handle_mouse_up_event(self, _mouse_position: Point) -> None:
        _LOG.debug('TableHeaderControl %s UP', _mouse_position)
        self.column_drag_stop = True
        self.log_view.table.stop_column_resize()

    def handle_mouse_drag_event(self, mouse_position: Point) -> None:
        _LOG.debug('TableHeaderControl %s MOVE', mouse_position)

        # Drag in progress, set flags accordingly.
        self.column_drag_start = True
        self.column_drag_stop = False
        self.column_drag_event_time = time.monotonic()

        # self.log_view.table.set_column_to_resize(mouse_position.x)
        self.log_view.table.set_column_resize_amount(mouse_position.x)
        self.log_view.refresh_visible_table_columns()


class TableToolbar(ConditionalContainer):
    """One line toolbar for showing table headers."""

    TOOLBAR_HEIGHT = 1

    def __init__(self, log_pane: LogPane):
        # FormattedText of the table column headers.
        self.table_header_bar_control = TableHeaderControl(
            log_pane, log_pane.log_view.render_table_header
        )

        # Left justify the header content.
        table_header_bar_window = Window(
            content=self.table_header_bar_control,
            align=WindowAlign.LEFT,
            dont_extend_width=False,
        )
        super().__init__(
            VSplit(
                [table_header_bar_window],
                height=1,
                style=functools.partial(get_toolbar_style, log_pane, dim=True),
                align=HorizontalAlign.LEFT,
            ),
            filter=Condition(
                lambda: log_pane.table_view
                and log_pane.log_view.get_total_count() > 0
            ),
        )
