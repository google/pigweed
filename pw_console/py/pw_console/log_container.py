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
"""LogContainer storage class."""

import collections
import logging
import sys
import time
from datetime import datetime
from typing import List, Dict, Optional

from prompt_toolkit.data_structures import Point
from prompt_toolkit.formatted_text import (
    to_formatted_text,
    fragment_list_width,
    StyleAndTextTuples,
)

import pw_cli.color

import pw_console.text_formatting
from pw_console.log_line import LogLine
from pw_console.widgets.table import TableView

_LOG = logging.getLogger(__package__)


class LogContainer(logging.Handler):
    """Class to hold many log events."""

    # pylint: disable=too-many-instance-attributes,too-many-public-methods

    def __init__(self):
        # Log storage deque for fast addition and deletion from the beginning
        # and end of the iterable.
        self.logs: collections.deque = collections.deque()
        # Estimate of the logs in memory.
        self.byte_size: int = 0

        # Only allow this many log lines in memory.
        self.max_history_size: int = 1000000

        # Counts of logs per python logger name
        self.channel_counts: Dict[str, int] = {}
        # Widths of each logger prefix string. For example: the character length
        # of the timestamp string.
        self.channel_formatted_prefix_widths: Dict[str, int] = {}
        # Longest of the above prefix widths.
        self.longest_channel_prefix_width = 0

        # Current log line index state variables:
        self.line_index = 0
        self._last_start_index = 0
        self._last_end_index = 0
        self._current_start_index = 0
        self._current_end_index = 0

        self.table = TableView()

        # LogPane prompt_toolkit container render size.
        self._window_height = 20
        self._window_width = 80

        # Max frequency in seconds of prompt_toolkit UI redraws triggered by new
        # log lines.
        self._ui_update_frequency = 0.1
        self._last_ui_update_time = time.time()

        # Erase existing logs.
        self.clear_logs()
        # New logs have arrived flag.
        self.has_new_logs = True

        # Should new log lines be tailed?
        self.follow = True

        # Parent LogPane reference. Updated by calling `set_log_pane()`.
        self.log_pane = None

        # Cache of formatted text tuples used in the last UI render.  Used after
        # rendering by `get_cursor_position()`.
        self._line_fragment_cache = None

        super().__init__()

    def set_log_pane(self, log_pane):
        """Set the parent LogPane instance."""
        self.log_pane = log_pane

    def set_formatting(self):
        """Setup log formatting."""
        # Copy of pw_cli log formatter
        colors = pw_cli.color.colors(True)
        timestamp_prefix = colors.black_on_white('%(asctime)s') + ' '
        timestamp_format = '%Y%m%d %H:%M:%S'
        format_string = timestamp_prefix + '%(levelname)s %(message)s'
        formatter = logging.Formatter(format_string, timestamp_format)

        self.setLevel(logging.DEBUG)
        self.setFormatter(formatter)

        # Update log time character width.
        example_time_string = datetime.now().strftime(timestamp_format)
        self.table.column_width_time = len(example_time_string)

    def get_current_line(self):
        """Return the currently selected log event index."""
        return self.line_index

    def clear_logs(self):
        """Erase all stored pane lines."""
        self.logs = collections.deque()
        self.byte_size = 0
        self.channel_counts = {}
        self.channel_formatted_prefix_widths = {}
        self.line_index = 0

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

    def get_total_count(self):
        """Total size of the logs container."""
        return len(self.logs)

    def get_last_log_line_index(self):
        """Last valid index of the logs."""
        # Subtract 1 since self.logs is zero indexed.
        total = self.get_total_count()
        return 0 if total < 0 else total - 1

    def get_channel_counts(self):
        """Return the seen channel log counts for this conatiner."""
        return ', '.join([
            f'{name}: {count}' for name, count in self.channel_counts.items()
        ])

    def get_line_wrap_prefix_width(self):
        if self.wrap_lines_enabled():
            if self.log_pane.table_view:
                return self.table.column_width_prefix_total
            return self.longest_channel_prefix_width
        return 0

    def _update_log_prefix_width(self, record: logging.LogRecord):
        """Save the formatted prefix width if this is a new logger channel
        name."""
        if self.formatter and (
                record.name
                not in self.channel_formatted_prefix_widths.keys()):
            # Find the width of the formatted timestamp and level
            format_string = self.formatter._fmt  # pylint: disable=protected-access

            # There may not be a _fmt defined.
            if not format_string:
                return

            format_without_message = format_string.replace('%(message)s', '')
            formatted_time_and_level = format_without_message % dict(
                asctime=record.asctime, levelname=record.levelname)

            # Delete ANSI escape sequences.
            ansi_stripped_time_and_level = (
                pw_console.text_formatting.strip_ansi(formatted_time_and_level)
            )

            self.channel_formatted_prefix_widths[record.name] = len(
                ansi_stripped_time_and_level)

            # Set the max width of all known formats so far.
            self.longest_channel_prefix_width = max(
                self.channel_formatted_prefix_widths.values())

    def _append_log(self, record: logging.LogRecord):
        """Add a new log event."""
        # Format incoming log line.
        formatted_log = self.format(record)
        ansi_stripped_log = pw_console.text_formatting.strip_ansi(
            formatted_log)
        # Save this log.
        self.logs.append(
            LogLine(record=record,
                    formatted_log=formatted_log,
                    ansi_stripped_log=ansi_stripped_log))
        # Increment this logger count
        self.channel_counts[record.name] = self.channel_counts.get(
            record.name, 0) + 1

        # Save prefix width of this log line.
        self._update_log_prefix_width(record)

        # Parse metadata fields
        self.logs[-1].update_metadata()

        # Check for bigger column widths.
        self.table.update_metadata_column_widths(self.logs[-1])

        # Update estimated byte_size.
        self.byte_size += sys.getsizeof(self.logs[-1])
        # If the total log lines is > max_history_size, delete the oldest line.
        if self.get_total_count() > self.max_history_size:
            self.byte_size -= sys.getsizeof(self.logs.popleft())

        # Set the has_new_logs flag.
        self.has_new_logs = True
        # If follow is on, scroll to the last line.
        if self.follow:
            self.scroll_to_bottom()

    def _update_prompt_toolkit_ui(self):
        """Update Prompt Toolkit UI if a certain amount of time has passed."""
        emit_time = time.time()
        # Has enough time passed since last UI redraw?
        if emit_time > self._last_ui_update_time + self._ui_update_frequency:
            # Update last log time
            self._last_ui_update_time = emit_time

            # Trigger Prompt Toolkit UI redraw.
            console_app = self.log_pane.application
            if hasattr(console_app, 'application'):
                # Thread safe way of sending a repaint trigger to the input
                # event loop.
                console_app.application.invalidate()

    def log_content_changed(self):
        """Return True if new log lines have appeared since the last render."""
        return self.has_new_logs

    def get_cursor_position(self) -> Optional[Point]:
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
                    return Point(x=column, y=row)
                column += len(text)
        return Point(0, 0)

    def emit(self, record):
        """Process a new log record.

        This defines the logging.Handler emit() fuction which is called by
        logging.Handler.handle() We don't implement handle() as it is done in
        the parent class with thread safety and filters applied.
        """
        self._append_log(record)
        self._update_prompt_toolkit_ui()

    def scroll_to_top(self):
        """Move selected index to the beginning."""
        # Stop following so cursor doesn't jump back down to the bottom.
        self.follow = False
        self.line_index = 0

    def scroll_to_bottom(self):
        """Move selected index to the end."""
        # Don't change following state like scroll_to_top.
        self.line_index = max(0, self.get_last_log_line_index())

    def scroll(self, lines):
        """Scroll up or down by plus or minus lines.

        This method is only called by user keybindings.
        """
        # If the user starts scrolling, stop auto following.
        self.follow = False

        # If scrolling to an index below zero, set to zero.
        new_line_index = max(0, self.line_index + lines)
        # If past the end, set to the last index of self.logs.
        if new_line_index >= self.get_total_count():
            new_line_index = self.get_last_log_line_index()
        # Set the new selected line index.
        self.line_index = new_line_index

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

        starting_index = 0
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

            starting_index = max(0, self.line_index - max_window_row_index)
            # Use the current_window_height if line_index is less
            ending_index = max(self.line_index, max_window_row_index)

        if ending_index > self.get_last_log_line_index():
            ending_index = self.get_last_log_line_index()

        # Save start and end index.
        self._current_start_index = starting_index
        self._current_end_index = ending_index
        self.has_new_logs = False

        return starting_index, ending_index

    def render_table_header(self):
        """Get pre-formatted table header."""
        return self.table.formatted_header()

    def render_content(self) -> List:
        """Return log lines as a list of FormattedText tuples.

        This function handles selecting the lines that should be displayed for
        the current log line position and the given window size. It also sets
        the cursor position depending on which line is selected.
        """
        # Reset _line_fragment_cache ( used in self.get_cursor_position )
        self._line_fragment_cache = collections.deque()

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

        # Get indicies of stored logs that will fit on screen.
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
            line_fragments: StyleAndTextTuples = (self.table.formatted_row(
                self.logs[i]) if self.log_pane.table_view else
                                                  self.logs[i].get_fragments())

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
            line_breaks = self.logs[i].ansi_stripped_log.count('\n')
            used_lines += line_breaks

            # If this is the selected line apply a style class for highlighting.
            if i == self.line_index:
                line_fragments = (
                    pw_console.text_formatting.fill_character_width(
                        line_fragments,
                        fragment_width,
                        self._window_width,
                        remaining_width,
                        self.wrap_lines_enabled(),
                        add_cursor=True))

                # Apply the selected-log-line background color
                line_fragments = to_formatted_text(
                    line_fragments, style='class:selected-log-line')

            # Save this line to the beginning of the cache.
            self._line_fragment_cache.appendleft(line_fragments)
            total_used_lines += used_lines

        # Pad empty lines above current lines if the window isn't filled. This
        # will push the table header to the top.
        if total_used_lines < self._window_height:
            empty_line_count = self._window_height - total_used_lines
            self._line_fragment_cache.appendleft([('', '\n' * empty_line_count)
                                                  ])

        return pw_console.text_formatting.flatten_formatted_text_tuples(
            self._line_fragment_cache)
