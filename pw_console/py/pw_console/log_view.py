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
"""LogView maintains a log pane's scrolling and searching state."""

from __future__ import annotations
import asyncio
import collections
import copy
import itertools
import logging
import re
import time
from typing import List, Optional, TYPE_CHECKING

from prompt_toolkit.data_structures import Point
from prompt_toolkit.formatted_text import (
    to_formatted_text,
    fragment_list_to_text,
    fragment_list_width,
    StyleAndTextTuples,
)

from pw_console.log_filter import (
    DEFAULT_SEARCH_MATCHER,
    LogFilter,
    RegexValidator,
    SearchMatcher,
    preprocess_search_regex,
)
from pw_console.log_store import LogStore
import pw_console.text_formatting

if TYPE_CHECKING:
    from pw_console.console_app import ConsoleApp
    from pw_console.log_line import LogLine
    from pw_console.log_pane import LogPane

_LOG = logging.getLogger(__package__)


class LogView:
    """Viewing window into a LogStore."""

    # pylint: disable=too-many-instance-attributes,too-many-public-methods

    def __init__(
        self,
        log_pane: 'LogPane',
        application: 'ConsoleApp',
        log_store: Optional[LogStore] = None,
    ):
        # Parent LogPane reference. Updated by calling `set_log_pane()`.
        self.log_pane = log_pane
        self.log_store = log_store if log_store else LogStore(
            prefs=application.prefs)
        self.log_store.register_viewer(self)

        # Search variables
        self.search_text: Optional[str] = None
        self.search_filter: Optional[LogFilter] = None
        self.search_highlight: bool = False
        self.search_matcher = DEFAULT_SEARCH_MATCHER
        self.search_validator = RegexValidator()

        # Filter
        self.filtering_on: bool = False
        self.filters: 'collections.OrderedDict[str, LogFilter]' = (
            collections.OrderedDict())
        self.filtered_logs: collections.deque = collections.deque()
        self.filter_existing_logs_task = None

        # Current log line index state variables:
        self._line_index = 0
        self._filtered_line_index = 0
        self._last_start_index = 0
        self._last_end_index = 0
        self._current_start_index = 0
        self._current_end_index = 0
        self._scrollback_start_index = 0

        # LogPane prompt_toolkit container render size.
        self._window_height = 20
        self._window_width = 80

        # Max frequency in seconds of prompt_toolkit UI redraws triggered by new
        # log lines.
        self._ui_update_frequency = 0.1
        self._last_ui_update_time = time.time()
        self._last_log_store_index = 0

        # Should new log lines be tailed?
        self.follow: bool = True

        # Cache of formatted text tuples used in the last UI render.  Used after
        # rendering by `get_cursor_position()`.
        self._line_fragment_cache: collections.deque = collections.deque()
        self._line_fragment_cache_flattened: Optional[
            StyleAndTextTuples] = None

    @property
    def line_index(self):
        if self.filtering_on:
            return self._filtered_line_index
        return self._line_index

    @line_index.setter
    def line_index(self, line_index):
        if self.filtering_on:
            self._filtered_line_index = line_index
        else:
            self._line_index = line_index

    def _set_match_position(self, position: int):
        self.follow = False
        self.line_index = position
        self.log_pane.application.redraw_ui()

    def select_next_search_matcher(self):
        matchers = list(SearchMatcher)
        index = matchers.index(self.search_matcher)
        new_index = (index + 1) % len(matchers)
        self.search_matcher = matchers[new_index]

    def search_forwards(self):
        if not self.search_filter:
            return
        self.search_highlight = True

        log_beginning_index = self.hidden_line_count()

        starting_index = self.line_index + 1
        if starting_index > self.get_last_log_line_index():
            starting_index = log_beginning_index

        logs = self._get_log_lines()

        # From current position +1 and down
        for i in range(starting_index, self.get_last_log_line_index() + 1):
            if self.search_filter.matches(logs[i]):
                self._set_match_position(i)
                return

        # From the beginning to the original start
        for i in range(log_beginning_index, starting_index):
            if self.search_filter.matches(logs[i]):
                self._set_match_position(i)
                return

    def search_backwards(self):
        if not self.search_filter:
            return
        self.search_highlight = True

        log_beginning_index = self.hidden_line_count()

        starting_index = self.line_index - 1
        if starting_index < 0:
            starting_index = self.get_last_log_line_index()

        logs = self._get_log_lines()

        # From current position - 1 and up
        for i in range(starting_index, log_beginning_index - 1, -1):
            if self.search_filter.matches(logs[i]):
                self._set_match_position(i)
                return

        # From the end to the original start
        for i in range(self.get_last_log_line_index(), starting_index, -1):
            if self.search_filter.matches(logs[i]):
                self._set_match_position(i)
                return

    def _set_search_regex(self,
                          text,
                          invert,
                          field,
                          matcher: Optional[SearchMatcher] = None) -> bool:
        search_matcher = matcher if matcher else self.search_matcher
        _LOG.debug(search_matcher)

        regex_text, regex_flags = preprocess_search_regex(
            text, matcher=search_matcher)

        try:
            compiled_regex = re.compile(regex_text, regex_flags)
            self.search_filter = LogFilter(
                regex=compiled_regex,
                input_text=text,
                invert=invert,
                field=field,
            )
            _LOG.debug(self.search_filter)
        except re.error as error:
            _LOG.debug(error)
            return False

        self.search_highlight = True
        self.search_text = regex_text
        return True

    def new_search(
        self,
        text,
        invert=False,
        field: Optional[str] = None,
        search_matcher: Optional[str] = None,
    ) -> bool:
        """Start a new search for the given text."""
        valid_matchers = list(s.name for s in SearchMatcher)
        selected_matcher: Optional[SearchMatcher] = None
        if (search_matcher is not None
                and search_matcher.upper() in valid_matchers):
            selected_matcher = SearchMatcher(search_matcher.upper())

        if self._set_search_regex(text, invert, field, selected_matcher):
            # Default search direction when hitting enter in the search bar.
            self.search_backwards()
            return True
        return False

    def disable_search_highlighting(self):
        self.log_pane.log_view.search_highlight = False

    def _restart_filtering(self):
        # Turn on follow
        if not self.follow:
            self.toggle_follow()

        # Reset filtered logs.
        self.filtered_logs.clear()
        # Reset scrollback start
        self._scrollback_start_index = 0

        # Start filtering existing log lines.
        self.filter_existing_logs_task = asyncio.create_task(
            self.filter_past_logs())

        # Reset existing search
        self.clear_search()

        # Trigger a main menu update to set log window menu titles.
        self.log_pane.application.update_menu_items()
        # Redraw the UI
        self.log_pane.application.redraw_ui()

    def install_new_filter(self):
        """Set a filter using the current search_regex."""
        if not self.search_filter:
            return
        self.search_highlight = False

        self.filtering_on = True
        self.filters[self.search_text] = copy.deepcopy(self.search_filter)

    def apply_filter(self):
        """Set new filter and schedule historical log filter asyncio task."""
        self.install_new_filter()
        self._restart_filtering()

    def clear_search(self):
        self.search_text = None
        self.search_filter = None
        self.search_highlight = False

    def _get_log_lines(self):
        logs = self.log_store.logs
        if self.filtering_on:
            logs = self.filtered_logs
        return logs

    def _get_visible_log_lines(self):
        logs = self._get_log_lines()
        if self._scrollback_start_index > 0:
            return collections.deque(
                itertools.islice(logs, self.hidden_line_count(), len(logs)))
        return logs

    def delete_filter(self, filter_text):
        if filter_text not in self.filters:
            return

        # Delete this filter
        del self.filters[filter_text]

        # If no filters left, stop filtering.
        if len(self.filters) == 0:
            self.clear_filters()
        else:
            # Erase existing filtered lines.
            self._restart_filtering()

    def clear_filters(self):
        if not self.filtering_on:
            return
        self.clear_search()
        self.filtering_on = False
        self.filters: 'collections.OrderedDict[str, re.Pattern]' = (
            collections.OrderedDict())
        self.filtered_logs.clear()
        # Reset scrollback start
        self._scrollback_start_index = 0
        if not self.follow:
            self.toggle_follow()

    async def filter_past_logs(self):
        """Filter past log lines."""
        starting_index = self.log_store.get_last_log_line_index()
        ending_index = -1

        # From the end of the log store to the beginning.
        for i in range(starting_index, ending_index, -1):
            # Is this log a match?
            if self.filter_scan(self.log_store.logs[i]):
                # Add to the beginning of the deque.
                self.filtered_logs.appendleft(self.log_store.logs[i])
            # TODO(tonymd): Tune these values.
            # Pause every 100 lines or so
            if i % 100 == 0:
                await asyncio.sleep(.1)

    def set_log_pane(self, log_pane: 'LogPane'):
        """Set the parent LogPane instance."""
        self.log_pane = log_pane

    def get_current_line(self):
        """Return the currently selected log event index."""
        return self.line_index

    def get_total_count(self):
        """Total size of the logs store."""
        return (len(self.filtered_logs)
                if self.filtering_on else self.log_store.get_total_count())

    def get_last_log_line_index(self):
        total = self.get_total_count()
        return 0 if total < 0 else total - 1

    def clear_scrollback(self):
        """Hide log lines before the max length of the stored logs."""
        # Enable follow and scroll to the bottom, then clear.
        if not self.follow:
            self.toggle_follow()
        self._scrollback_start_index = self.line_index

    def hidden_line_count(self):
        """Return the number of hidden lines."""
        if self._scrollback_start_index > 0:
            return self._scrollback_start_index + 1
        return 0

    def undo_clear_scrollback(self):
        """Reset the current scrollback start index."""
        self._scrollback_start_index = 0

    def wrap_lines_enabled(self):
        """Get the parent log pane wrap lines setting."""
        if not self.log_pane:
            return False
        return self.log_pane.wrap_lines

    def toggle_follow(self):
        """Toggle auto line following."""
        self.follow = not self.follow
        if self.follow:
            self.scroll_to_bottom()

    def get_line_wrap_prefix_width(self):
        if self.wrap_lines_enabled():
            if self.log_pane.table_view:
                return self.log_store.table.column_width_prefix_total
            return self.log_store.longest_channel_prefix_width
        return 0

    def filter_scan(self, log: 'LogLine'):
        filter_match_count = 0
        for _filter_text, log_filter in self.filters.items():
            if log_filter.matches(log):
                filter_match_count += 1
            else:
                break

        if filter_match_count == len(self.filters):
            return True
        return False

    def new_logs_arrived(self):
        # If follow is on, scroll to the last line.
        latest_total = self.log_store.get_total_count()

        if self.filtering_on:
            # Scan newly arived log lines
            for i in range(self._last_log_store_index, latest_total):
                if self.filter_scan(self.log_store.logs[i]):
                    self.filtered_logs.append(self.log_store.logs[i])

        self._last_log_store_index = latest_total

        if self.follow:
            self.scroll_to_bottom()

        # Trigger a UI update
        self._update_prompt_toolkit_ui()

    def _update_prompt_toolkit_ui(self):
        """Update Prompt Toolkit UI if a certain amount of time has passed."""
        emit_time = time.time()
        # Has enough time passed since last UI redraw?
        if emit_time > self._last_ui_update_time + self._ui_update_frequency:
            # Update last log time
            self._last_ui_update_time = emit_time

            # Trigger Prompt Toolkit UI redraw.
            self.log_pane.application.redraw_ui()

    def get_cursor_position(self) -> Point:
        """Return the position of the cursor."""
        # This implementation is based on get_cursor_position from
        # prompt_toolkit's FormattedTextControl class.

        fragment = "[SetCursorPosition]"
        # If no lines were rendered.
        if not self._line_fragment_cache:
            return Point(0, 0)
        # For each line rendered in the last pass:
        for row, line in enumerate(self._line_fragment_cache):
            column = 0
            # For each style string and raw text tuple in this line:
            for style_str, text, *_ in line:
                # If [SetCursorPosition] is in the style set the cursor position
                # to this row and column.
                if fragment in style_str:
                    return Point(x=column +
                                 self.log_pane.get_horizontal_scroll_amount(),
                                 y=row)
                column += len(text)
        return Point(0, 0)

    def scroll_to_top(self):
        """Move selected index to the beginning."""
        # Stop following so cursor doesn't jump back down to the bottom.
        self.follow = False
        log_beginning_index = self.hidden_line_count()
        self.line_index = log_beginning_index

    def scroll_to_bottom(self):
        """Move selected index to the end."""
        # Don't change following state like scroll_to_top.
        self.line_index = max(0, self.get_last_log_line_index())
        # Sticky follow mode
        self.follow = True

    def scroll(self, lines):
        """Scroll up or down by plus or minus lines.

        This method is only called by user keybindings.
        """
        # If the user starts scrolling, stop auto following.
        self.follow = False

        last_index = self.get_last_log_line_index()

        log_beginning_index = self.hidden_line_count()

        # If scrolling to an index below zero, set to zero.
        new_line_index = max(log_beginning_index, self.line_index + lines)
        # If past the end, set to the last index of self.logs.
        if new_line_index >= self.get_total_count():
            new_line_index = last_index
        # Set the new selected line index.
        self.line_index = new_line_index
        # Sticky follow mode
        if self.line_index == last_index:
            self.follow = True

    def scroll_to_position(self, mouse_position: Point):
        """Set the selected log line to the mouse_position."""
        # If auto following don't move the cursor arbitrarily. That would stop
        # following and position the cursor incorrectly.
        if self.follow:
            return

        cursor_position = self.get_cursor_position()
        if cursor_position:
            scroll_amount = cursor_position.y - mouse_position.y
            self.scroll(-1 * scroll_amount)

    def scroll_up_one_page(self):
        """Move the selected log index up by one window height."""
        lines = 1
        if self._window_height > 0:
            lines = self._window_height
        self.scroll(-1 * lines)

    def scroll_down_one_page(self):
        """Move the selected log index down by one window height."""
        lines = 1
        if self._window_height > 0:
            lines = self._window_height
        self.scroll(lines)

    def scroll_down(self, lines=1):
        """Move the selected log index down by one or more lines."""
        self.scroll(lines)

    def scroll_up(self, lines=1):
        """Move the selected log index up by one or more lines."""
        self.scroll(-1 * lines)

    def get_log_window_indices(self,
                               available_width=None,
                               available_height=None):
        """Get start and end index."""
        self._last_start_index = self._current_start_index
        self._last_end_index = self._current_end_index

        log_beginning_index = self.hidden_line_count()
        starting_index = log_beginning_index
        ending_index = self.line_index

        self._window_width = self.log_pane.current_log_pane_width
        self._window_height = self.log_pane.current_log_pane_height
        if available_width:
            self._window_width = available_width
        if available_height:
            self._window_height = available_height

        # If render info is available we use the last window height.
        if self._window_height > 0:
            # Window lines are zero indexed so subtract 1 from the height.
            max_window_row_index = self._window_height - 1

            starting_index = max(log_beginning_index,
                                 self.line_index - max_window_row_index)
            # Use the current_window_height if line_index is less
            ending_index = max(self.line_index, max_window_row_index)

            # If log scrollback is cleared we may end up with only 1 visible log
            # line. Compare the total line_count with the available window
            # height.
            line_count = ending_index + 1 - starting_index
            if self._window_height > line_count:
                ending_index += self._window_height - line_count

        if ending_index > self.get_last_log_line_index():
            ending_index = self.get_last_log_line_index()

        # Save start and end index.
        self._current_start_index = starting_index
        self._current_end_index = ending_index

        return starting_index, ending_index

    def render_table_header(self):
        """Get pre-formatted table header."""
        return self.log_store.render_table_header()

    def render_content(self) -> List:
        """Return log lines as a list of FormattedText tuples.

        This function handles selecting the lines that should be displayed for
        the current log line position and the given window size. It also sets
        the cursor position depending on which line is selected.
        """

        logs = self._get_log_lines()

        # Reset _line_fragment_cache ( used in self.get_cursor_position )
        self._line_fragment_cache.clear()

        # Track used lines.
        total_used_lines = 0

        # If we have no logs add one with at least a single space character for
        # the cursor to land on. Otherwise the cursor will be left on the line
        # above the log pane container.
        if self.get_total_count() < 1:
            return [(
                '[SetCursorPosition]', '\n' * self._window_height
                # LogContentControl.mouse_handler will handle focusing the log
                # pane on click.
            )]

        # Get indices of stored logs that will fit on screen.
        starting_index, ending_index = self.get_log_window_indices()

        # NOTE: Since range() is not inclusive use ending_index + 1.
        #
        # Build up log lines from the bottom of the window working up.
        #
        # From the ending_index to the starting index in reverse:
        for i in range(ending_index, starting_index - 1, -1):
            # Stop if we have used more lines than available.
            if total_used_lines > self._window_height:
                break

            # Grab the rendered log line using the table or standard view.
            line_fragments: StyleAndTextTuples = (
                self.log_store.table.formatted_row(logs[i])
                if self.log_pane.table_view else logs[i].get_fragments())

            # Get the width, height and remaining width.
            fragment_width = fragment_list_width(line_fragments)
            line_height = 1
            remaining_width = 0
            # Get the line height respecting line wrapping.
            if self.wrap_lines_enabled() and (fragment_width >
                                              self._window_width):
                line_height, remaining_width = (
                    pw_console.text_formatting.get_line_height(
                        fragment_width, self._window_width,
                        self.get_line_wrap_prefix_width()))

            # Keep track of how many lines are used.
            used_lines = line_height

            # Count the number of line breaks are included in the log line.
            line_breaks = logs[i].ansi_stripped_log.count('\n')
            used_lines += line_breaks

            # If this is the selected line apply a style class for highlighting.
            selected = i == self.line_index
            if selected:
                line_fragments = (
                    pw_console.text_formatting.fill_character_width(
                        line_fragments,
                        fragment_width,
                        self._window_width,
                        remaining_width,
                        self.wrap_lines_enabled(),
                        horizontal_scroll_amount=(
                            self.log_pane.get_horizontal_scroll_amount()),
                        add_cursor=True))

                # Apply the selected-log-line background color
                line_fragments = to_formatted_text(
                    line_fragments, style='class:selected-log-line')

            # Apply search term highlighting.
            if self.search_filter and self.search_highlight and (
                    self.search_filter.matches(logs[i])):
                line_fragments = self.search_filter.highlight_search_matches(
                    line_fragments, selected)

            # Save this line to the beginning of the cache.
            self._line_fragment_cache.appendleft(line_fragments)
            total_used_lines += used_lines

        # Pad empty lines above current lines if the window isn't filled. This
        # will push the table header to the top.
        if total_used_lines < self._window_height:
            empty_line_count = self._window_height - total_used_lines
            self._line_fragment_cache.appendleft([('', '\n' * empty_line_count)
                                                  ])

        self._line_fragment_cache_flattened = (
            pw_console.text_formatting.flatten_formatted_text_tuples(
                self._line_fragment_cache))

        return self._line_fragment_cache_flattened

    def copy_visible_lines(self):
        """Copy the currently visible log lines to the system clipboard."""
        if self._line_fragment_cache_flattened is None:
            return
        text = fragment_list_to_text(self._line_fragment_cache_flattened)
        text = text.strip()
        self.log_pane.application.application.clipboard.set_text(text)
