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
"""WindowManager"""

import collections
from typing import Any

from prompt_toolkit.filters import has_focus
from prompt_toolkit.layout import (
    Dimension,
    HSplit,
    VSplit,
)

# Weighted amount for adjusting window dimensions when enlarging and shrinking.
_WINDOW_SIZE_ADJUST = 2


class WindowManager:
    """WindowManager class

    This class handles adding/removing/resizing windows and rendering the
    prompt_toolkit split layout."""
    def __init__(
        self,
        application: Any,
    ):
        self.application = application
        self.active_panes: collections.deque = collections.deque()
        self.vertical_split = False

        # Reference to the current prompt_toolkit window split for the current
        # set of active_panes.
        self.active_pane_split = None

    def add_window(self, pane: Any):
        self.active_panes.append(pane)

    def active_split(self):
        return self.active_panes

    def _get_current_active_pane(self):
        """Return the current active window pane."""
        focused_pane = None
        for pane in self.active_panes:
            if has_focus(pane)():
                focused_pane = pane
                break
        return focused_pane

    def toggle_vertical_split(self):
        """Toggle visibility of the help window."""
        self.vertical_split = not self.vertical_split

        self.application.update_menu_items()
        self.update_root_container_body()

        self.application.redraw_ui()

    def _update_split_orientation(self):
        if self.vertical_split:
            self.active_pane_split = VSplit(
                list(pane for pane in self.active_panes if pane.show_pane),
                # Add a vertical separator between each active window pane.
                padding=1,
                padding_char='│',
                padding_style='class:pane_separator',
            )
        else:
            self.active_pane_split = HSplit(self.active_panes)

    def create_root_split(self):
        """Create a vertical or horizontal split container for all active
        panes."""
        self._update_split_orientation()
        return HSplit([
            self.active_pane_split,
        ])

    def update_root_container_body(self):
        # Replace the root MenuContainer body with the new split.
        self.application.root_container.container.content.children[
            1] = self.create_root_split()

    def _pane_index(self, pane):
        pane_index = None
        try:
            pane_index = self.active_panes.index(pane)
        except ValueError:
            # Ignore ValueError which can be raised by the self.active_panes
            # deque if existing_pane can't be found.
            pass
        return pane_index

    def add_pane(self, new_pane, existing_pane=None, add_at_beginning=False):
        existing_pane_index = self._pane_index(existing_pane)
        if existing_pane_index:
            self.active_panes.insert(new_pane, existing_pane_index + 1)
        else:
            if add_at_beginning:
                self.active_panes.appendleft(new_pane)
            else:
                self.active_panes.append(new_pane)

        self.application.update_menu_items()
        self.update_root_container_body()

        self.application.redraw_ui()

    def remove_pane(self, existing_pane):
        existing_pane_index = self._pane_index(existing_pane)
        if not existing_pane_index:
            return

        self.active_panes.remove(existing_pane)
        self.application.update_menu_items()
        self.update_root_container_body()

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
        pane = self._get_current_active_pane()
        if pane:
            self.adjust_pane_size(pane, _WINDOW_SIZE_ADJUST)

    def shrink_pane(self):
        """Shrink the currently focused window pane."""
        pane = self._get_current_active_pane()
        if pane:
            self.adjust_pane_size(pane, -_WINDOW_SIZE_ADJUST)

    def adjust_pane_size(self, pane, diff: int = _WINDOW_SIZE_ADJUST):
        """Increase or decrease a given pane's width or height weight."""
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

        # Get current weight values
        if self.vertical_split:
            old_weight = pane.width.weight
            next_old_weight = next_pane.width.weight  # type: ignore
        else:  # Horizontal split
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

        # Set new weight values
        if self.vertical_split:
            pane.width.weight = new_weight
            next_pane.width.weight = next_new_weight  # type: ignore
        else:  # Horizontal split
            pane.height.weight = new_weight
            next_pane.height.weight = next_new_weight  # type: ignore

    def reset_pane_sizes(self):
        """Reset all active pane width and height to 50%"""
        for pane in self.active_panes:
            pane.height = Dimension(weight=50)
            pane.width = Dimension(weight=50)

    def move_pane_up(self):
        pane = self._get_current_active_pane()
        pane_index = self._pane_index(pane)
        if pane_index is None or pane_index <= 0:
            # Already at the beginning
            return

        # Swap with the previous pane
        previous_pane = self.active_panes[pane_index - 1]
        self.active_panes[pane_index - 1] = pane
        self.active_panes[pane_index] = previous_pane

        self.application.update_menu_items()
        self.update_root_container_body()

    def move_pane_down(self):
        pane = self._get_current_active_pane()
        pane_index = self._pane_index(pane)
        pane_count = len(self.active_panes)
        if pane_index is None or pane_index + 1 >= pane_count:
            # Already at the end
            return

        # Swap with the next pane
        next_pane = self.active_panes[pane_index + 1]
        self.active_panes[pane_index + 1] = pane
        self.active_panes[pane_index] = next_pane

        self.application.update_menu_items()
        self.update_root_container_body()

    def toggle_pane(self, pane):
        """Toggle a pane on or off."""
        pane.show_pane = not pane.show_pane
        self.application.update_menu_items()
        self.update_root_container_body()

        # Set focus to the top level menu. This has the effect of keeping the
        # menu open if it's already open.
        self.application.focus_main_menu()

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
