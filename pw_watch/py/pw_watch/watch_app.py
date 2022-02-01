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
from typing import Any
from pygments.lexers.special import TextLexer
from prompt_toolkit.application import Application
from prompt_toolkit.clipboard.pyperclip import PyperclipClipboard
from prompt_toolkit.document import Document
from prompt_toolkit.history import (
    FileHistory,
    History,
    ThreadedHistory,
)
from prompt_toolkit.key_binding import KeyBindings, merge_key_bindings
from prompt_toolkit.layout import (
    Dimension,
    DynamicContainer,
    FormattedTextControl,
    HSplit,
    Layout,
    Window,
)
from prompt_toolkit.lexers import PygmentsLexer
from prompt_toolkit.widgets import SearchToolbar, TextArea
from prompt_toolkit.styles import DynamicStyle, merge_styles
from pw_console.console_app import get_default_colordepth
from pw_console.console_prefs import ConsolePrefs
from pw_console.get_pw_console_app import PW_CONSOLE_APP_CONTEXTVAR
from pw_console.log_pane import LogPane
import pw_console.style
from pw_console.widgets import ToolbarButton, WindowPane, WindowPaneToolbar
from pw_console.window_manager import WindowManager
from pw_console.plugin_mixin import PluginMixin


class WatchWindowManager(WindowManager):
    def update_root_container_body(self):
        self.application.window_manager_container = self.create_root_container(
        )


class RunHistoryPane(WindowPane):
    """Pigweed Console plugin window pane.
    Accepts text input in one window split and displays the output in another.
    """
    def __init__(
        self,
        application: Any,
        pane_title: str = 'Run History',
    ):
        super().__init__(pane_title=pane_title, application=application)
        self.application = application
        self.search_field = SearchToolbar()  # For reverse search (ctrl-r)
        self.output_field = TextArea(
            style='class:output-field',
            text='pw_watch',
            focusable=True,
            focus_on_click=True,
            scrollbar=True,
            line_numbers=False,
            search_field=self.search_field,
            lexer=PygmentsLexer(TextLexer),
        )
        key_bindings = KeyBindings()

        self.output_field.control.key_bindings = key_bindings
        # Setup the bottom toolbar
        self.bottom_toolbar = WindowPaneToolbar(self)
        self.bottom_toolbar.add_button(
            ToolbarButton('Enter', 'Run Build', self.run_build))
        self.bottom_toolbar.add_button(
            ToolbarButton(description='Copy Output',
                          mouse_handler=self.copy_all_output))
        self.bottom_toolbar.add_button(
            ToolbarButton('q', 'Quit', self.application.quit))
        self.container = self._create_pane_container(
            self.output_field,
            self.search_field,
            self.bottom_toolbar,
        )

    # Trigger text input, same as hitting enter inside sefl.input_field.
    def run_build(self):
        self.application.run_build()

    def update_output(self, text: str):
        # Add text to output buffer.
        cursor_position = self.output_field.document.cursor_position
        if cursor_position > len(text):
            cursor_position = len(text)
        self.output_field.buffer.set_document(
            Document(text=text, cursor_position=cursor_position))

    def copy_selected_output(self):
        clipboard_data = self.output_field.buffer.copy_selection()
        self.application.application.clipboard.set_data(clipboard_data)

    def copy_all_output(self):
        self.application.application.clipboard.set_text(
            self.output_field.buffer.text)


class WatchApp(PluginMixin):
    """Pigweed Watch main window application."""

    # pylint: disable=R0902 # too many instance attributes
    def __init__(self, startup_args, event_handler, loggers):
        # watch startup args
        self.startup_args = startup_args
        self.event_handler = event_handler
        self.color_depth = get_default_colordepth()

        PW_CONSOLE_APP_CONTEXTVAR.set(self)
        self.prefs = ConsolePrefs(project_file=False, user_file=False)
        self.search_history_filename = self.prefs.search_history
        # History instance for search toolbars.
        self.search_history: History = ThreadedHistory(
            FileHistory(self.search_history_filename))

        self.window_manager = WatchWindowManager(self)

        pw_console.python_logging.setup_python_logging()
        #log pane
        log_pane = LogPane(application=self, pane_title='log pane')
        for logger in loggers:
            log_pane.add_log_handler(logger, level_name='INFO')
        # disable table view to allow teriminal based coloring
        log_pane.table_view = False

        self.run_history_pane = RunHistoryPane(self)

        # Bottom Window: pw watch log messages
        self.window_manager.add_pane(log_pane)
        # Top Window: Ninja build output
        self.window_manager.add_pane(self.run_history_pane)

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
        key_bindings = KeyBindings()

        @key_bindings.add('c-c')
        @key_bindings.add("q")
        def _quit(event):
            "Quit."
            loggers[0].info('Got quit signal; exiting...')
            event.app.exit()

        @key_bindings.add("enter")
        def _quit(_):
            "Rebuild."
            self.run_build()

        self.key_bindings = key_bindings
        self._current_theme = pw_console.style.generate_styles()
        self.application = Application(
            layout=Layout(self.root_container,
                          focused_element=self.run_history_pane),
            key_bindings=merge_key_bindings([
                self.key_bindings,
                self.window_manager.key_bindings,
            ]),
            enable_page_navigation_bindings=True,
            mouse_support=True,
            color_depth=self.color_depth,
            clipboard=PyperclipClipboard(),
            style=DynamicStyle(lambda: merge_styles([
                self._current_theme,
            ])),
            full_screen=True,
        )

        self.plugin_init(
            plugin_callback=self.check_stdout,
            plugin_callback_frequency=1.0,
            plugin_logger_name='pw_watch_stdout_checker',
        )

    def update_menu_items(self):
        # Required by the Window Manager Class.
        pass

    def redraw_ui(self):
        """Redraw the prompt_toolkit UI."""
        if hasattr(self, 'application'):
            # Thread safe way of sending a repaint trigger to the input event
            # loop.
            self.application.invalidate()

    def focus_on_container(self, pane):
        """Set application focus to a specific container."""
        self.application.layout.focus(pane)

    def run_build(self):
        """Manually trigger a rebuild."""
        self.event_handler.rebuild()

    def get_statusbar_text(self):
        return [('class:logo', 'Pigweed Watch'),
                ('class:theme-fg-cyan',
                 ' {} '.format(self.startup_args.get('build_directories')))]

    def quit(self):
        self.application.exit()

    def check_stdout(self) -> bool:
        if self.event_handler.current_stdout:
            self.run_history_pane.update_output(
                self.event_handler.current_stdout)
            return True
        return False

    def run(self):
        self.plugin_start()
        # Run the prompt_toolkit application
        self.application.run()
