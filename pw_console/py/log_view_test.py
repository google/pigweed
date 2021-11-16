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
"""Tests for pw_console.log_view"""

import logging
import time
import sys
import unittest
from datetime import datetime
from unittest.mock import MagicMock, patch
from parameterized import parameterized  # type: ignore
from prompt_toolkit.data_structures import Point
from prompt_toolkit.formatted_text import FormattedText
from pw_console.console_prefs import ConsolePrefs

from pw_console.log_view import LogView

_PYTHON_3_8 = sys.version_info >= (
    3,
    8,
)


def _create_log_view():
    log_pane = MagicMock()
    application = MagicMock()
    application.prefs = ConsolePrefs()
    application.prefs.reset_config()
    log_view = LogView(log_pane, application)
    return log_view, log_pane


class TestLogView(unittest.TestCase):
    """Tests for LogView."""
    def setUp(self):
        self.maxDiff = None  # pylint: disable=invalid-name

    def _create_log_view_with_logs(self, log_count=100):
        log_view, log_pane = _create_log_view()

        if log_count > 0:
            test_log = logging.getLogger('log_view.test')
            with self.assertLogs(test_log, level='DEBUG') as _log_context:
                test_log.addHandler(log_view.log_store)
                for i in range(log_count):
                    test_log.debug('Test log %s', i)

        return log_view, log_pane

    def test_follow_toggle(self) -> None:
        log_view, _pane = _create_log_view()
        self.assertTrue(log_view.follow)
        log_view.toggle_follow()
        self.assertFalse(log_view.follow)

    def test_follow_scrolls_to_bottom(self) -> None:
        log_view, _pane = _create_log_view()
        log_view.toggle_follow()
        self.assertFalse(log_view.follow)
        self.assertEqual(log_view.get_current_line(), 0)

        test_log = logging.getLogger('log_view.test')

        # Log 5 messagse, current_line should stay at 0
        with self.assertLogs(test_log, level='DEBUG') as _log_context:
            test_log.addHandler(log_view.log_store)
            for i in range(5):
                test_log.debug('Test log %s', i)

        self.assertEqual(log_view.get_total_count(), 5)
        self.assertEqual(log_view.get_current_line(), 0)
        # Turn follow on
        log_view.toggle_follow()
        self.assertTrue(log_view.follow)

        # Log another messagse, current_line should move to the last.
        with self.assertLogs(test_log, level='DEBUG') as _log_context:
            test_log.addHandler(log_view.log_store)
            test_log.debug('Test log')

        self.assertEqual(log_view.get_total_count(), 6)
        self.assertEqual(log_view.get_current_line(), 5)

    @parameterized.expand([
        ('no logs',
         80, 10,  # window_width, window_height
         0, True,  # log_count, follow_enabled
         0, [0, -1]),  # expected_total logs, expected_indices
        ('logs shorter than window height',
         80, 10,  # window_width, window_height
         7, True,  # log_count, follow_enabled
         7, [0, 6]),  # expected_total logs, expected_indices
        ('logs larger than window height with follow on',
         80, 10,  # window_width, window_height
         20, True,  # log_count, follow_enabled
         10, [10, 19]),  # expected_total logs, expected_indices
        ('logs larger than window height with follow off',
         80, 10,  # window_width, window_height
         20, False,  # log_count, follow_enabled
         10, [0, 9]),  # expected_total logs, expected_indices
    ]) # yapf: disable
    def test_get_log_window_indices(
        self,
        _name,
        window_width,
        window_height,
        log_count,
        follow_enabled,
        expected_total,
        expected_indices,
    ) -> None:
        """Test get_log_window_indices() with various window sizes and log
        counts."""
        log_view, _pane = _create_log_view()

        if not follow_enabled:
            log_view.toggle_follow()

        # Make required number of log messages
        if log_count > 0:
            test_log = logging.getLogger('log_view.test')
            with self.assertLogs(test_log, level='DEBUG') as _log_context:
                test_log.addHandler(log_view.log_store)
                for i in range(log_count):
                    test_log.debug('Test log %s', i)

        # Get indices
        start_index, end_index = log_view.get_log_window_indices(
            available_width=window_width, available_height=window_height)

        self.assertEqual([start_index, end_index], expected_indices)

        # Number of logs should equal the height of the window
        self.assertEqual((end_index - start_index) + 1, expected_total)

    def test_scrolling(self) -> None:
        """Test all scrolling methods."""
        log_view, _pane = self._create_log_view_with_logs(log_count=100)
        # Page scrolling needs to know the current window height.
        log_view._window_height = 10  # pylint: disable=protected-access

        # Follow is on by default, current line should be at the end.
        self.assertEqual(log_view.get_current_line(), 99)

        # Move to the beginning.
        log_view.scroll_to_top()
        self.assertEqual(log_view.get_current_line(), 0)

        # Should not be able to scroll before the beginning.
        log_view.scroll_up()
        self.assertEqual(log_view.get_current_line(), 0)
        log_view.scroll_up_one_page()
        self.assertEqual(log_view.get_current_line(), 0)

        # Single and multi line movement.
        log_view.scroll_down()
        self.assertEqual(log_view.get_current_line(), 1)
        log_view.scroll(5)
        self.assertEqual(log_view.get_current_line(), 6)
        log_view.scroll_up()
        self.assertEqual(log_view.get_current_line(), 5)

        # Page down and up.
        log_view.scroll_down_one_page()
        self.assertEqual(log_view.get_current_line(), 15)
        log_view.scroll_up_one_page()
        self.assertEqual(log_view.get_current_line(), 5)

        # Move to the end.
        log_view.scroll_to_bottom()
        self.assertEqual(log_view.get_current_line(), 99)

        # Should not be able to scroll beyond the end.
        log_view.scroll_down()
        self.assertEqual(log_view.get_current_line(), 99)
        log_view.scroll_down_one_page()
        self.assertEqual(log_view.get_current_line(), 99)

        # Move up a bit.
        log_view.scroll(-5)
        self.assertEqual(log_view.get_current_line(), 94)

        # Simulate a mouse click to scroll.
        # Click 5 lines above current position.
        log_view.scroll_to_position(Point(0, -5))
        self.assertEqual(log_view.get_current_line(), 89)
        # Click 3 lines below current position.
        log_view.scroll_to_position(Point(0, 3))
        self.assertEqual(log_view.get_current_line(), 92)

        # Clicking if follow is enabled should not scroll.
        log_view.toggle_follow()
        self.assertTrue(log_view.follow)
        self.assertEqual(log_view.get_current_line(), 99)
        log_view.scroll_to_position(Point(0, -5))
        self.assertEqual(log_view.get_current_line(), 99)

    def test_render_content_and_cursor_position(self) -> None:
        """Test render_content results and get_cursor_position

        get_cursor_position() should return the correct position depending on
        what line is selected."""

        # Mock time to always return the same value.
        mock_time = MagicMock(  # type: ignore
            return_value=time.mktime(
                datetime(2021, 7, 13, 0, 0, 0).timetuple()))
        with patch('time.time', new=mock_time):
            log_view, log_pane = self._create_log_view_with_logs(log_count=4)

        # Mock needed LogPane functions that pull info from prompt_toolkit.
        log_pane.get_horizontal_scroll_amount = MagicMock(return_value=0)
        log_pane.current_log_pane_width = 30
        log_pane.current_log_pane_height = 10

        log_view.scroll_to_top()
        log_view.render_content()
        self.assertEqual(log_view.get_cursor_position(), Point(x=0, y=2))

        log_view.scroll_to_bottom()
        log_view.render_content()
        self.assertEqual(log_view.get_cursor_position(), Point(x=0, y=5))

        expected_line_cache = [
            [('', '\n')],
            [('', '\n')],
            [
                ('class:log-time', '20210713 00:00:00'),
                ('', '  '),
                ('class:log-level-10', 'DEBUG'),
                ('', '  '),
                ('', 'Test log 0'),
                ('', '\n')
            ],
            [
                ('class:log-time', '20210713 00:00:00'),
                ('', '  '),
                ('class:log-level-10', 'DEBUG'),
                ('', '  '),
                ('', 'Test log 1'),
                ('', '\n')
            ],
            [
                ('class:log-time', '20210713 00:00:00'),
                ('', '  '),
                ('class:log-level-10', 'DEBUG'),
                ('', '  '),
                ('', 'Test log 2'),
                ('', '\n')
            ],
            FormattedText([
                ('class:selected-log-line [SetCursorPosition]', ''),
                ('class:selected-log-line class:log-time', '20210713 00:00:00'),
                ('class:selected-log-line ', '  '),
                ('class:selected-log-line class:log-level-10', 'DEBUG'),
                ('class:selected-log-line ', '  '),
                ('class:selected-log-line ', 'Test log 3'),
                ('class:selected-log-line ', '                        \n')
            ]),
        ]  # yapf: disable

        self.assertEqual(expected_line_cache,
                         list(log_view._line_fragment_cache))  # pylint: disable=protected-access

    def test_clear_scrollback(self) -> None:
        """Test various functions with clearing log scrollback history."""
        # pylint: disable=protected-access
        # Create log_view with 4 logs
        starting_log_count = 4
        log_view, _pane = self._create_log_view_with_logs(
            log_count=starting_log_count)

        # Check setup is correct
        self.assertTrue(log_view.follow)
        self.assertEqual(log_view.get_current_line(), 3)
        self.assertEqual(log_view.get_total_count(), 4)
        self.assertEqual(
            list(log.record.message
                 for log in log_view._get_visible_log_lines()),
            ['Test log 0', 'Test log 1', 'Test log 2', 'Test log 3'])
        self.assertEqual(
            log_view.get_log_window_indices(available_width=80,
                                            available_height=10), (0, 3))

        # Clear scrollback
        log_view.clear_scrollback()
        # Follow is still on
        self.assertTrue(log_view.follow)
        self.assertEqual(log_view.hidden_line_count(), 4)
        # Current line index should stay the same
        self.assertEqual(log_view.get_current_line(), 3)
        # Total count should stay the same
        self.assertEqual(log_view.get_total_count(), 4)
        # No lines returned
        self.assertEqual(
            list(log.record.message
                 for log in log_view._get_visible_log_lines()), [])
        self.assertEqual(
            log_view.get_log_window_indices(available_width=80,
                                            available_height=10), (4, 3))

        # Add Log 4 more lines
        test_log = logging.getLogger('log_view.test')
        with self.assertLogs(test_log, level='DEBUG') as _log_context:
            test_log.addHandler(log_view.log_store)
            for i in range(4):
                test_log.debug('Test log %s', i + starting_log_count)

        # Current line
        self.assertEqual(log_view.hidden_line_count(), 4)
        self.assertEqual(log_view.get_last_log_line_index(), 7)
        self.assertEqual(log_view.get_current_line(), 7)
        self.assertEqual(log_view.get_total_count(), 8)
        # Only the last 4 logs should appear
        self.assertEqual(
            list(log.record.message
                 for log in log_view._get_visible_log_lines()),
            ['Test log 4', 'Test log 5', 'Test log 6', 'Test log 7'])
        # Window height == 2
        self.assertEqual(
            log_view.get_log_window_indices(available_width=80,
                                            available_height=2), (6, 7))
        # Window height == 10
        self.assertEqual(
            log_view.get_log_window_indices(available_width=80,
                                            available_height=10), (4, 7))

        log_view.scroll_to_top()
        self.assertEqual(log_view.get_current_line(), 4)
        log_view.scroll_to_bottom()
        self.assertEqual(log_view.get_current_line(), 7)
        # Turn follow back on
        log_view.toggle_follow()

        log_view.undo_clear_scrollback()
        # Current line and total are the same
        self.assertEqual(log_view.get_current_line(), 7)
        self.assertEqual(log_view.get_total_count(), 8)
        # All logs should appear
        self.assertEqual(
            list(log.record.message
                 for log in log_view._get_visible_log_lines()), [
                     'Test log 0', 'Test log 1', 'Test log 2', 'Test log 3',
                     'Test log 4', 'Test log 5', 'Test log 6', 'Test log 7'
                 ])
        self.assertEqual(
            log_view.get_log_window_indices(available_width=80,
                                            available_height=10), (0, 7))

        log_view.scroll_to_top()
        self.assertEqual(log_view.get_current_line(), 0)
        log_view.scroll_to_bottom()
        self.assertEqual(log_view.get_current_line(), 7)


if _PYTHON_3_8:
    from unittest import IsolatedAsyncioTestCase  # type: ignore # pylint: disable=no-name-in-module

    class TestLogViewFiltering(IsolatedAsyncioTestCase):  # pylint: disable=undefined-variable
        """Test LogView log filtering capabilities."""
        def _create_log_view_from_list(self, log_messages):
            log_view, log_pane = _create_log_view()

            test_log = logging.getLogger('log_view.test')
            with self.assertLogs(test_log, level='DEBUG') as _log_context:
                test_log.addHandler(log_view.log_store)
                for log, extra_arg in log_messages:
                    test_log.debug('%s', log, extra=extra_arg)

            return log_view, log_pane

        @parameterized.expand([
            (
                'regex filter',
                'log.*item',
                [
                    ('Log some item', dict()),
                    ('Log another item', dict()),
                    ('Some exception', dict()),
                ],
                [
                    'Log some item',
                    'Log another item',
                ],
                None,  # field
                False,  # invert
            ),
            (
                'regex filter with field',
                'earth',
                [
                    ('Log some item',
                    dict(extra_metadata_fields={'planet': 'Jupiter'})),
                    ('Log another item',
                    dict(extra_metadata_fields={'planet': 'Earth'})),
                    ('Some exception',
                    dict(extra_metadata_fields={'planet': 'Earth'})),
                ],
                [
                    'Log another item',
                    'Some exception',
                ],
                'planet',  # field
                False,  # invert
            ),
            (
                'regex filter with field inverted',
                'earth',
                [
                    ('Log some item',
                    dict(extra_metadata_fields={'planet': 'Jupiter'})),
                    ('Log another item',
                    dict(extra_metadata_fields={'planet': 'Earth'})),
                    ('Some exception',
                    dict(extra_metadata_fields={'planet': 'Earth'})),
                ],
                [
                    'Log some item',
                ],
                'planet',  # field
                True,  # invert
            ),
        ]) # yapf: disable
        async def test_log_filtering(
            self,
            _name,
            input_text,
            input_lines,
            expected_matched_lines,
            field=None,
            invert=False,
        ) -> None:
            """Test run log view filtering."""
            log_view, _log_pane = self._create_log_view_from_list(input_lines)
            self.assertEqual(log_view.get_total_count(), len(input_lines))

            log_view.new_search(input_text, invert=invert, field=field)
            log_view.apply_filter()
            await log_view.filter_existing_logs_task

            self.assertEqual(log_view.get_total_count(),
                             len(expected_matched_lines))
            self.assertEqual(
                [log.record.message for log in log_view.filtered_logs],
                expected_matched_lines)

            log_view.clear_filters()
            self.assertEqual(log_view.get_total_count(), len(input_lines))


if __name__ == '__main__':
    unittest.main()
