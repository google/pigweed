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
"""LogStore saves logs and acts as a Python logging handler."""

from __future__ import annotations
import collections
import logging
import sys
from datetime import datetime
from typing import Dict, List, TYPE_CHECKING

import pw_cli.color

from pw_console.console_prefs import ConsolePrefs
from pw_console.log_line import LogLine
import pw_console.text_formatting
from pw_console.widgets.table import TableView

if TYPE_CHECKING:
    from pw_console.log_view import LogView


class LogStore(logging.Handler):
    """Class to hold many log events."""
    def __init__(self, prefs: ConsolePrefs):
        self.prefs = prefs
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

        self.table: TableView = TableView(prefs=self.prefs)

        # Erase existing logs.
        self.clear_logs()

        # List of viewers that should be notified on new log line arrival.
        self.registered_viewers: List['LogView'] = []

        super().__init__()

        # Set formatting after logging.Handler init.
        self.set_formatting()

    def register_viewer(self, viewer: 'LogView'):
        self.registered_viewers.append(viewer)

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

    def clear_logs(self):
        """Erase all stored pane lines."""
        self.logs = collections.deque()
        self.byte_size = 0
        self.channel_counts = {}
        self.channel_formatted_prefix_widths = {}
        self.line_index = 0

    def get_channel_counts(self):
        """Return the seen channel log counts for this conatiner."""
        return ', '.join([
            f'{name}: {count}' for name, count in self.channel_counts.items()
        ])

    def get_total_count(self):
        """Total size of the logs store."""
        return len(self.logs)

    def get_last_log_line_index(self):
        """Last valid index of the logs."""
        # Subtract 1 since self.logs is zero indexed.
        total = self.get_total_count()
        return 0 if total < 0 else total - 1

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

    def emit(self, record):
        """Process a new log record.

        This defines the logging.Handler emit() fuction which is called by
        logging.Handler.handle() We don't implement handle() as it is done in
        the parent class with thread safety and filters applied.
        """
        self._append_log(record)
        # Notify viewers of new logs
        for viewer in self.registered_viewers:
            # TODO(tonymd): Type of viewer does not seem to be checked
            viewer.new_logs_arrived()

    def render_table_header(self):
        """Get pre-formatted table header."""
        return self.table.formatted_header()
