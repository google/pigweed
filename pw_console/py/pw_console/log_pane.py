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
import logging
import re
from typing import Any, List, Optional, Union

from prompt_toolkit.application.current import get_app
from prompt_toolkit.data_structures import Point
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
    VerticalAlign,
    Window,
)
from prompt_toolkit.layout.dimension import AnyDimension
from prompt_toolkit.mouse_events import MouseEvent, MouseEventType

import pw_console.widgets.checkbox
import pw_console.style
from pw_console.log_view import LogView
from pw_console.log_pane_toolbars import (
    BottomToolbarBar,
    LineInfoBar,
    TableToolbar,
)
from pw_console.search_toolbar import SearchToolbar
from pw_console.filter_toolbar import FilterToolbar


class LogContentControl(FormattedTextControl):
    """LogPane prompt_toolkit UIControl for displaying LogContainer lines."""
    @staticmethod
    def indent_wrapped_pw_log_format_line(log_pane: 'LogPane', line_number,
                                          wrap_count):
        """Indent wrapped lines to match pw_cli timestamp & level formatter."""
        prefix_width = log_pane.log_view.get_line_wrap_prefix_width()

        # Return no prefix string if no wrapping is required. If the current log
        # window is smaller than the prefix width then don't indent when
        # wrapping lines.
        if wrap_count == 0 or log_pane.current_log_pane_width <= prefix_width:
            return None

        prefix_string = ' ' * prefix_width

        # If this line matches the selected log line, highlight it.
        cursor: Point = log_pane.log_view.get_cursor_position()
        if cursor and line_number == cursor.y:
            return to_formatted_text(prefix_string,
                                     style='class:selected-log-line')

        return prefix_string

    def create_content(self, width: int, height: Optional[int]) -> UIContent:
        # Save redered height
        if height:
            self.log_pane.last_log_content_height += height
        return super().create_content(width, height)

    def __init__(self, log_pane: 'LogPane', *args, **kwargs) -> None:
        # pylint: disable=too-many-locals
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

        @key_bindings.add('insert')
        def _duplicate(_event: KeyPressEvent) -> None:
            """Duplicate this log pane."""
            self.log_pane.duplicate()

        @key_bindings.add('delete')
        def _delete(_event: KeyPressEvent) -> None:
            """Remove log pane."""
            if self.log_pane.is_a_duplicate:
                self.log_pane.application.window_manager.remove_pane(
                    self.log_pane)

        @key_bindings.add('C')
        def _clear_history(_event: KeyPressEvent) -> None:
            """Clear log pane history."""
            self.log_pane.clear_history()

        @key_bindings.add('g')
        def _scroll_to_top(_event: KeyPressEvent) -> None:
            """Scroll to top."""
            self.log_pane.log_view.scroll_to_top()

        @key_bindings.add('G')
        def _scroll_to_bottom(_event: KeyPressEvent) -> None:
            """Scroll to bottom."""
            self.log_pane.log_view.scroll_to_bottom()

        @key_bindings.add('f')
        def _toggle_follow(_event: KeyPressEvent) -> None:
            """Toggle log line following."""
            self.log_pane.toggle_follow()

        @key_bindings.add('home')
        @key_bindings.add('^')
        @key_bindings.add('0')
        def _horizontal_scroll_beginning(_event: KeyPressEvent) -> None:
            """Scroll all the way to the left."""
            self.log_pane.horizontal_scroll_beginning()

        @key_bindings.add('right')
        def _horizontal_scroll_right(_event: KeyPressEvent) -> None:
            """Scroll to the right."""
            self.log_pane.horizontal_scroll_right()

        @key_bindings.add('left')
        def _horizontal_scroll_left(_event: KeyPressEvent) -> None:
            """Scroll to the left."""
            self.log_pane.horizontal_scroll_left()

        @key_bindings.add('up')
        @key_bindings.add('k')
        def _up(_event: KeyPressEvent) -> None:
            """Select previous log line."""
            self.log_pane.log_view.scroll_up()

        @key_bindings.add('down')
        @key_bindings.add('j')
        def _down(_event: KeyPressEvent) -> None:
            """Select next log line."""
            self.log_pane.log_view.scroll_down()

        @key_bindings.add('pageup')
        def _pageup(_event: KeyPressEvent) -> None:
            """Scroll the logs up by one page."""
            self.log_pane.log_view.scroll_up_one_page()

        @key_bindings.add('pagedown')
        def _pagedown(_event: KeyPressEvent) -> None:
            """Scroll the logs down by one page."""
            self.log_pane.log_view.scroll_down_one_page()

        @key_bindings.add('/')
        @key_bindings.add('c-f')
        def _start_search(_event: KeyPressEvent) -> None:
            """Start searching."""
            self.log_pane.start_search()

        @key_bindings.add('n')
        @key_bindings.add('c-s')
        @key_bindings.add('c-g')
        def _next_search(_event: KeyPressEvent) -> None:
            """Next search match."""
            self.log_pane.log_view.search_forwards()

        @key_bindings.add('N')
        @key_bindings.add('c-r')
        def _previous_search(_event: KeyPressEvent) -> None:
            """Previous search match."""
            self.log_pane.log_view.search_backwards()

        @key_bindings.add('c-l')
        def _clear_search_highlight(_event: KeyPressEvent) -> None:
            """Remove search highlighting."""
            self.log_pane.log_view.search_highlight = False

        @key_bindings.add('escape', 'c-f')  # Alt-Ctrl-f
        def _apply_filter(_event: KeyPressEvent) -> None:
            """Apply current search as a filter."""
            self.log_pane.log_view.apply_filter()

        @key_bindings.add('escape', 'c-r')  # Alt-Ctrl-r
        def _clear_filter(_event: KeyPressEvent) -> None:
            """Reset / erase active filters."""
            self.log_pane.log_view.clear_filters()

        @key_bindings.add('c-c')
        def _copy_log_lines(_event: KeyPressEvent) -> None:
            """Copy visible log lines to the system clipboard."""
            self.log_pane.copy_text()

        kwargs['key_bindings'] = key_bindings
        super().__init__(*args, **kwargs)

    def mouse_handler(self, mouse_event: MouseEvent):
        """Mouse handler for this control."""
        mouse_position = mouse_event.position

        # Check for pane focus first.
        # If not in focus, change forus to the log pane and do nothing else.
        if not has_focus(self)():
            if mouse_event.event_type == MouseEventType.MOUSE_UP:
                # Focus the search bar if it is open.
                if self.log_pane.search_bar_active:
                    get_app().layout.focus(self.log_pane.search_toolbar)
                # Otherwise focus on the log pane content.
                else:
                    get_app().layout.focus(self)
                # Mouse event handled, return None.
                return None

        if mouse_event.event_type == MouseEventType.MOUSE_UP:
            # Scroll to the line clicked.
            self.log_pane.log_view.scroll_to_position(mouse_position)
            # Mouse event handled, return None.
            return None

        if mouse_event.event_type == MouseEventType.SCROLL_DOWN:
            self.log_pane.log_view.scroll_down(lines=5)
            # Mouse event handled, return None.
            return None

        if mouse_event.event_type == MouseEventType.SCROLL_UP:
            self.log_pane.log_view.scroll_up(lines=5)
            # Mouse event handled, return None.
            return None

        # Mouse event not handled, return NotImplemented.
        return NotImplemented


class LogLineHSplit(HSplit):
    """PromptToolkit HSplit class with a write_to_screen function that saves the
    width and height of the container to be rendered.
    """
    def __init__(self, log_pane: 'LogPane', *args, **kwargs):
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

    # pylint: disable=too-many-instance-attributes,too-many-public-methods
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
        self._table_view = True
        self.height = height if height else Dimension(weight=50)
        self.width = width if width else Dimension(weight=50)
        self.show_pane = True
        self._pane_title = pane_title
        self._pane_subtitle = None
        self.is_a_duplicate = False

        self.horizontal_scroll_amount = 0

        # Create the log container which stores and handles incoming logs.
        self.log_view: LogView = LogView(self, self.application)

        # Log pane size variables. These are updated just befor rendering the
        # pane by the LogLineHSplit class.
        self.current_log_pane_width = 0
        self.current_log_pane_height = 0
        self.last_log_pane_width = 0
        self.last_log_pane_height = 0
        self.last_log_content_height = 0

        # Search tracking
        self.search_bar_active = False
        self.search_toolbar = SearchToolbar(self)
        self.filter_toolbar = FilterToolbar(self)

        # Table header bar, only shown if table view is active.
        self.table_header_toolbar = TableToolbar(self)

        # Create the bottom toolbar for the whole log pane.
        self.bottom_toolbar = BottomToolbarBar(self)

        self.log_content_control = LogContentControl(
            self,  # parent LogPane
            # FormattedTextControl args:
            self.log_view.render_content,
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
            # get_vertical_scroll=self.get_horizontal_scroll_amount,
            get_horizontal_scroll=self.get_horizontal_scroll_amount,
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
                        self.filter_toolbar,
                        self.search_toolbar,
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
                    # Floating LineInfoBar
                    Float(top=0, right=0, height=1, content=LineInfoBar(self)),
                ]),
            filter=Condition(lambda: self.show_pane))

    @property
    def table_view(self):
        return self._table_view

    @table_view.setter
    def table_view(self, table_view):
        self._table_view = table_view

    def get_horizontal_scroll_amount(self, *_args):
        return self.horizontal_scroll_amount

    def horizontal_scroll_left(self):
        if self.horizontal_scroll_amount > 0:
            self.horizontal_scroll_amount -= 1

    def horizontal_scroll_beginning(self):
        self.horizontal_scroll_amount = 0

    def horizontal_scroll_right(self):
        if self.wrap_lines:
            self.toggle_wrap_lines()
        self.horizontal_scroll_amount += 1

    def pane_title(self):
        title = self._pane_title
        if not title:
            title = 'Logs'
        return title

    def set_pane_title(self, title: str):
        self._pane_title = title

    def menu_title(self):
        """Return the title to display in the Window menu."""
        title = self.pane_title()

        # List active filters
        if self.log_view.filtering_on:
            title += ' (FILTERS: '
            title += ' '.join([
                log_filter.pattern()
                for log_filter in self.log_view.filters.values()
            ])
            title += ')'
        return title

    def append_pane_subtitle(self, text):
        if not self._pane_subtitle:
            self._pane_subtitle = text
        else:
            self._pane_subtitle = self._pane_subtitle + ', ' + text

    def pane_subtitle(self):
        if not self._pane_subtitle:
            return ', '.join(self.log_view.log_store.channel_counts.keys())
        logger_names = self._pane_subtitle.split(', ')
        additional_text = ''
        if len(logger_names) > 1:
            additional_text = ' + {} more'.format(len(logger_names))

        return logger_names[0] + additional_text

    def copy_text(self):
        """Copy visible text in this window pane to the system clipboard."""
        self.log_view.copy_visible_lines()

    def start_search(self):
        """Show the search bar to begin a search."""
        # Show the search bar
        self.search_bar_active = True
        # Focus on the search bar
        self.application.focus_on_container(self.search_toolbar)

    def update_log_pane_size(self, width, height):
        """Save width and height of the log pane for the current UI render
        pass."""
        if width:
            self.last_log_pane_width = self.current_log_pane_width
            self.current_log_pane_width = width
        if height:
            # Subtract the height of the BottomToolbarBar
            height -= BottomToolbarBar.TOOLBAR_HEIGHT
            if self._table_view:
                height -= TableToolbar.TOOLBAR_HEIGHT
            if self.search_bar_active:
                height -= SearchToolbar.TOOLBAR_HEIGHT
            if self.log_view.filtering_on:
                height -= FilterToolbar.TOOLBAR_HEIGHT
            self.last_log_pane_height = self.current_log_pane_height
            self.current_log_pane_height = height

    def redraw_ui(self):
        """Trigger a prompt_toolkit UI redraw."""
        self.application.redraw_ui()

    def toggle_table_view(self):
        """Enable or disable table view."""
        self._table_view = not self._table_view
        self.redraw_ui()

    def toggle_wrap_lines(self):
        """Enable or disable line wraping/truncation."""
        self.wrap_lines = not self.wrap_lines
        if self.wrap_lines:
            self.horizontal_scroll_beginning()
        self.redraw_ui()

    def toggle_follow(self):
        """Enable or disable following log lines."""
        self.log_view.toggle_follow()
        self.redraw_ui()

    def clear_history(self):
        """Erase stored log lines."""
        self.log_view.clear_scrollback()
        self.redraw_ui()

    def __pt_container__(self):
        """Return the prompt_toolkit root container for this log pane.

        This allows self to be used wherever prompt_toolkit expects a container
        object."""
        return self.container

    def get_all_key_bindings(self) -> List:
        """Return all keybinds for this pane."""
        # Return log content control keybindings
        return [self.log_content_control.get_key_bindings()]

    def get_all_menu_options(self) -> List:
        """Return all menu options for the log pane."""

        options = [
            # Menu separator
            ('-', None),
            (
                '{check} Line wrapping'.format(
                    check=pw_console.widgets.checkbox.to_checkbox_text(
                        self.wrap_lines, end='')),
                self.toggle_wrap_lines,
            ),
            (
                '{check} Table view'.format(
                    check=pw_console.widgets.checkbox.to_checkbox_text(
                        self._table_view, end='')),
                self.toggle_table_view,
            ),
            (
                '{check} Follow'.format(
                    check=pw_console.widgets.checkbox.to_checkbox_text(
                        self.log_view.follow, end='')),
                self.toggle_follow,
            ),
            # Menu separator
            ('-', None),
            (
                'Clear history',
                self.clear_history,
            ),
            (
                'Duplicate pane',
                self.duplicate,
            ),
        ]
        if self.is_a_duplicate:
            options += [(
                'Remove pane',
                functools.partial(self.application.window_manager.remove_pane,
                                  self),
            )]

        # Search / Filter section
        options += [
            # Menu separator
            ('-', None),
            (
                'Hide search highlighting',
                self.log_view.disable_search_highlighting,
            ),
            (
                'Create filter from search results',
                self.log_view.apply_filter,
            ),
            (
                'Reset active filters',
                self.log_view.clear_filters,
            ),
        ]

        return options

    def after_render_hook(self):
        """Run tasks after the last UI render."""
        self.reset_log_content_height()

    def reset_log_content_height(self):
        """Reset log line pane content height."""
        self.last_log_content_height = 0

    def log_content_control_get_cursor_position(self):
        return self.log_view.get_cursor_position()

    def focus_self(self):
        """Switch prompt_toolkit focus to this LogPane."""
        self.application.application.layout.focus(self)

    def apply_filters_from_config(self, window_options) -> None:
        if 'filters' not in window_options:
            return

        for field, criteria in window_options['filters'].items():
            for matcher_name, search_string in criteria.items():
                inverted = matcher_name.endswith('-inverted')
                matcher_name = re.sub(r'-inverted$', '', matcher_name)
                if field == 'all':
                    field = None
                if self.log_view.new_search(
                        search_string,
                        invert=inverted,
                        field=field,
                        search_matcher=matcher_name,
                ):
                    self.log_view.install_new_filter()

    def create_duplicate(self) -> 'LogPane':
        """Create a duplicate of this LogView."""
        new_pane = LogPane(self.application, pane_title=self.pane_title())
        # Set the log_store
        log_store = self.log_view.log_store
        new_pane.log_view.log_store = log_store
        # Register the duplicate pane as a viewer
        log_store.register_viewer(new_pane.log_view)

        # Set any existing search state.
        new_pane.log_view.search_text = self.log_view.search_text
        new_pane.log_view.search_filter = self.log_view.search_filter
        new_pane.log_view.search_matcher = self.log_view.search_matcher
        new_pane.log_view.search_highlight = self.log_view.search_highlight

        # Mark new pane as a duplicate so it can be deleted.
        new_pane.is_a_duplicate = True
        return new_pane

    def duplicate(self) -> None:
        new_pane = self.create_duplicate()
        # Add the new pane.
        self.application.window_manager.add_pane(new_pane)

    def add_log_handler(self,
                        logger: Union[str, logging.Logger],
                        level_name: Optional[str] = None) -> None:
        """Add a log handlers to this LogPane."""

        if isinstance(logger, logging.Logger):
            logger_instance = logger
        elif isinstance(logger, str):
            logger_instance = logging.getLogger(logger)

        if level_name:
            if not hasattr(logging, level_name):
                raise Exception(f'Unknown log level: {level_name}')
            logger_instance.level = getattr(logging, level_name, logging.INFO)
        logger_instance.addHandler(self.log_view.log_store  # type: ignore
                                   )
        self.append_pane_subtitle(  # type: ignore
            logger_instance.name)
