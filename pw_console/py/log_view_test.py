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
import unittest
from datetime import datetime
from unittest.mock import MagicMock, patch
from parameterized import parameterized  # type: ignore

from prompt_toolkit.data_structures import Point
from prompt_toolkit.formatted_text import FormattedText

from pw_console.log_view import LogView


def _create_log_view():
    log_pane = MagicMock()
    log_view = LogView(log_pane)
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
        self.assertEqual(log_view.get_cursor_position(), Point(x=0, y=0))
        log_view.scroll_to_bottom()
        log_view.render_content()
        self.assertEqual(log_view.get_cursor_position(), Point(x=0, y=3))

        expected_line_cache = [
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
                ('class:selected-log-line ', '  \n')
            ]),
        ]  # yapf: disable

        self.assertEqual(
            list(log_view._line_fragment_cache  # pylint: disable=protected-access
                 ),
            expected_line_cache)


if __name__ == '__main__':
    unittest.main()
