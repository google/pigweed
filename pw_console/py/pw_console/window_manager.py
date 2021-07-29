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
import functools
import operator
from itertools import chain
from typing import Any

from prompt_toolkit.layout import (
    Dimension,
    HSplit,
    VSplit,
)
from prompt_toolkit.widgets import MenuItem

import pw_console.widgets.checkbox
from pw_console.window_list import WindowList, DisplayMode

# Weighted amount for adjusting window dimensions when enlarging and shrinking.
_WINDOW_SPLIT_ADJUST = 2


class WindowManager:
    """WindowManager class

    This class handles adding/removing/resizing windows and rendering the
    prompt_toolkit split layout."""
    def __init__(
        self,
        application: Any,
    ):
        self.application = application
        self.window_lists: collections.deque = collections.deque()
        self.window_lists.append(WindowList(self))

    def delete_empty_window_lists(self):
        empty_lists = [
            window_list for window_list in self.window_lists
            if window_list.empty()
        ]
        for empty_list in empty_lists:
            self.window_lists.remove(empty_list)

    def create_root_container(self):
        """Create a vertical or horizontal split container for all active
        panes."""
        self.delete_empty_window_lists()

        for window_list in self.window_lists:
            window_list.update_container()

        return HSplit([
            VSplit(
                [window_list.container for window_list in self.window_lists],
                padding=1,
                padding_char='│',
                padding_style='class:pane_separator',
                # height=Dimension(weight=50),
                # width=Dimension(weight=50),
            )
        ])

    def update_root_container_body(self):
        # Replace the root MenuContainer body with the new split.
        self.application.root_container.container.content.children[
            1] = self.create_root_container()

    def _get_active_window_list_and_pane(self):
        active_pane = None
        active_window_list = None
        for window_list in self.window_lists:
            active_pane = window_list.get_current_active_pane()
            if active_pane:
                active_window_list = window_list
                break
        return active_window_list, active_pane

    def window_list_index(self, window_list: WindowList):
        index = None
        try:
            index = self.window_lists.index(window_list)
        except ValueError:
            # Ignore ValueError which can be raised by the self.window_lists
            # deque if the window_list can't be found.
            pass
        return index

    def run_action_on_active_pane(self, function_name):
        _active_window_list, active_pane = (
            self._get_active_window_list_and_pane())
        if not hasattr(active_pane, function_name):
            return
        method_to_call = getattr(active_pane, function_name)
        method_to_call()
        return

    def move_pane_left(self):
        active_window_list, active_pane = (
            self._get_active_window_list_and_pane())
        if not active_window_list:
            return

        window_list_index = self.window_list_index(active_window_list)
        # Move left should pick the previous window_list
        target_window_list_index = window_list_index - 1

        # Do nothing if target_window_list is the first one.
        if target_window_list_index < 0:
            return

        # Get the destination window_list
        target_window_list = self.window_lists[target_window_list_index]

        # Move the pane
        active_window_list.remove_pane_no_checks(active_pane)
        target_window_list.add_pane(active_pane, add_at_beginning=True)
        self.delete_empty_window_lists()

    def move_pane_right(self):
        active_window_list, active_pane = (
            self._get_active_window_list_and_pane())
        if not active_window_list:
            return

        window_list_index = self.window_list_index(active_window_list)
        # Move right should pick the next window_list
        target_window_list_index = window_list_index + 1

        # Check if a new WindowList should be created
        if target_window_list_index == len(self.window_lists):
            # Add a new WindowList
            target_window_list = WindowList(self)
            self.window_lists.append(target_window_list)

        # Get the destination window_list
        target_window_list = self.window_lists[target_window_list_index]

        # Move the pane
        active_window_list.remove_pane_no_checks(active_pane)
        target_window_list.add_pane(active_pane, add_at_beginning=True)
        self.delete_empty_window_lists()

    def move_pane_up(self):
        active_window_list, _active_pane = (
            self._get_active_window_list_and_pane())
        if not active_window_list:
            return

        active_window_list.move_pane_up()

    def move_pane_down(self):
        active_window_list, _active_pane = (
            self._get_active_window_list_and_pane())
        if not active_window_list:
            return

        active_window_list.move_pane_down()

    def shrink_pane(self):
        active_window_list, _active_pane = (
            self._get_active_window_list_and_pane())
        if not active_window_list:
            return

        active_window_list.shrink_pane()

    def enlarge_pane(self):
        active_window_list, _active_pane = (
            self._get_active_window_list_and_pane())
        if not active_window_list:
            return

        active_window_list.enlarge_pane()

    def shrink_split(self):
        if len(self.window_lists) < 2:
            return

        active_window_list, _active_pane = (
            self._get_active_window_list_and_pane())
        if not active_window_list:
            return

        self.adjust_split_size(active_window_list, -_WINDOW_SPLIT_ADJUST)

    def enlarge_split(self):
        active_window_list, _active_pane = (
            self._get_active_window_list_and_pane())
        if not active_window_list:
            return

        self.adjust_split_size(active_window_list, _WINDOW_SPLIT_ADJUST)

    def balance_window_sizes(self):
        """Reset all splits and pane sizes."""
        self.reset_pane_sizes()
        self.reset_split_sizes()

    def reset_split_sizes(self):
        """Reset all active pane width and height to 50%"""
        for window_list in self.window_lists:
            window_list.height = Dimension(weight=50)
            window_list.width = Dimension(weight=50)

    def adjust_split_size(self,
                          window_list: WindowList,
                          diff: int = _WINDOW_SPLIT_ADJUST):
        """Increase or decrease a given window_list's vertical split width."""
        # No need to resize if only one split.
        if len(self.window_lists) < 2:
            return

        # Get the next split to subtract a weight value from.
        window_list_index = self.window_list_index(window_list)
        next_window_list_index = ((window_list_index + 1) %
                                  len(self.window_lists))

        # Use the previous window if we are on the last split
        if window_list_index == len(self.window_lists) - 1:
            next_window_list_index = window_list_index - 1

        next_window_list = self.window_lists[next_window_list_index]

        # Get current weight values
        old_weight = window_list.width.weight
        next_old_weight = next_window_list.width.weight  # type: ignore

        # Add to the current split
        new_weight = old_weight + diff
        if new_weight <= 0:
            new_weight = old_weight

        # Subtract from the next split
        next_new_weight = next_old_weight - diff
        if next_new_weight <= 0:
            next_new_weight = next_old_weight

        # Set new weight values
        window_list.width.weight = new_weight
        next_window_list.width.weight = next_new_weight  # type: ignore

    def toggle_pane(self, pane):
        """Toggle a pane on or off."""
        pane.show_pane = not pane.show_pane
        self.application.update_menu_items()
        self.update_root_container_body()

        # Set focus to the top level menu. This has the effect of keeping the
        # menu open if it's already open.
        self.application.focus_main_menu()

    def add_pane_no_checks(self, pane: Any):
        self.window_lists[0].add_pane_no_checks(pane)

    def add_pane(self, pane: Any):
        self.window_lists[0].add_pane(pane, add_at_beginning=True)

    def first_window_list(self):
        return self.window_lists[0]

    def active_panes(self):
        """Return all active panes from all window lists."""
        return chain.from_iterable(
            map(operator.attrgetter('active_panes'), self.window_lists))

    def _find_window_list_and_pane_index(self, pane: Any):
        pane_index = None
        parent_window_list = None
        for window_list in self.window_lists:
            pane_index = window_list.pane_index(pane)
            if pane_index is not None:
                parent_window_list = window_list
                break
        return parent_window_list, pane_index

    def remove_pane(self, existing_pane: Any):
        window_list, _pane_index = (
            self._find_window_list_and_pane_index(existing_pane))
        if window_list:
            window_list.remove_pane(existing_pane)

    def reset_pane_sizes(self):
        for window_list in self.window_lists:
            window_list.reset_pane_sizes()

    def create_window_menu(self):
        """Build the [Window] menu for the current set of window lists."""
        root_menu_items = []
        for window_list_index, window_list in enumerate(self.window_lists):
            menu_items = []
            menu_items.append(
                MenuItem(
                    'Column {index} View Modes'.format(
                        index=window_list_index + 1),
                    children=[
                        MenuItem(
                            '{check} {display_mode} Windows'.format(
                                display_mode=display_mode.value,
                                check=pw_console.widgets.checkbox.
                                to_checkbox_text(
                                    window_list.display_mode == display_mode,
                                    end='',
                                )),
                            handler=functools.partial(
                                window_list.set_display_mode, display_mode),
                        ) for display_mode in DisplayMode
                    ],
                ))
            menu_items.extend(
                MenuItem(
                    '{index}: {title} {subtitle}'.format(
                        index=pane_index + 1,
                        title=pane.menu_title(),
                        subtitle=pane.pane_subtitle()),
                    children=[
                        MenuItem(
                            '{check} Show Window'.format(
                                check=pw_console.widgets.checkbox.
                                to_checkbox_text(pane.show_pane, end='')),
                            handler=functools.partial(self.toggle_pane, pane),
                        ),
                    ] + [
                        MenuItem(text,
                                 handler=functools.partial(
                                     self.application.run_pane_menu_option,
                                     handler))
                        for text, handler in pane.get_all_menu_options()
                    ],
                ) for pane_index, pane in enumerate(window_list.active_panes))
            if window_list_index + 1 < len(self.window_lists):
                menu_items.append(MenuItem('-'))
            root_menu_items.extend(menu_items)

        menu = MenuItem(
            '[Windows]',
            children=root_menu_items,
        )

        return [menu]
