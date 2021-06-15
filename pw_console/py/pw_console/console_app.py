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
"""ConsoleApp control class."""

import builtins
import asyncio
import logging
from threading import Thread
from typing import Iterable, Optional

from prompt_toolkit.application import Application
from prompt_toolkit.filters import Condition
from prompt_toolkit.styles import (
    DynamicStyle,
    merge_styles,
)
from prompt_toolkit.layout import (
    ConditionalContainer,
    Float,
    HSplit,
    Layout,
    VSplit,
)
from prompt_toolkit.widgets import FormattedTextToolbar
from prompt_toolkit.widgets import (
    MenuContainer,
    MenuItem,
)
from prompt_toolkit.key_binding import merge_key_bindings
from ptpython.key_bindings import load_python_bindings  # type: ignore

from pw_console.help_window import HelpWindow
from pw_console.key_bindings import create_key_bindings
from pw_console.log_pane import LogPane
from pw_console.pw_ptpython_repl import PwPtPythonRepl
from pw_console.repl_pane import ReplPane
from pw_console.style import pw_console_styles

_LOG = logging.getLogger(__package__)

# Fake logger for --test-mode
FAKE_DEVICE_LOGGER_NAME = 'fake_device.1'
_FAKE_DEVICE_LOG = logging.getLogger(FAKE_DEVICE_LOGGER_NAME)


class FloatingMessageBar(ConditionalContainer):
    """Floating message bar for showing status messages."""
    def __init__(self, application):
        super().__init__(
            FormattedTextToolbar(lambda: application.message
                                 if application.message else []),
            filter=Condition(
                lambda: application.message and application.message != ''))


class ConsoleApp:
    """The main ConsoleApp class containing the whole console."""

    # pylint: disable=too-many-instance-attributes
    def __init__(self, global_vars=None, local_vars=None):
        # Create a default global and local symbol table. Values are the same
        # structure as what is returned by globals():
        #   https://docs.python.org/3/library/functions.html#globals
        if global_vars is None:
            global_vars = {
                '__name__': '__main__',
                '__package__': None,
                '__doc__': None,
                '__builtins__': builtins,
            }

        local_vars = local_vars or global_vars

        # Event loop for executing user repl code.
        self.user_code_loop = asyncio.new_event_loop()

        # Top title message
        # 'Pigweed CLI v0.1 | Mouse supported | F1:Help Ctrl-W:Quit.'
        self.message = [
            ('class:logo', ' Pigweed CLI v0.1 '),
            ('class:menu-bar', '| Mouse supported; click on pane to focus | '),
            ('class:keybind', 'F1'),
            ('class:keyhelp', ':Help '),
            ('class:keybind', 'Ctrl-W'),
            ('class:keyhelp', ':Quit '),
        ]

        # Top level UI state toggles.
        self.show_help_window = False
        self.vertical_split = False

        # Create one log pane.
        self.log_pane = LogPane(application=self)

        # Create a ptpython repl instance.
        self.pw_ptpython_repl = PwPtPythonRepl(
            get_globals=lambda: global_vars,
            get_locals=lambda: local_vars,
        )

        self.repl_pane = ReplPane(
            application=self,
            python_repl=self.pw_ptpython_repl,
        )

        # List of enabled panes.
        self.active_panes = [
            self.log_pane,
            self.repl_pane,
        ]

        # Top of screen menu items
        self.menu_items = [
            # File menu
            MenuItem(
                '[File] ',
                children=[
                    MenuItem('Exit', handler=self.exit_console),
                ],
            ),
            # View menu
            MenuItem(
                '[View] ',
                children=[
                    MenuItem('Toggle Vertical/Horizontal Split',
                             handler=self.toggle_vertical_split),
                    MenuItem('Toggle Log line Wrapping',
                             handler=self.toggle_log_line_wrapping),
                ],
            ),
            # Info / Help
            MenuItem(
                '[Help] ',
                children=[
                    MenuItem('Keyboard Shortcuts', handler=self.toggle_help),
                ],
            ),
        ]

        # Key bindings registry.
        self.key_bindings = create_key_bindings(self)

        # prompt_toolkit root container.
        self.root_container = MenuContainer(
            body=self._create_root_split(),
            menu_items=self.menu_items,
            floats=[
                # Top message bar
                Float(top=0,
                      right=0,
                      height=1,
                      content=FloatingMessageBar(self)),
                # Centered floating Help Window
                Float(content=self._create_help_window()),
            ],
        )

        # Setup the prompt_toolkit layout with the repl pane as the initially
        # focused element.
        self.layout: Layout = Layout(
            self.root_container,
            focused_element=self.repl_pane,
        )

        # Create the prompt_toolkit Application instance.
        self.application: Application = Application(
            layout=self.layout,
            after_render=self.run_after_render_hooks,
            key_bindings=merge_key_bindings([
                # Pull key bindings from ptpython
                load_python_bindings(self.pw_ptpython_repl),
                self.key_bindings,
            ]),
            style=DynamicStyle(lambda: merge_styles([
                pw_console_styles,
                # Include ptpython styles
                self.pw_ptpython_repl._current_style,  # pylint: disable=protected-access
            ])),
            enable_page_navigation_bindings=True,
            full_screen=True,
            mouse_support=True,
        )

    def add_log_handler(self, logger_instance: logging.Logger):
        """Add the Log pane as a handler for this logger instance."""
        logger_instance.addHandler(self.log_pane.log_container)

    def _user_code_thread_entry(self):
        """Entry point for the user code thread."""
        asyncio.set_event_loop(self.user_code_loop)
        self.user_code_loop.run_forever()

    def run_after_render_hooks(self, *unused_args, **unused_kwargs):
        """Run each active pane's `after_render_hook` if defined."""
        for pane in self.active_panes:
            if hasattr(pane, 'after_render_hook'):
                pane.after_render_hook()

    def start_user_code_thread(self):
        """Create a thread for running user code so the UI isn't blocked."""
        thread = Thread(target=self._user_code_thread_entry,
                        args=(),
                        daemon=True)
        thread.start()

    def _create_help_window(self):
        help_window = HelpWindow(self)
        # Create the help window and generate help text.
        # Add global key bindings to the help text
        help_window.add_keybind_help_text('Global', self.key_bindings)
        # Add activated plugin key bindings to the help text
        for pane in self.active_panes:
            for key_bindings in pane.get_all_key_bindings():
                help_window.add_keybind_help_text(pane.__class__.__name__,
                                                  key_bindings)
        help_window.generate_help_text()
        return help_window

    def _create_root_split(self):
        """Create a vertical or horizontal split container for all active
        panes."""
        if self.vertical_split:
            self.active_pane_split = VSplit(
                self.active_panes,
                # Add a vertical separator between each active window pane.
                padding=1,
                padding_char='│',
                padding_style='',
            )
        else:
            self.active_pane_split = HSplit(self.active_panes)

        return HSplit([
            self.active_pane_split,
        ])

    def toggle_log_line_wrapping(self):
        """Menu item handler to toggle line wrapping of the first log pane."""
        self.log_pane.toggle_wrap_lines()

    def toggle_vertical_split(self):
        """Toggle visibility of the help window."""
        self.vertical_split = not self.vertical_split

        # Replace the root MenuContainer body with the new split.
        self.root_container.container.content.children[
            1] = self._create_root_split()

        self.redraw_ui()

    def toggle_help(self):
        """Toggle visibility of the help window."""
        self.show_help_window = not self.show_help_window

    def exit_console(self):
        """Quit the console prompt_toolkit application UI."""
        self.application.exit()

    def redraw_ui(self):
        """Redraw the prompt_toolkit UI."""
        self.application.invalidate()

    async def run(self, test_mode=False):
        """Start the prompt_toolkit UI."""
        if test_mode:
            background_log_task = asyncio.create_task(self.log_forever())

        try:
            unused_result = await self.application.run_async(
                set_exception_handler=True)
        finally:
            if test_mode:
                background_log_task.cancel()

    async def log_forever(self):
        """Test mode async log generator coroutine that runs forever."""
        message_count = 0
        # Sample log lines:
        # Log message [=         ] # 291
        # Log message [ =        ] # 292
        # Log message [  =       ] # 293
        # Log message [   =      ] # 294
        # Log message [    =     ] # 295
        # Log message [     =    ] # 296
        # Log message [      =   ] # 297
        # Log message [       =  ] # 298
        # Log message [        = ] # 299
        # Log message [         =] # 300
        while True:
            await asyncio.sleep(2)
            bar_size = 10
            position = message_count % bar_size
            bar_content = " " * (bar_size - position - 1) + "="
            if position > 0:
                bar_content = "=".rjust(position) + " " * (bar_size - position)
            new_log_line = 'Log message [{}] # {}'.format(
                bar_content, message_count)
            if message_count % 10 == 0:
                new_log_line += (" Lorem ipsum dolor sit amet, consectetur "
                                 "adipiscing elit.") * 8
            # TODO: Test log lines that include linebreaks.
            # if message_count % 11 == 0:
            #     new_log_line += inspect.cleandoc(""" [PYTHON] START
            #         In []: import time;
            #                 def t(s):
            #                     time.sleep(s)
            #                     return 't({}) seconds done'.format(s)""")

            message_count += 1
            _FAKE_DEVICE_LOG.info(new_log_line)


def embed(
    global_vars=None,
    local_vars=None,
    loggers: Optional[Iterable[logging.Logger]] = None,
    test_mode=False,
) -> None:
    """Call this to embed pw console at the call point within your program.
    It's similar to `ptpython.embed` and `IPython.embed`. ::

        import logging

        from pw_console.console_app import embed

        embed(global_vars=globals(),
              local_vars=locals(),
              loggers=[
                  logging.getLogger(__package__),
                  logging.getLogger('device logs'),
              ],
        )

    :param global_vars: Dictionary representing the desired global symbol
        table. Similar to what is returned by `globals()`.
    :type global_vars: dict, optional
    :param local_vars: Dictionary representing the desired local symbol
        table. Similar to what is returned by `locals()`.
    :type local_vars: dict, optional
    :param loggers: List of `logging.getLogger()` instances that should be shown
        in the pw console log pane user interface.
    :type loggers: list, optional
    """
    console_app = ConsoleApp(
        global_vars=global_vars,
        local_vars=local_vars,
    )

    # Add loggers to the console app log pane.
    if loggers:
        for logger in loggers:
            console_app.add_log_handler(logger)

    # TODO: Start prompt_toolkit app here
    _LOG.debug('Pigweed Console Start')

    # Start a thread for running user code.
    console_app.start_user_code_thread()

    # Start the prompt_toolkit UI app.
    asyncio.run(console_app.run(test_mode=test_mode), debug=test_mode)
