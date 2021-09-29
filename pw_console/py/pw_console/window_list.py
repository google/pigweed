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
"""WindowList"""

import collections
import functools
from enum import Enum
from typing import Any, Optional, TYPE_CHECKING

from prompt_toolkit.filters import has_focus
from prompt_toolkit.layout import (
    Dimension,
    HSplit,
    VSplit,
    FormattedTextControl,
    Window,
    WindowAlign,
    HorizontalAlign,
)

import pw_console.style
import pw_console.widgets.mouse_handlers

if TYPE_CHECKING:
    from pw_console.window_manager import WindowManager


class DisplayMode(Enum):
    """WindowList display modes."""
    STACK = 'Stacked'
    TABBED = 'Tabbed'


DEFAULT_DISPLAY_MODE = DisplayMode.STACK

# Weighted amount for adjusting window dimensions when enlarging and shrinking.
_WINDOW_SIZE_ADJUST = 2


class WindowList:
    """WindowList holds a stack of windows for the WindowManager."""
    def __init__(
        self,
        window_manager: 'WindowManager',
    ):
        self.window_manager = window_manager
        self.application = window_manager.application

        self.display_mode = DEFAULT_DISPLAY_MODE
        self.active_panes: collections.deque = collections.deque()
        self.focused_pane_index: Optional[int] = None

        self.height = Dimension(weight=50)
        self.width = Dimension(weight=50)

        # Reference to the current prompt_toolkit window split for the current
        # set of active_panes.
        self.container = None

    def get_tab_mode_active_pane(self):
        if self.focused_pane_index is None:
            self.focused_pane_index = 0

        pane = None
        try:
            pane = self.active_panes[self.focused_pane_index]
        except IndexError:
            # Ignore ValueError which can be raised by the self.active_panes
            # deque if existing_pane can't be found.
            self.focused_pane_index = 0
            pane = self.active_panes[self.focused_pane_index]
        return pane

    def get_current_active_pane(self):
        """Return the current active window pane."""
        focused_pane = None
        for index, pane in enumerate(self.active_panes):
            if has_focus(pane)():
                focused_pane = pane
                self.focused_pane_index = index
                break
        return focused_pane

    def get_pane_titles(self, omit_subtitles=False, use_menu_title=True):
        fragments = []
        separator = ('', ' ')
        fragments.append(separator)
        for pane_index, pane in enumerate(self.active_panes):
            title = pane.menu_title() if use_menu_title else pane.pane_title()
            subtitle = pane.pane_subtitle()
            text = f' {title} {subtitle} '
            if omit_subtitles:
                text = f' {title} '

            fragments.append((
                # Style
                ('class:window-tab-active' if pane_index
                 == self.focused_pane_index else 'class:window-tab-inactive'),
                # Text
                text,
                # Mouse handler
                functools.partial(
                    pw_console.widgets.mouse_handlers.on_click,
                    functools.partial(self.switch_to_tab, pane_index),
                ),
            ))
            fragments.append(separator)
        return fragments

    def switch_to_tab(self, index: int):
        self.focused_pane_index = index

        self.refresh_ui()
        self.application.focus_on_container(self.active_panes[index])

    def set_display_mode(self, mode: DisplayMode):
        self.display_mode = mode

        if self.display_mode == DisplayMode.TABBED:
            self.focused_pane_index = 0
            # Un-hide all panes, they must be visible to switch between tabs.
            for pane in self.active_panes:
                pane.show_pane = True

        self.application.focus_main_menu()
        self.refresh_ui()

    def refresh_ui(self):
        self.window_manager.update_root_container_body()
        # Update menu after the window manager rebuilds the root container.
        self.application.update_menu_items()

        if self.display_mode == DisplayMode.TABBED:
            self.application.focus_on_container(
                self.active_panes[self.focused_pane_index])

        self.application.redraw_ui()

    def update_container(self):
        """Re-create the window list split depending on the display mode."""
        if self.display_mode == DisplayMode.STACK:
            self.container = HSplit(
                list(pane for pane in self.active_panes if pane.show_pane),
                height=lambda: self.height,
                width=lambda: self.width,
            )

        elif self.display_mode == DisplayMode.TABBED:
            self.container = HSplit(
                [
                    self._create_window_tab_toolbar(),
                    self.get_tab_mode_active_pane(),
                ],
                height=lambda: self.height,
                width=lambda: self.width,
            )

    def _create_window_tab_toolbar(self):
        tab_bar_control = FormattedTextControl(
            functools.partial(self.get_pane_titles,
                              omit_subtitles=True,
                              use_menu_title=False))
        tab_bar_window = Window(content=tab_bar_control,
                                align=WindowAlign.LEFT,
                                dont_extend_width=True)

        spacer = Window(content=FormattedTextControl([('', '')]),
                        align=WindowAlign.LEFT,
                        dont_extend_width=False)

        tab_toolbar = VSplit(
            [
                tab_bar_window,
                spacer,
            ],
            style='class:toolbar_dim_inactive',
            height=1,
            align=HorizontalAlign.LEFT,
        )
        return tab_toolbar

    def empty(self) -> bool:
        return len(self.active_panes) == 0

    def pane_index(self, pane):
        pane_index = None
        try:
            pane_index = self.active_panes.index(pane)
        except ValueError:
            # Ignore ValueError which can be raised by the self.active_panes
            # deque if existing_pane can't be found.
            pass
        return pane_index

    def add_pane_no_checks(self, pane: Any, add_at_beginning=False):
        if add_at_beginning:
            self.active_panes.appendleft(pane)
        else:
            self.active_panes.append(pane)

    def add_pane(self, new_pane, existing_pane=None, add_at_beginning=False):
        existing_pane_index = self.pane_index(existing_pane)
        if existing_pane_index is not None:
            self.active_panes.insert(new_pane, existing_pane_index + 1)
        else:
            if add_at_beginning:
                self.active_panes.appendleft(new_pane)
            else:
                self.active_panes.append(new_pane)

        self.refresh_ui()

    def remove_pane_no_checks(self, pane: Any):
        try:
            self.active_panes.remove(pane)
        except ValueError:
            # ValueError will be raised if the the pane is not found
            pass
        return pane

    def remove_pane(self, existing_pane):
        existing_pane_index = self.pane_index(existing_pane)
        if existing_pane_index is None:
            return

        self.active_panes.remove(existing_pane)
        self.refresh_ui()

        # Set focus to the previous window pane
        if len(self.active_panes) > 0:
            existing_pane_index -= 1
            try:
                self.application.focus_on_container(
                    self.active_panes[existing_pane_index])
            except ValueError:
                # ValueError will be raised if the the pane at
                # existing_pane_index can't be accessed.
                # Focus on the main menu if the existing pane is hidden.
                self.application.focus_main_menu()

        self.application.redraw_ui()

    def enlarge_pane(self):
        """Enlarge the currently focused window pane."""
        pane = self.get_current_active_pane()
        if pane:
            self.adjust_pane_size(pane, _WINDOW_SIZE_ADJUST)

    def shrink_pane(self):
        """Shrink the currently focused window pane."""
        pane = self.get_current_active_pane()
        if pane:
            self.adjust_pane_size(pane, -_WINDOW_SIZE_ADJUST)

    def adjust_pane_size(self, pane, diff: int = _WINDOW_SIZE_ADJUST):
        """Increase or decrease a given pane's height."""
        # Placeholder next_pane value to allow setting width and height without
        # any consequences if there is no next visible pane.
        next_pane = HSplit([],
                           height=Dimension(weight=50),
                           width=Dimension(weight=50))  # type: ignore
        # Try to get the next visible pane to subtract a weight value from.
        next_visible_pane = self._get_next_visible_pane_after(pane)
        if next_visible_pane:
            next_pane = next_visible_pane

        # If the last pane is selected, and there are at least 2 panes, make
        # next_pane the previous pane.
        try:
            if len(self.active_panes) >= 2 and (self.active_panes.index(pane)
                                                == len(self.active_panes) - 1):
                next_pane = self.active_panes[-2]
        except ValueError:
            # Ignore ValueError raised if self.active_panes[-2] doesn't exist.
            pass

        old_weight = pane.height.weight
        next_old_weight = next_pane.height.weight  # type: ignore

        # Add to the current pane
        new_weight = old_weight + diff
        if new_weight <= 0:
            new_weight = old_weight

        # Subtract from the next pane
        next_new_weight = next_old_weight - diff
        if next_new_weight <= 0:
            next_new_weight = next_old_weight

        pane.height.weight = new_weight
        next_pane.height.weight = next_new_weight  # type: ignore

    def reset_pane_sizes(self):
        """Reset all active pane width and height to 50%"""
        for pane in self.active_panes:
            pane.height = Dimension(weight=50)
            pane.width = Dimension(weight=50)

    def move_pane_up(self):
        pane = self.get_current_active_pane()
        pane_index = self.pane_index(pane)
        if pane_index is None or pane_index <= 0:
            # Already at the beginning
            return

        # Swap with the previous pane
        previous_pane = self.active_panes[pane_index - 1]
        self.active_panes[pane_index - 1] = pane
        self.active_panes[pane_index] = previous_pane

        self.refresh_ui()

    def move_pane_down(self):
        pane = self.get_current_active_pane()
        pane_index = self.pane_index(pane)
        pane_count = len(self.active_panes)
        if pane_index is None or pane_index + 1 >= pane_count:
            # Already at the end
            return

        # Swap with the next pane
        next_pane = self.active_panes[pane_index + 1]
        self.active_panes[pane_index + 1] = pane
        self.active_panes[pane_index] = next_pane

        self.refresh_ui()

    def _get_next_visible_pane_after(self, target_pane):
        """Return the next visible pane that appears after the target pane."""
        try:
            target_pane_index = self.active_panes.index(target_pane)
        except ValueError:
            # If pane can't be found, focus on the main menu.
            return None

        # Loop through active panes (not including the target_pane).
        for i in range(1, len(self.active_panes)):
            next_pane_index = (target_pane_index + i) % len(self.active_panes)
            next_pane = self.active_panes[next_pane_index]
            if next_pane.show_pane:
                return next_pane
        return None

    def focus_next_visible_pane(self, pane):
        """Focus on the next visible window pane if possible."""
        next_visible_pane = self._get_next_visible_pane_after(pane)
        if next_visible_pane:
            self.application.layout.focus(next_visible_pane)
            return
        self.application.focus_main_menu()
