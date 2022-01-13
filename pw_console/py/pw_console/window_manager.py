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
import copy
import functools
from itertools import chain
import logging
import operator
from typing import Any, Dict, Iterable, List

from prompt_toolkit.key_binding import KeyBindings
from prompt_toolkit.layout import (
    Dimension,
    HSplit,
    VSplit,
)
from prompt_toolkit.widgets import MenuItem

from pw_console.console_prefs import ConsolePrefs, error_unknown_window
from pw_console.log_pane import LogPane
import pw_console.widgets.checkbox
from pw_console.widgets import WindowPaneToolbar
from pw_console.window_list import WindowList, DisplayMode

_LOG = logging.getLogger(__package__)

# Weighted amount for adjusting window dimensions when enlarging and shrinking.
_WINDOW_SPLIT_ADJUST = 1


class WindowManager:
    """WindowManager class

    This class handles adding/removing/resizing windows and rendering the
    prompt_toolkit split layout."""

    # pylint: disable=too-many-public-methods

    def __init__(
        self,
        application: Any,
    ):
        self.application = application
        self.window_lists: collections.deque = collections.deque()
        self.window_lists.append(WindowList(self))
        self.key_bindings = self._create_key_bindings()
        self.top_toolbars: List[WindowPaneToolbar] = []
        self.bottom_toolbars: List[WindowPaneToolbar] = []

    def _create_key_bindings(self) -> KeyBindings:
        bindings = KeyBindings()

        @bindings.add('escape', 'c-left')  # Alt-Ctrl-
        def move_pane_left(_event):
            """Move window pane left."""
            self.move_pane_left()

        @bindings.add('escape', 'c-right')  # Alt-Ctrl-
        def move_pane_right(_event):
            """Move window pane right."""
            self.move_pane_right()

        # NOTE: c-up and c-down seem swapped in prompt_toolkit
        @bindings.add('escape', 'c-up')  # Alt-Ctrl-
        def move_pane_down(_event):
            """Move window pane down."""
            self.move_pane_down()

        # NOTE: c-up and c-down seem swapped in prompt_toolkit
        @bindings.add('escape', 'c-down')  # Alt-Ctrl-
        def move_pane_up(_event):
            """Move window pane up."""
            self.move_pane_up()

        @bindings.add('escape', '=')  # Alt-= (mnemonic: Alt Plus)
        def enlarge_pane(_event):
            """Enlarge the active window pane."""
            self.enlarge_pane()

        @bindings.add('escape', '-')  # Alt-minus (mnemonic: Alt Minus)
        def shrink_pane(_event):
            """Shrink the active window pane."""
            self.shrink_pane()

        @bindings.add('escape', ',')  # Alt-, (mnemonic: Alt <)
        def shrink_split(_event):
            """Shrink the current window split."""
            self.shrink_split()

        @bindings.add('escape', '.')  # Alt-. (mnemonic: Alt >)
        def enlarge_split(_event):
            """Enlarge the current window split."""
            self.enlarge_split()

        @bindings.add('escape', 'c-p')  # Ctrl-Alt-p
        def focus_prev_pane(_event):
            """Switch focus to the previous window pane or tab."""
            self.focus_previous_pane()

        @bindings.add('escape', 'c-n')  # Ctrl-Alt-n
        def focus_next_pane(_event):
            """Switch focus to the next window pane or tab."""
            self.focus_next_pane()

        @bindings.add('c-u')
        def balance_window_panes(_event):
            """Balance all window sizes."""
            self.balance_window_sizes()

        return bindings

    def delete_empty_window_lists(self):
        empty_lists = [
            window_list for window_list in self.window_lists
            if window_list.empty()
        ]
        for empty_list in empty_lists:
            self.window_lists.remove(empty_list)

    def add_top_toolbar(self, toolbar: WindowPaneToolbar) -> None:
        self.top_toolbars.append(toolbar)

    def add_bottom_toolbar(self, toolbar: WindowPaneToolbar) -> None:
        self.bottom_toolbars.append(toolbar)

    def create_root_container(self):
        """Create a vertical or horizontal split container for all active
        panes."""
        self.delete_empty_window_lists()

        for window_list in self.window_lists:
            window_list.update_container()

        window_containers = [
            window_list.container for window_list in self.window_lists
        ]

        if self.application.prefs.window_column_split_method == 'horizontal':
            split = HSplit(
                window_containers,
                padding=1,
                padding_char='─',
                padding_style='class:pane_separator',
            )
        else:  # vertical
            split = VSplit(
                window_containers,
                padding=1,
                padding_char='│',
                padding_style='class:pane_separator',
            )

        split_items = []
        split_items.extend(self.top_toolbars)
        split_items.append(split)
        split_items.extend(self.bottom_toolbars)
        return HSplit(split_items)

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

    def focus_previous_pane(self) -> None:
        """Focus on the previous visible window pane or tab."""
        self.focus_next_pane(reverse_order=True)

    def focus_next_pane(self, reverse_order=False) -> None:
        """Focus on the next visible window pane or tab."""
        active_window_list, active_pane = (
            self._get_active_window_list_and_pane())
        if not active_window_list:
            return

        # Total count of window lists and panes
        window_list_count = len(self.window_lists)
        pane_count = len(active_window_list.active_panes)

        # Get currently focused indices
        active_window_list_index = self.window_list_index(active_window_list)
        active_pane_index = active_window_list.pane_index(active_pane)

        increment = -1 if reverse_order else 1
        # Assume we can switch to the next pane in the current window_list
        next_pane_index = active_pane_index + increment

        # Case 1: next_pane_index does not exist in this window list.
        # Action: Switch to the first pane of the next window list.
        if next_pane_index >= pane_count or next_pane_index < 0:
            # Get the next window_list
            next_window_list_index = ((active_window_list_index + increment) %
                                      window_list_count)
            next_window_list = self.window_lists[next_window_list_index]

            # If tabbed window mode is enabled, switch to the first tab.
            if next_window_list.display_mode == DisplayMode.TABBED:
                if reverse_order:
                    next_window_list.switch_to_tab(
                        len(next_window_list.active_panes) - 1)
                else:
                    next_window_list.switch_to_tab(0)
                return

            # Otherwise switch to the first visible window pane.
            pane_list = next_window_list.active_panes
            if reverse_order:
                pane_list = reversed(pane_list)
            for pane in pane_list:
                if pane.show_pane:
                    self.application.focus_on_container(pane)
                    return

        # Case 2: next_pane_index does exist and display mode is tabs.
        # Action: Switch to the next tab of the current window list.
        if active_window_list.display_mode == DisplayMode.TABBED:
            active_window_list.switch_to_tab(next_pane_index)
            return

        # Case 3: next_pane_index does exist and display mode is stacked.
        # Action: Switch to the next visible window pane.
        index_range = range(1, pane_count)
        if reverse_order:
            index_range = range(pane_count - 1, 0, -1)
        for i in index_range:
            next_pane_index = (active_pane_index + i) % pane_count
            next_pane = active_window_list.active_panes[next_pane_index]
            if next_pane.show_pane:
                self.application.focus_on_container(next_pane)
                return
        return

    def move_pane_left(self):
        active_window_list, active_pane = (
            self._get_active_window_list_and_pane())
        if not active_window_list:
            return

        window_list_index = self.window_list_index(active_window_list)
        # Move left should pick the previous window_list
        target_window_list_index = window_list_index - 1

        # Check if a new WindowList should be created on the left
        if target_window_list_index == -1:
            # Add the new WindowList
            target_window_list = WindowList(self)
            self.window_lists.appendleft(target_window_list)
            # New index is 0
            target_window_list_index = 0

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
        """Reset all active pane width and height to defaults"""
        for window_list in self.window_lists:
            window_list.height = Dimension(preferred=10)
            window_list.width = Dimension(preferred=10)

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
        old_width = window_list.width.preferred
        next_old_width = next_window_list.width.preferred  # type: ignore

        # Add to the current split
        new_width = old_width + diff
        if new_width <= 0:
            new_width = old_width

        # Subtract from the next split
        next_new_width = next_old_width - diff
        if next_new_width <= 0:
            next_new_width = next_old_width

        # Set new weight values
        window_list.width.preferred = new_width
        next_window_list.width.preferred = next_new_width  # type: ignore

    def toggle_pane(self, pane):
        """Toggle a pane on or off."""
        window_list, _pane_index = (
            self._find_window_list_and_pane_index(pane))

        # Don't hide if tabbed mode is enabled, the container can't be rendered.
        if window_list.display_mode == DisplayMode.TABBED:
            return
        pane.show_pane = not pane.show_pane
        self.update_root_container_body()
        self.application.update_menu_items()

        # Set focus to the top level menu. This has the effect of keeping the
        # menu open if it's already open.
        self.application.focus_main_menu()

    def focus_first_visible_pane(self):
        """Focus on the first visible container."""
        for pane in self.active_panes():
            if pane.show_pane:
                self.application.focus_on_container(pane)
                break

    def check_for_all_hidden_panes_and_unhide(self) -> None:
        """Scan for window_lists containing only hidden panes."""
        for window_list in self.window_lists:
            all_hidden = all(not pane.show_pane
                             for pane in window_list.active_panes)
            if all_hidden:
                # Unhide the first pane
                self.toggle_pane(window_list.active_panes[0])

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

    def start_resize_pane(self, pane):
        window_list, pane_index = self._find_window_list_and_pane_index(pane)
        window_list.start_resize(pane, pane_index)

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
            # Reset focus if this list is empty
            if len(window_list.active_panes) == 0:
                self.application.focus_main_menu()

    def reset_pane_sizes(self):
        for window_list in self.window_lists:
            window_list.reset_pane_sizes()

    def _remove_panes_from_layout(
            self, pane_titles: Iterable[str]) -> Dict[str, Any]:
        # Gather pane objects and remove them from the window layout.
        collected_panes = {}

        for window_list in self.window_lists:
            # Make a copy of active_panes to prevent mutating the while
            # iterating.
            for pane in copy.copy(window_list.active_panes):
                if pane.pane_title() in pane_titles:
                    collected_panes[pane.pane_title()] = (
                        window_list.remove_pane_no_checks(pane))
        return collected_panes

    def _set_pane_options(self, pane, options: dict) -> None:  # pylint: disable=no-self-use
        if options.get('hidden', False):
            # Hide this pane
            pane.show_pane = False
        if options.get('height', False):
            # Apply new height
            new_height = options['height']
            assert isinstance(new_height, int)
            pane.height.preferred = new_height

    def _set_window_list_display_modes(self, prefs: ConsolePrefs) -> None:
        # Set column display modes
        for column_index, column_type in enumerate(prefs.window_column_modes):
            mode = DisplayMode.STACK
            if 'tabbed' in column_type:
                mode = DisplayMode.TABBED
            self.window_lists[column_index].set_display_mode(mode)

    def _create_new_log_pane_with_loggers(self, window_title, window_options,
                                          existing_pane_titles) -> LogPane:
        if 'loggers' not in window_options:
            error_unknown_window(window_title, existing_pane_titles)

        new_pane = LogPane(application=self.application,
                           pane_title=window_title)
        # Add logger handlers
        for logger_name, logger_options in window_options.get('loggers',
                                                              {}).items():

            log_level_name = logger_options.get('level', None)
            new_pane.add_log_handler(logger_name, level_name=log_level_name)
        return new_pane

    # TODO(tonymd): Split this large function up.
    def apply_config(self, prefs: ConsolePrefs) -> None:
        """Apply window configuration from loaded ConsolePrefs."""
        if not prefs.windows:
            return

        unique_titles = prefs.unique_window_titles
        collected_panes = self._remove_panes_from_layout(unique_titles)
        existing_pane_titles = [
            p.pane_title() for p in collected_panes.values()
            if isinstance(p, LogPane)
        ]

        # Keep track of original non-duplicated pane titles
        already_added_panes = []

        for column_index, column in enumerate(prefs.windows.items()):  # pylint: disable=too-many-nested-blocks
            _column_type, windows = column
            # Add a new window_list if needed
            if column_index >= len(self.window_lists):
                self.window_lists.append(WindowList(self))

            # Set column display mode to stacked by default.
            self.window_lists[column_index].display_mode = DisplayMode.STACK

            # Add windows to the this column (window_list)
            for window_title, window_dict in windows.items():
                window_options = window_dict if window_dict else {}
                new_pane = None
                desired_window_title = window_title
                # Check for duplicate_of: Title value
                window_title = window_options.get('duplicate_of', window_title)

                # Check if this pane is brand new, ready to be added, or should
                # be duplicated.
                if (window_title not in already_added_panes
                        and window_title not in collected_panes):
                    # New pane entirely
                    new_pane = self._create_new_log_pane_with_loggers(
                        window_title, window_options, existing_pane_titles)

                elif window_title not in already_added_panes:
                    # First time adding this pane
                    already_added_panes.append(window_title)
                    new_pane = collected_panes[window_title]

                elif window_title in collected_panes:
                    # Pane added once, duplicate it
                    new_pane = collected_panes[window_title].create_duplicate()
                    # Rename this duplicate pane
                    assert isinstance(new_pane, LogPane)
                    new_pane.set_pane_title(desired_window_title)

                if new_pane:
                    # Set window size and visibility
                    self._set_pane_options(new_pane, window_options)
                    # Add the new pane
                    self.window_lists[column_index].add_pane_no_checks(
                        new_pane)
                    # Apply log filters
                    if isinstance(new_pane, LogPane):
                        new_pane.apply_filters_from_config(window_options)

        # Update column display modes.
        self._set_window_list_display_modes(prefs)
        # Check for columns where all panes are hidden and unhide at least one.
        self.check_for_all_hidden_panes_and_unhide()

        # Update prompt_toolkit containers.
        self.update_root_container_body()
        self.application.update_menu_items()

        # Focus on the first visible pane.
        self.focus_first_visible_pane()

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
