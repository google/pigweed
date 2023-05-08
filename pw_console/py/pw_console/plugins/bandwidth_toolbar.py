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
"""Bandwidth Monitor Toolbar and Tracker."""
from __future__ import annotations

from contextvars import ContextVar
import logging
import textwrap
from typing import TYPE_CHECKING

from prompt_toolkit.layout import WindowAlign

from pw_console.plugin_mixin import PluginMixin
from pw_console.widgets import ToolbarButton, WindowPaneToolbar
from pw_console.widgets.event_count_history import EventCountHistory

if TYPE_CHECKING:
    from _typeshed import ReadableBuffer

_LOG = logging.getLogger('pw_console.serial_debug_logger')


def _log_hex_strings(data: bytes, prefix=''):
    """Create alinged hex number and character view log messages."""
    # Make a list of 2 character hex number strings.
    hex_numbers = textwrap.wrap(data.hex(), 2)

    hex_chars = [
        ('<' + str(b.to_bytes(1, byteorder='big')) + '>')
        .replace("<b'\\x", '', 1)  # Remove b'\x from the beginning
        .replace("<b'", '', 1)  # Remove b' from the beginning
        .replace("'>", '', 1)  # Remove ' from the end
        .rjust(2)
        for b in data
    ]

    # Replace non-printable bytes with dots.
    for i, num in enumerate(hex_numbers):
        if num == hex_chars[i]:
            hex_chars[i] = '..'

    hex_numbers_msg = ' '.join(hex_numbers)
    hex_chars_msg = ' '.join(hex_chars)

    _LOG.debug(
        '%s%s',
        prefix,
        hex_numbers_msg,
        extra=dict(
            extra_metadata_fields={
                'msg': hex_numbers_msg,
                'view': 'hex',
            }
        ),
    )
    _LOG.debug(
        '%s%s',
        prefix,
        hex_chars_msg,
        extra=dict(
            extra_metadata_fields={
                'msg': hex_chars_msg,
                'view': 'chars',
            }
        ),
    )


BANDWIDTH_HISTORY_CONTEXTVAR = ContextVar(
    'pw_console_bandwidth_history',
    default={
        'total': EventCountHistory(interval=3),
        'read': EventCountHistory(interval=3),
        'write': EventCountHistory(interval=3),
    },
)


class SerialBandwidthTracker:
    """Tracks and logs the data read and written by a serial tranport."""

    def __init__(self):
        self.pw_bps_history = BANDWIDTH_HISTORY_CONTEXTVAR.get()

    def track_read_data(self, data: bytes) -> None:
        """Tracks and logs data read."""
        self.pw_bps_history['read'].log(len(data))
        self.pw_bps_history['total'].log(len(data))

        if len(data) > 0:
            prefix = 'Read %2d B: ' % len(data)
            _LOG.debug(
                '%s%s',
                prefix,
                data,
                extra=dict(
                    extra_metadata_fields={
                        'mode': 'Read',
                        'bytes': len(data),
                        'view': 'bytes',
                        'msg': str(data),
                    }
                ),
            )
            _log_hex_strings(data, prefix=prefix)

            # Print individual lines
            for line in data.decode(
                encoding='utf-8', errors='ignore'
            ).splitlines():
                _LOG.debug(
                    '%s',
                    line,
                    extra=dict(
                        extra_metadata_fields={
                            'msg': line,
                            'view': 'lines',
                        }
                    ),
                )

    def track_write_data(self, data: ReadableBuffer) -> None:
        """Tracks and logs data to be written."""
        if isinstance(data, bytes) and len(data) > 0:
            self.pw_bps_history['write'].log(len(data))
            self.pw_bps_history['total'].log(len(data))

            prefix = 'Write %2d B: ' % len(data)
            _LOG.debug(
                '%s%s',
                prefix,
                data,
                extra=dict(
                    extra_metadata_fields={
                        'mode': 'Write',
                        'bytes': len(data),
                        'view': 'bytes',
                        'msg': str(data),
                    }
                ),
            )
            _log_hex_strings(data, prefix=prefix)

            # Print individual lines
            for line in data.decode(
                encoding='utf-8', errors='ignore'
            ).splitlines():
                _LOG.debug(
                    '%s',
                    line,
                    extra=dict(
                        extra_metadata_fields={
                            'msg': line,
                            'view': 'lines',
                        }
                    ),
                )


class BandwidthToolbar(WindowPaneToolbar, PluginMixin):
    """Toolbar for displaying bandwidth history."""

    TOOLBAR_HEIGHT = 1

    def _update_toolbar_text(self):
        """Format toolbar text.

        This queries pyserial_wrapper's EventCountHistory context var to
        retrieve the byte count history for read, write and totals."""
        tokens = []
        self.plugin_logger.debug('BandwidthToolbar _update_toolbar_text')

        for count_name, events in self.history.items():
            tokens.extend(
                [
                    ('', '  '),
                    (
                        'class:theme-bg-active class:theme-fg-active',
                        ' {}: '.format(count_name.title()),
                    ),
                    (
                        'class:theme-bg-active class:theme-fg-cyan',
                        '{:.3f} '.format(events.last_count()),
                    ),
                    (
                        'class:theme-bg-active class:theme-fg-orange',
                        '{} '.format(events.display_unit_title),
                    ),
                ]
            )
            if count_name == 'total':
                tokens.append(
                    ('class:theme-fg-cyan', '{}'.format(events.sparkline()))
                )

        self.formatted_text = tokens

    def get_left_text_tokens(self):
        """Formatted text to display on the far left side."""
        return self.formatted_text

    def get_right_text_tokens(self):
        """Formatted text to display on the far right side."""
        return [('class:theme-fg-blue', 'Serial Bandwidth Usage ')]

    def __init__(self, *args, **kwargs):
        super().__init__(
            *args, center_section_align=WindowAlign.RIGHT, **kwargs
        )

        self.history = BANDWIDTH_HISTORY_CONTEXTVAR.get()
        self.show_toolbar = True
        self.formatted_text = []

        # Buttons for display in the center
        self.add_button(
            ToolbarButton(
                description='Refresh', mouse_handler=self._update_toolbar_text
            )
        )

        # Set plugin options
        self.background_task_update_count: int = 0
        self.plugin_init(
            plugin_callback=self._background_task,
            plugin_callback_frequency=1.0,
            plugin_logger_name='pw_console_bandwidth_toolbar',
        )

    def _background_task(self) -> bool:
        self.background_task_update_count += 1
        self._update_toolbar_text()
        self.plugin_logger.debug(
            'BandwidthToolbar Scheduled Update: #%s',
            self.background_task_update_count,
        )
        return True
