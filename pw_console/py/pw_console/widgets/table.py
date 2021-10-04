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
"""Table view renderer for LogLines."""

import collections
import copy
import logging

from prompt_toolkit.formatted_text import StyleAndTextTuples

from pw_console.console_prefs import ConsolePrefs
from pw_console.log_line import LogLine
import pw_console.text_formatting

_LOG = logging.getLogger(__package__)


class TableView:
    """Store column information and render logs into formatted tables."""

    # TODO(tonymd): Add a method to provide column formatters externally.
    # Should allow for string format, column color, and column ordering.
    FLOAT_FORMAT = '%.3f'
    INT_FORMAT = '%s'
    LAST_TABLE_COLUMN_NAMES = ['msg', 'message', 'file']

    def __init__(self, prefs: ConsolePrefs):
        self.prefs = prefs
        # Max column widths of each log field
        self.column_padding = ' ' * self.prefs.spaces_between_columns

        self.column_widths: collections.OrderedDict = collections.OrderedDict()
        self._header_fragment_cache = None

        # Assume common defaults here before recalculating in set_formatting().
        self._default_time_width: int = 17
        self.column_widths['time'] = self._default_time_width
        self.column_widths['level'] = 3
        self._year_month_day_width: int = 9
        if self.prefs.hide_date_from_log_time:
            self.column_widths['time'] = (self._default_time_width -
                                          self._year_month_day_width)

        # Width of all columns except the final message
        self.column_width_prefix_total = 0

    def all_column_names(self):
        columns_names = [
            name for name, _width in self._ordered_column_widths()
        ]
        return columns_names + ['message']

    def _width_of_justified_fields(self):
        """Calculate the width of all columns except LAST_TABLE_COLUMN_NAMES."""
        padding_width = len(self.column_padding)
        used_width = sum([
            width + padding_width for key, width in self.column_widths.items()
            if key not in TableView.LAST_TABLE_COLUMN_NAMES
        ])
        return used_width

    def _ordered_column_widths(self):
        """Return each column and width in the preferred order."""
        # Reverse sort if no custom ordering.
        if not self.prefs.column_order:
            return self.column_widths.items()

        # Get ordered_columns
        columns = copy.copy(self.column_widths)
        ordered_columns = {}

        for column_name in self.prefs.column_order:
            # If valid column name
            if column_name in columns:
                ordered_columns[column_name] = columns.pop(column_name)
        # NOTE: Any remaining columns not specified by the user are not shown.
        # Perhaps a user setting could add them at the end. To add them in:
        if not self.prefs.omit_unspecified_columns:
            for column_name in columns:
                ordered_columns[column_name] = columns[column_name]
        return ordered_columns.items()

    def update_metadata_column_widths(self, log: LogLine):
        """Calculate the max widths for each metadata field."""
        for field_name, value in log.metadata.fields.items():
            value_string = str(value)

            # Get width of formatted numbers
            if isinstance(value, float):
                value_string = TableView.FLOAT_FORMAT % value
            elif isinstance(value, int):
                value_string = TableView.INT_FORMAT % value

            current_width = self.column_widths.get(field_name, 0)
            if len(value_string) > current_width:
                self.column_widths[field_name] = len(value_string)

        # Update log level character width.
        ansi_stripped_level = pw_console.text_formatting.strip_ansi(
            log.record.levelname)
        if len(ansi_stripped_level) > self.column_widths['level']:
            self.column_widths['level'] = len(ansi_stripped_level)

        self.column_width_prefix_total = self._width_of_justified_fields()
        self._update_table_header()

    def _update_table_header(self):
        default_style = 'bold'
        fragments: collections.deque = collections.deque()

        for name, width in self._ordered_column_widths():
            # These fields will be shown at the end
            if name in ['msg', 'message', 'file']:
                continue
            fragments.append(
                (default_style, name.title()[:width].ljust(width)))
            fragments.append(('', self.column_padding))

        fragments.append((default_style, 'Message'))

        self._header_fragment_cache = list(fragments)

    def formatted_header(self):
        """Get pre-formatted table header."""
        return self._header_fragment_cache

    def formatted_row(self, log: LogLine) -> StyleAndTextTuples:
        """Render a single table row."""
        padding_formatted_text = ('', self.column_padding)
        # Don't apply any background styling that would override the parent
        # window or selected-log-line style.
        default_style = ''

        fragments: collections.deque = collections.deque()

        # NOTE: To preseve ANSI formatting on log level use:
        # fragments.extend(
        #     ANSI(log.record.levelname.ljust(
        #         self.column_widths['level'])).__pt_formatted_text__())

        # Collect remaining columns to display after host time and level.
        columns = {}
        for name, width in self._ordered_column_widths():
            # Skip these modifying these fields
            if name in ['msg', 'message', 'file']:
                continue

            if name == 'time':
                time_text = log.record.asctime
                if self.prefs.hide_date_from_log_time:
                    time_text = time_text[self._year_month_day_width:]
                time_style = self.prefs.column_style('time',
                                                     time_text,
                                                     default='class:log-time')
                columns['time'] = (time_style,
                                   time_text.ljust(self.column_widths['time']))
                continue

            if name == 'level':
                # Remove any existing ANSI formatting and apply our colors.
                level_text = pw_console.text_formatting.strip_ansi(
                    log.record.levelname)
                level_style = self.prefs.column_style(
                    'level',
                    level_text,
                    default='class:log-level-{}'.format(log.record.levelno))
                columns['level'] = (level_style,
                                    level_text.ljust(
                                        self.column_widths['level']))
                continue

            value = log.metadata.fields.get(name, ' ')
            left_justify = True

            # Right justify and format numbers
            if isinstance(value, float):
                value = TableView.FLOAT_FORMAT % value
                left_justify = False
            elif isinstance(value, int):
                value = TableView.INT_FORMAT % value
                left_justify = False

            if left_justify:
                columns[name] = value.ljust(width)
            else:
                columns[name] = value.rjust(width)

        # Grab the message to appear after the justified columns.
        message = log.metadata.fields.get(
            'msg',
            # ANSI format the standard formatted log message if no 'msg'
            # available from metadata.fields.
            # ANSI(log.record.message).__pt_formatted_text__()
            pw_console.text_formatting.strip_ansi(log.record.message))
        # Convert to FormattedText if we have a raw string from fields.
        if isinstance(message, str):
            message_style = default_style
            if log.record.levelno >= 30:  # Warning, Error and Critical
                # Style the whole message to match it's level
                message_style = 'class:log-level-{}'.format(log.record.levelno)
            message = (message_style, message)
        # Add to columns
        columns['message'] = message

        # TODO(tonymd): Display 'file' metadata right justified after the
        # message? It could also appear in the column section.

        index_modifier = 0
        # Go through columns and convert to FormattedText where needed.
        for i, column in enumerate(columns.items()):
            column_name, column_value = column
            if i in [0, 1] and column_name in ['time', 'level']:
                index_modifier -= 1
            # For raw strings that don't have their own ANSI colors, apply the
            # theme color style for this column.
            if isinstance(column_value, str):
                fallback_style = 'class:log-table-column-{}'.format(
                    i + index_modifier) if 0 <= i <= 7 else default_style

                style = self.prefs.column_style(column_name,
                                                column_value.strip(),
                                                default=fallback_style)

                fragments.append((style, column_value))
                fragments.append(padding_formatted_text)
            # Add this tuple to fragments.
            elif isinstance(column, tuple):
                fragments.append(column_value)
                # Add padding if not the last column.
                if i < len(columns) - 1:
                    fragments.append(padding_formatted_text)

        # Add the final new line for this row.
        fragments.append(('', '\n'))
        return list(fragments)
