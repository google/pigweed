#!/usr/bin/env python
# Copyright 2022 The Pigweed Authors
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
""" Prompt toolkit application for pw watch. """

import asyncio
import logging
from pathlib import Path
import re
import sys
from typing import List, NoReturn, Optional

from prompt_toolkit.application import Application
from prompt_toolkit.clipboard.pyperclip import PyperclipClipboard
from prompt_toolkit.filters import Condition
from prompt_toolkit.history import (
    FileHistory,
    History,
    ThreadedHistory,
)
from prompt_toolkit.key_binding import (
    KeyBindings,
    KeyBindingsBase,
    merge_key_bindings,
)
from prompt_toolkit.layout import (
    Dimension,
    DynamicContainer,
    FormattedTextControl,
    HSplit,
    Layout,
    Window,
)
from prompt_toolkit.layout.controls import BufferControl
from prompt_toolkit.styles import DynamicStyle, merge_styles, Style

from pw_console.console_app import get_default_colordepth
from pw_console.console_prefs import ConsolePrefs
from pw_console.get_pw_console_app import PW_CONSOLE_APP_CONTEXTVAR
from pw_console.log_pane import LogPane
from pw_console.plugin_mixin import PluginMixin
import pw_console.style
from pw_console.window_manager import WindowManager

_NINJA_LOG = logging.getLogger('pw_watch_ninja_output')
_LOG = logging.getLogger('pw_watch')


class WatchWindowManager(WindowManager):
    def update_root_container_body(self):
        self.application.window_manager_container = (
            self.create_root_container())


class WatchApp(PluginMixin):
    """Pigweed Watch main window application."""
    # pylint: disable=too-many-instance-attributes

    NINJA_FAILURE_TEXT = '\033[31mFAILED: '

    NINJA_BUILD_STEP = re.compile(
        r"^\[(?P<step>[0-9]+)/(?P<total_steps>[0-9]+)\] (?P<action>.*)$")

    def __init__(self,
                 event_handler,
                 debug_logging: bool = False,
                 log_file_name: Optional[str] = None):

        self.event_handler = event_handler

        self.external_logfile: Optional[Path] = (Path(log_file_name)
                                                 if log_file_name else None)
        self.color_depth = get_default_colordepth()

        # Necessary for some of pw_console's window manager features to work
        # such as mouse drag resizing.
        PW_CONSOLE_APP_CONTEXTVAR.set(self)  # type: ignore

        theme_name = 'ansi'
        self.prefs = ConsolePrefs(project_file=False, user_file=False)
        self.prefs.set_ui_theme(theme_name)

        key_bindings = KeyBindings()

        @key_bindings.add('c-c', filter=self.input_box_not_focused())
        def _quit(_event):
            "Quit."
            _LOG.info('Got quit signal; exiting...')
            self.exit(0)

        @key_bindings.add('enter', filter=self.input_box_not_focused())
        def _run_build(_event):
            "Rebuild."
            self.run_build()

        self.search_history_filename = self.prefs.search_history
        # History instance for search toolbars.
        self.search_history: History = ThreadedHistory(
            FileHistory(str(self.search_history_filename)))

        self.window_manager = WatchWindowManager(self)

        pw_console.python_logging.setup_python_logging()

        self._build_error_count = 0
        self._errors_in_output = False

        self.ninja_log_pane = LogPane(application=self,
                                      pane_title='Pigweed Watch')
        self.ninja_log_pane.add_log_handler(_NINJA_LOG, level_name='INFO')
        self.ninja_log_pane.add_log_handler(
            _LOG, level_name=('DEBUG' if debug_logging else 'INFO'))
        # Set python log format to just the message itself.
        self.ninja_log_pane.log_view.log_store.formatter = logging.Formatter(
            '%(message)s')
        self.ninja_log_pane.table_view = False
        # Enable line wrapping
        self.ninja_log_pane.toggle_wrap_lines()
        # Blank right side toolbar text
        self.ninja_log_pane._pane_subtitle = ' '
        self.ninja_log_view = self.ninja_log_pane.log_view

        # Make tab and shift-tab search for next and previous error
        next_error_bindings = KeyBindings()

        @next_error_bindings.add('s-tab')
        def _previous_error(_event):
            self.jump_to_error(backwards=True)

        @next_error_bindings.add('tab')
        def _next_error(_event):
            self.jump_to_error()

        existing_log_bindings: Optional[KeyBindingsBase] = (
            self.ninja_log_pane.log_content_control.key_bindings)

        key_binding_list: List[KeyBindingsBase] = []
        if existing_log_bindings:
            key_binding_list.append(existing_log_bindings)
        key_binding_list.append(next_error_bindings)
        self.ninja_log_pane.log_content_control.key_bindings = (
            merge_key_bindings(key_binding_list))

        self.window_manager.add_pane(self.ninja_log_pane)

        self.window_manager_container = (
            self.window_manager.create_root_container())

        self.root_container = HSplit([
            # The top toolbar.
            Window(
                content=FormattedTextControl(self.get_statusbar_text),
                height=Dimension.exact(1),
                style='',
            ),
            # The main content.
            DynamicContainer(lambda: self.window_manager_container),
        ])

        self.key_bindings = merge_key_bindings([
            self.window_manager.key_bindings,
            key_bindings,
        ])

        self.current_theme = pw_console.style.generate_styles(theme_name)
        self.current_theme = merge_styles([
            self.current_theme,
            Style.from_dict({'search': 'bg:ansired ansiblack'}),
        ])

        self.application: Application = Application(
            layout=Layout(self.root_container,
                          focused_element=self.ninja_log_pane),
            key_bindings=self.key_bindings,
            mouse_support=True,
            color_depth=self.color_depth,
            clipboard=PyperclipClipboard(),
            style=DynamicStyle(lambda: merge_styles([
                self.current_theme,
            ])),
            full_screen=True,
        )

        self.plugin_init(
            plugin_callback=self.check_build_status,
            plugin_callback_frequency=1.0,
            plugin_logger_name='pw_watch_stdout_checker',
        )

    def jump_to_error(self, backwards: bool = False) -> None:
        if not self.ninja_log_pane.log_view.search_text:
            self.ninja_log_pane.log_view.set_search_regex(
                '^FAILED: ', False, None)
        if backwards:
            self.ninja_log_pane.log_view.search_backwards()
        else:
            self.ninja_log_pane.log_view.search_forwards()
        self.ninja_log_pane.log_view.log_screen.reset_logs(
            log_index=self.ninja_log_pane.log_view.log_index)

        self.ninja_log_pane.log_view.move_selected_line_to_top()

    def update_menu_items(self):
        """Required by the Window Manager Class."""

    def redraw_ui(self):
        """Redraw the prompt_toolkit UI."""
        if hasattr(self, 'application'):
            self.application.invalidate()

    def focus_on_container(self, pane):
        """Set application focus to a specific container."""
        self.application.layout.focus(pane)

    def clear_ninja_log(self) -> None:
        self.ninja_log_view.log_store.clear_logs()
        self.ninja_log_view._restart_filtering()  # pylint: disable=protected-access
        self.ninja_log_view.view_mode_changed()

    def run_build(self):
        """Manually trigger a rebuild."""
        self.clear_ninja_log()
        self.event_handler.rebuild()

    def get_statusbar_text(self):
        status = self.event_handler.status_message
        fragments = [('class:logo', 'Pigweed Watch')]
        is_building = False
        if status:
            fragments = [status]
            is_building = status[1] == 'Building'
        separator = ('', '  ')

        if is_building:
            percent = self.event_handler.current_build_percent
            percent *= 100
            fragments.append(separator)
            fragments.append(('ansicyan', '{:.0f}%'.format(percent)))

        if self.event_handler.current_build_errors > 0:
            fragments.append(separator)
            fragments.append(('', 'Errors:'))
            fragments.append(
                ('ansired', str(self.event_handler.current_build_errors)))

        if is_building:
            fragments.append(separator)
            fragments.append(('', self.event_handler.current_build_step))

        return fragments

    def exit(self, exit_code: int) -> None:
        log_file = self.external_logfile

        def _really_exit(future: asyncio.Future) -> NoReturn:
            if log_file:
                # Print a message showing where logs were saved to.
                print('Logs saved to: {}'.format(log_file.resolve()))
            sys.exit(future.result())

        if self.application.future:
            self.application.future.add_done_callback(_really_exit)
        self.application.exit(result=exit_code)

    def check_build_status(self) -> bool:
        if not self.event_handler.current_stdout:
            return False

        if self._errors_in_output:
            return True

        if self.event_handler.current_build_errors > self._build_error_count:
            self._errors_in_output = True
            self.jump_to_error()

        return True

    def run(self):
        self.plugin_start()
        # Run the prompt_toolkit application
        self.application.run(set_exception_handler=True)

    def input_box_not_focused(self) -> Condition:
        """Condition checking the focused control is not a text input field."""
        @Condition
        def _test() -> bool:
            """Check if the currently focused control is an input buffer.

            Returns:
                bool: True if the currently focused control is not a text input
                    box. For example if the user presses enter when typing in
                    the search box, return False.
            """
            return not isinstance(self.application.layout.current_control,
                                  BufferControl)

        return _test
