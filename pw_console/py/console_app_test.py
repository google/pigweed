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
"""Tests for pw_console.console_app"""

import logging
import unittest
from unittest.mock import MagicMock

from prompt_toolkit.application import create_app_session
from prompt_toolkit.output import ColorDepth
# inclusive-language: ignore
from prompt_toolkit.output import DummyOutput as FakeOutput

from pw_console.console_app import ConsoleApp


class TestConsoleApp(unittest.TestCase):
    """Tests for ConsoleApp."""
    def test_instantiate(self) -> None:
        """Test init."""
        with create_app_session(output=FakeOutput()):
            console_app = ConsoleApp(color_depth=ColorDepth.DEPTH_8_BIT)
            self.assertIsNotNone(console_app)

    def test_window_resizing(self) -> None:
        """Test window resizing."""
        # pylint: disable=protected-access
        with create_app_session(output=FakeOutput()):
            console_app = ConsoleApp(color_depth=ColorDepth.DEPTH_8_BIT)

            loggers = {
                'Log1': [logging.getLogger('test_log1')],
                'Log2': [logging.getLogger('test_log2')],
                'Log3': [logging.getLogger('test_log3')],
            }
            for window_title, logger_instances in loggers.items():
                console_app.add_log_handler(window_title, logger_instances)

            # 4 panes, 3 for the loggers and 1 for the repl.
            self.assertEqual(len(console_app.window_manager.active_panes), 4)

            # Bypass prompt_toolkit has_focus()
            console_app.window_manager._get_current_active_pane = (
                MagicMock(  # type: ignore
                    return_value=console_app.window_manager.active_panes[0]))

            # Shrink the first pane
            console_app.window_manager.shrink_pane()
            self.assertEqual([
                pane.height.weight
                for pane in console_app.window_manager.active_panes
            ], [48, 52, 50, 50])

            # Reset pane sizes
            console_app.window_manager.reset_pane_sizes()
            self.assertEqual([
                pane.height.weight
                for pane in console_app.window_manager.active_panes
            ], [50, 50, 50, 50])

            # Shrink last pane
            console_app.window_manager._get_current_active_pane = (
                MagicMock(  # type: ignore
                    return_value=console_app.window_manager.active_panes[3]))
            console_app.window_manager.shrink_pane()
            self.assertEqual([
                pane.height.weight
                for pane in console_app.window_manager.active_panes
            ], [50, 50, 52, 48])

            # Enlarge second pane
            console_app.window_manager._get_current_active_pane = (
                MagicMock(  # type: ignore
                    return_value=console_app.window_manager.active_panes[1]))
            console_app.window_manager.enlarge_pane()
            console_app.window_manager.enlarge_pane()
            self.assertEqual([
                pane.height.weight
                for pane in console_app.window_manager.active_panes
            ], [50, 54, 48, 48])

    def test_multiple_loggers_in_one_pane(self) -> None:
        """Test window resizing."""
        # pylint: disable=protected-access
        with create_app_session(output=FakeOutput()):
            console_app = ConsoleApp(color_depth=ColorDepth.DEPTH_8_BIT)

            loggers = {
                'Logs': [
                    logging.getLogger('test_log1'),
                    logging.getLogger('test_log2'),
                    logging.getLogger('test_log3'),
                ]
            }
            for window_title, logger_instances in loggers.items():
                console_app.add_log_handler(window_title, logger_instances)

            # Two panes, one for the loggers and one for the repl.
            self.assertEqual(len(console_app.window_manager.active_panes), 2)

            self.assertEqual(
                console_app.window_manager.active_panes[0].pane_title(),
                'Logs')
            self.assertEqual(
                console_app.window_manager.active_panes[0]._pane_subtitle,
                'test_log1, test_log2, test_log3')
            self.assertEqual(
                console_app.window_manager.active_panes[0].pane_subtitle(),
                'test_log1 + 3 more')


if __name__ == '__main__':
    unittest.main()
