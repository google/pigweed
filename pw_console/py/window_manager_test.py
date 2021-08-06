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


def _create_console_app(logger_count=2):
    console_app = ConsoleApp(color_depth=ColorDepth.DEPTH_8_BIT)

    loggers = {}
    for i in range(logger_count):
        loggers['Log{}'.format(i)] = [
            logging.getLogger('test_log{}'.format(i))
        ]
    for window_title, logger_instances in loggers.items():
        console_app.add_log_handler(window_title, logger_instances)
    return console_app


def _window_list_sizes(window_manager):
    return [
        window_list.width.weight for window_list in window_manager.window_lists
    ]


def _window_pane_sizes(window_manager, window_list_index=0):
    return [
        pane.height.weight
        for pane in window_manager.window_lists[window_list_index].active_panes
    ]


def _window_pane_counts(window_manager):
    return [
        len(window_list.active_panes)
        for window_list in window_manager.window_lists
    ]


def _window_pane_titles(window_manager):
    return [[
        pane.pane_title() + ' - ' + pane.pane_subtitle()
        for pane in window_list.active_panes
    ] for window_list in window_manager.window_lists]


def _target_list_and_pane(window_manager, list_index, pane_index):
    # pylint: disable=protected-access
    # Bypass prompt_toolkit has_focus()
    window_manager._get_active_window_list_and_pane = (
        MagicMock(  # type: ignore
            return_value=(
                window_manager.window_lists[list_index],
                window_manager.window_lists[list_index].
                active_panes[pane_index],
            )))


class TestWindowManager(unittest.TestCase):
    # pylint: disable=protected-access
    """Tests for window management functions."""
    def setUp(self):
        self.maxDiff = None  # pylint: disable=invalid-name

    def test_find_window_list_and_pane(self) -> None:
        """Test getting the window list for a given pane."""
        with create_app_session(output=FakeOutput()):
            console_app = _create_console_app(logger_count=3)

            window_manager = console_app.window_manager
            self.assertEqual(_window_pane_counts(window_manager), [4])

            # Move 2 windows to the right into their own splits
            _target_list_and_pane(window_manager, 0, 0)
            window_manager.move_pane_right()
            _target_list_and_pane(window_manager, 0, 0)
            window_manager.move_pane_right()
            _target_list_and_pane(window_manager, 1, 0)
            window_manager.move_pane_right()
            # 3 splits, first split has 2 windows
            self.assertEqual(_window_pane_counts(window_manager), [2, 1, 1])

            # Move the first window in the first split left
            _target_list_and_pane(window_manager, 0, 0)
            window_manager.move_pane_left()
            # 4 splits, each with their own window
            self.assertEqual(_window_pane_counts(window_manager), [1, 1, 1, 1])

            # Move the first window to the right
            _target_list_and_pane(window_manager, 0, 0)
            window_manager.move_pane_right()
            # 3 splits, first split has 2 windows
            self.assertEqual(_window_pane_counts(window_manager), [2, 1, 1])

            target_pane = window_manager.window_lists[2].active_panes[0]

            result_window_list, result_pane_index = (
                window_manager._find_window_list_and_pane_index(target_pane))
            self.assertEqual(
                (window_manager.window_lists[2], 0),
                (result_window_list, result_pane_index),
            )
            window_manager.remove_pane(target_pane)
            self.assertEqual(_window_pane_counts(window_manager), [2, 1])

    def test_window_list_moving_and_resizing(self) -> None:
        """Test window split movement resizing."""
        with create_app_session(output=FakeOutput()):
            console_app = _create_console_app(logger_count=3)

            window_manager = console_app.window_manager

            _target_list_and_pane(window_manager, 0, 0)
            # Should have one window list split of size 50.
            self.assertEqual(_window_list_sizes(window_manager), [50])

            # Move one pane to the right, creating a new window_list split.
            window_manager.move_pane_right()
            self.assertEqual(_window_list_sizes(window_manager), [50, 50])

            # Move another pane to the right twice, creating a third
            # window_list split.
            _target_list_and_pane(window_manager, 0, 0)
            window_manager.move_pane_right()

            # Above window pane is at a new location
            _target_list_and_pane(window_manager, 1, 0)
            window_manager.move_pane_right()

            # Should have 3 splits now
            self.assertEqual(_window_list_sizes(window_manager), [50, 50, 50])

            # Total of 4 active panes
            self.assertEqual(len(list(window_manager.active_panes())), 4)

            # Target the middle split
            _target_list_and_pane(window_manager, 1, 0)
            # Shrink the middle split twice
            window_manager.shrink_split()
            window_manager.shrink_split()
            self.assertEqual(_window_list_sizes(window_manager), [50, 46, 54])

            # Target the first split
            _target_list_and_pane(window_manager, 0, 0)
            # Shrink the first split once
            window_manager.shrink_split()
            self.assertEqual(_window_list_sizes(window_manager), [48, 48, 54])

            # Target the third (last) split
            _target_list_and_pane(window_manager, 2, 0)

            # Shrink the third split once
            window_manager.shrink_split()
            self.assertEqual(_window_list_sizes(window_manager), [48, 50, 52])

            # Enlarge the third split a few times.
            window_manager.enlarge_split()
            window_manager.enlarge_split()
            window_manager.enlarge_split()
            self.assertEqual(_window_list_sizes(window_manager), [48, 44, 58])

            # Target the middle split
            _target_list_and_pane(window_manager, 1, 0)
            # Move the middle window pane left
            window_manager.move_pane_left()
            # Middle split should be removed
            self.assertEqual(_window_list_sizes(window_manager), [48, 58])

            # Revert sizes to default
            window_manager.reset_split_sizes()
            self.assertEqual(_window_list_sizes(window_manager), [50, 50])

    def test_get_pane_titles(self) -> None:
        """Test window resizing."""
        with create_app_session(output=FakeOutput()):
            console_app = _create_console_app(logger_count=3)

            window_manager = console_app.window_manager
            list_pane_titles = [
                # Remove mouse click handler partials in tup[2]
                [(tup[0], tup[1]) for tup in window_list.get_pane_titles()]
                for window_list in window_manager.window_lists
            ]
            self.assertEqual(
                list_pane_titles[0],
                [('', ' '), ('class:window-tab-inactive', ' Log2 test_log2 '),
                 ('', ' '), ('class:window-tab-inactive', ' Log1 test_log1 '),
                 ('', ' '), ('class:window-tab-inactive', ' Log0 test_log0 '),
                 ('', ' '), ('class:window-tab-inactive', ' Python Repl  '),
                 ('', ' ')],
            )

    def test_window_pane_movement_resizing(self) -> None:
        """Test window resizing."""
        with create_app_session(output=FakeOutput()):
            console_app = _create_console_app(logger_count=3)

            window_manager = console_app.window_manager

            # 4 panes, 3 for the loggers and 1 for the repl.
            self.assertEqual(
                len(window_manager.first_window_list().active_panes), 4)

            def target_window_pane(index: int):
                # Bypass prompt_toolkit has_focus()
                window_manager._get_active_window_list_and_pane = (
                    MagicMock(  # type: ignore
                        return_value=(
                            window_manager.window_lists[0],
                            window_manager.window_lists[0].active_panes[index],
                        )))
                window_list = console_app.window_manager.first_window_list()
                window_list.get_current_active_pane = (
                    MagicMock(  # type: ignore
                        return_value=window_list.active_panes[index]))

            # Target the first window pane
            target_window_pane(0)

            # Shrink the first pane
            window_manager.shrink_pane()
            self.assertEqual(_window_pane_sizes(window_manager),
                             [48, 52, 50, 50])

            # Reset pane sizes
            window_manager.reset_pane_sizes()
            self.assertEqual(_window_pane_sizes(window_manager),
                             [50, 50, 50, 50])

            # Shrink last pane
            target_window_pane(3)

            window_manager.shrink_pane()
            self.assertEqual(_window_pane_sizes(window_manager),
                             [50, 50, 52, 48])

            # Enlarge second pane
            target_window_pane(1)

            window_manager.enlarge_pane()
            window_manager.enlarge_pane()
            self.assertEqual(_window_pane_sizes(window_manager),
                             [50, 54, 48, 48])

            # Check window pane ordering
            self.assertEqual(
                _window_pane_titles(window_manager),
                [
                    [
                        'Log2 - test_log2',
                        'Log1 - test_log1',
                        'Log0 - test_log0',
                        'Python Repl - ',
                    ],
                ],
            )

            target_window_pane(0)
            window_manager.move_pane_down()
            self.assertEqual(
                _window_pane_titles(window_manager),
                [
                    [
                        'Log1 - test_log1',
                        'Log2 - test_log2',
                        'Log0 - test_log0',
                        'Python Repl - ',
                    ],
                ],
            )
            target_window_pane(2)
            window_manager.move_pane_up()
            target_window_pane(1)
            window_manager.move_pane_up()
            self.assertEqual(
                _window_pane_titles(window_manager),
                [
                    [
                        'Log0 - test_log0',
                        'Log1 - test_log1',
                        'Log2 - test_log2',
                        'Python Repl - ',
                    ],
                ],
            )
            target_window_pane(0)
            window_manager.move_pane_up()
            self.assertEqual(
                _window_pane_titles(window_manager),
                [
                    [
                        'Log0 - test_log0',
                        'Log1 - test_log1',
                        'Log2 - test_log2',
                        'Python Repl - ',
                    ],
                ],
            )


if __name__ == '__main__':
    unittest.main()
