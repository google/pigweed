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

from prompt_toolkit.formatted_text import StyleAndTextTuples

import pw_console.text_formatting
from pw_console.log_line import LogLine


class TableView:
    """Store column information and render logs into formatted tables."""

    # TODO(tonymd): Add a method to provide column formatters externally.
    # Should allow for string format, column color, and column ordering.
    FLOAT_FORMAT = '%.3f'
    INT_FORMAT = '%s'
    LAST_TABLE_COLUMN_NAMES = ['msg', 'message', 'file']
    COLUMN_PADDING = '  '

    def __init__(self):
        # Max column widths of each log field
        self.column_widths: collections.OrderedDict = collections.OrderedDict()
        self._header_fragment_cache = None

        # Assume common defaults here before recalculating in set_formatting().
        self._column_width_time = 17
        self._column_width_level = 3

        # Width of all columns except the final message
        self.column_width_prefix_total = 0

    def all_column_names(self):
        columns_names = [
            name for name, _width in self._ordered_column_widths()
        ]
        return columns_names + ['message', 'lvl', 'time']

    def _width_of_justified_fields(self):
        """Calculate the width of all columns except LAST_TABLE_COLUMN_NAMES."""
        padding_width = len(TableView.COLUMN_PADDING)
        used_width = self._column_width_time + padding_width
        used_width += self._column_width_level + padding_width
        used_width += sum([
            width + padding_width for key, width in self.column_widths.items()
            if key not in TableView.LAST_TABLE_COLUMN_NAMES
        ])
        return used_width

    def _ordered_column_widths(self):
        """Return each column and width in the preferred order."""
        # TODO(tonymd): Apply custom ordering here, for now reverse sort.
        return sorted(self.column_widths.items(), reverse=True)

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
        if len(ansi_stripped_level) > self._column_width_level:
            self._column_width_level = len(ansi_stripped_level)

        self.column_width_prefix_total = self._width_of_justified_fields()
        self._update_table_header()

    def _update_table_header(self):
        padding = TableView.COLUMN_PADDING
        padding_formatted_text = ('', padding)
        default_style = 'bold'
        fragments: collections.deque = collections.deque()

        fragments.append(
            (default_style, 'Time'.ljust(self._column_width_time)))
        fragments.append(padding_formatted_text)
        fragments.append(
            (default_style, 'Lvl'.ljust(self._column_width_level)))
        fragments.append(padding_formatted_text)

        for name, width in self._ordered_column_widths():
            # These fields will be shown at the end
            if name in ['msg', 'message', 'file']:
                continue
            fragments.append(
                (default_style, name.title()[:width].ljust(width) + padding))

        fragments.append((default_style, 'Message'))

        self._header_fragment_cache = list(fragments)

    def formatted_header(self):
        """Get pre-formatted table header."""
        return self._header_fragment_cache

    def formatted_row(self, log: LogLine) -> StyleAndTextTuples:
        """Render a single table row."""
        padding = TableView.COLUMN_PADDING
        padding_formatted_text = ('', padding)
        # Don't apply any background styling that would override the parent
        # window or selected-log-line style.
        default_style = ''

        fragments: collections.deque = collections.deque()

        # Column 1: Time
        fragments.append(('class:log-time',
                          log.record.asctime.ljust(self._column_width_time)))
        fragments.append(padding_formatted_text)

        # Column 2: Level
        # Remove any existing ANSI formatting and apply our colors.
        fragments.append(
            ('class:log-level-{}'.format(log.record.levelno),
             pw_console.text_formatting.strip_ansi(log.record.levelname).ljust(
                 self._column_width_level)))
        fragments.append(padding_formatted_text)

        # NOTE: To preseve ANSI formatting on log level use:
        # fragments.extend(
        #     ANSI(log.record.levelname.ljust(
        #         self._column_width_level)).__pt_formatted_text__())

        # Collect remaining columns to display after host time and level.
        columns = []
        for name, width in self._ordered_column_widths():
            # These fields will be shown at the end
            if name in ['msg', 'message', 'file']:
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
                columns.append(value.ljust(width))
            else:
                columns.append(value.rjust(width))

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
            # TODO(tonymd): Make message text coloring accorind to level a user
            # option.
            if log.record.levelno >= 30:  # Warning, Error and Critical
                message_style = 'class:log-level-{}'.format(log.record.levelno)
            message = (message_style, message)
        # Add to columns
        columns.append(message)

        # TODO(tonymd): Display 'file' metadata right justified after the
        # message? It could also appear in the column section.

        # Go through columns and convert to FormattedText where needed.
        for i, column in enumerate(columns):
            # For raw strings that don't have their own ANSI colors, apply the
            # theme color style for this column.
            if isinstance(column, str):
                style = 'class:log-table-column-{}'.format(
                    i + 3) if i <= 7 else default_style
                fragments.append((style, column + padding))
            # Add this tuple to fragments.
            elif isinstance(column, tuple):
                fragments.append(column)
            # Add this list to the end of the fragments list.
            elif isinstance(column, list):
                fragments.extend(column)

        # Add the final new line for this row.
        fragments.append(('', '\n'))
        return list(fragments)
