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
import functools
from threading import Thread
from typing import Iterable, Optional

from prompt_toolkit.layout.menus import CompletionsMenu
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
from prompt_toolkit.key_binding import KeyBindings, merge_key_bindings
from ptpython.layout import CompletionVisualisation  # type: ignore
from ptpython.key_bindings import (  # type: ignore
    load_python_bindings, load_sidebar_bindings,
)

import pw_console.key_bindings
import pw_console.style
from pw_console.help_window import HelpWindow
from pw_console.log_pane import LogPane
from pw_console.pw_ptpython_repl import PwPtPythonRepl
from pw_console.repl_pane import ReplPane

_LOG = logging.getLogger(__package__)

# Fake logger for --test-mode
FAKE_DEVICE_LOGGER_NAME = 'fake_device.1'
_FAKE_DEVICE_LOG = logging.getLogger(FAKE_DEVICE_LOGGER_NAME)


class FloatingMessageBar(ConditionalContainer):
    """Floating message bar for showing status messages."""
    def __init__(self, application):
        super().__init__(
            FormattedTextToolbar(
                (lambda: application.message if application.message else []),
                style='class:toolbar_inactive',
            ),
            filter=Condition(
                lambda: application.message and application.message != ''))


class ConsoleApp:
    """The main ConsoleApp class containing the whole console."""

    # pylint: disable=too-many-instance-attributes
    def __init__(
        self,
        global_vars=None,
        local_vars=None,
        repl_startup_message=None,
        help_text=None,
        app_title=None,
    ):
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

        self.app_title = app_title if app_title else 'Pigweed Console'

        # Top title message
        self.message = [
            ('class:logo', self.app_title),
            ('class:menu-bar', ' | '),
            ('class:keybind', 'F1'),
            ('class:keyhelp', ':Help '),
            ('class:keybind', 'Ctrl-W'),
            ('class:keyhelp', ':Quit '),
        ]

        # Top level UI state toggles.
        self.show_help_window = False
        self.vertical_split = False
        self.load_theme()

        self.help_window = HelpWindow(self,
                                      preamble='Pigweed CLI v0.1',
                                      additional_help_text=help_text)
        # Used for tracking which pane was in focus before showing help window.
        self.last_focused_pane = None

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
            startup_message=repl_startup_message,
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
                    MenuItem(
                        'Themes',
                        children=[
                            MenuItem('Toggle Light/Dark',
                                     handler=self.toggle_light_theme),
                            MenuItem('-'),
                            MenuItem('UI: Default',
                                     handler=functools.partial(
                                         self.load_theme, 'dark')),
                            MenuItem('UI: High Contrast',
                                     handler=functools.partial(
                                         self.load_theme,
                                         'high-contrast-dark')),
                            MenuItem('-'),
                            MenuItem(
                                'Code: tomorrow-night',
                                functools.partial(
                                    self.pw_ptpython_repl.use_code_colorscheme,
                                    'tomorrow-night')),
                            MenuItem(
                                'Code: tomorrow-night-bright',
                                functools.partial(
                                    self.pw_ptpython_repl.use_code_colorscheme,
                                    'tomorrow-night-bright')),
                            MenuItem(
                                'Code: tomorrow-night-blue',
                                functools.partial(
                                    self.pw_ptpython_repl.use_code_colorscheme,
                                    'tomorrow-night-blue')),
                            MenuItem(
                                'Code: tomorrow-night-eighties',
                                functools.partial(
                                    self.pw_ptpython_repl.use_code_colorscheme,
                                    'tomorrow-night-eighties')),
                            MenuItem(
                                'Code: dracula',
                                functools.partial(
                                    self.pw_ptpython_repl.use_code_colorscheme,
                                    'dracula')),
                            MenuItem(
                                'Code: zenburn',
                                functools.partial(
                                    self.pw_ptpython_repl.use_code_colorscheme,
                                    'zenburn')),
                        ],
                    ),
                    MenuItem('Toggle Vertical/Horizontal Split',
                             handler=self.toggle_vertical_split),
                    MenuItem('-'),
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
        self.key_bindings = pw_console.key_bindings.create_key_bindings(self)

        # Create help window text based global key_bindings and active panes.
        self._update_help_window()

        # prompt_toolkit root container.
        self.root_container = MenuContainer(
            body=self._create_root_split(),
            menu_items=self.menu_items,
            floats=[
                # Top message bar
                Float(
                    content=FloatingMessageBar(self),
                    top=0,
                    right=0,
                    height=1,
                ),
                # Centered floating Help Window
                Float(
                    content=self.help_window,
                    top=2,
                    bottom=2,
                    # Callable to get width
                    width=self.help_window.content_width,
                ),
                # Completion menu that can overlap other panes since it lives in
                # the top level Float container.
                Float(
                    xcursor=True,
                    ycursor=True,
                    content=ConditionalContainer(
                        content=CompletionsMenu(
                            scroll_offset=(lambda: self.pw_ptpython_repl.
                                           completion_menu_scroll_offset),
                            max_height=16,
                        ),
                        # Only show our completion if ptpython's is disabled.
                        filter=Condition(lambda: self.pw_ptpython_repl.
                                         completion_visualisation ==
                                         CompletionVisualisation.NONE),
                    ),
                ),
            ],
        )

        # NOTE: ptpython stores it's completion menus in this HSplit:
        #
        # self.pw_ptpython_repl.__pt_container__()
        #   .children[0].children[0].children[0].floats[0].content.children
        #
        # Index 1 is a CompletionsMenu and is shown when:
        #   self.pw_ptpython_repl
        #     .completion_visualisation == CompletionVisualisation.POP_UP
        #
        # Index 2 is a MultiColumnCompletionsMenu and is shown when:
        #   self.pw_ptpython_repl
        #     .completion_visualisation == CompletionVisualisation.MULTI_COLUMN
        #

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
                load_sidebar_bindings(self.pw_ptpython_repl),
                self.key_bindings,
            ]),
            style=DynamicStyle(lambda: merge_styles([
                self._current_theme,
                # Include ptpython styles
                self.pw_ptpython_repl._current_style,  # pylint: disable=protected-access
            ])),
            style_transformation=self.pw_ptpython_repl.style_transformation,
            enable_page_navigation_bindings=True,
            full_screen=True,
            mouse_support=True,
        )

    def toggle_light_theme(self):
        """Toggle light and dark theme colors."""
        # Use ptpython's style_transformation to swap dark and light colors.
        self.pw_ptpython_repl.swap_light_and_dark = (
            not self.pw_ptpython_repl.swap_light_and_dark)

    def load_theme(self, theme_name=None):
        """Regenerate styles for the current theme_name."""
        self._current_theme = pw_console.style.generate_styles(theme_name)

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

    def _update_help_window(self):
        """Generate the help window text based on active pane keybindings."""
        # Add global mouse bindings to the help text.
        mouse_functions = {
            'Focus pane, menu or log line.': ['Click'],
            'Scroll current window.': ['Scroll wheel'],
        }

        self.help_window.add_custom_keybinds_help_text('Global Mouse',
                                                       mouse_functions)

        # Add global key bindings to the help text.
        self.help_window.add_keybind_help_text('Global', self.key_bindings)

        # Add activated plugin key bindings to the help text.
        for pane in self.active_panes:
            for key_bindings in pane.get_all_key_bindings():
                if isinstance(key_bindings, KeyBindings):
                    self.help_window.add_keybind_help_text(
                        pane.__class__.__name__, key_bindings)
                elif isinstance(key_bindings, dict):
                    self.help_window.add_custom_keybinds_help_text(
                        pane.__class__.__name__, key_bindings)

        self.help_window.generate_help_text()

    def _create_root_split(self):
        """Create a vertical or horizontal split container for all active
        panes."""
        if self.vertical_split:
            self.active_pane_split = VSplit(
                self.active_panes,
                # Add a vertical separator between each active window pane.
                padding=1,
                padding_char='│',
                padding_style='class:pane_separator',
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

    def focused_window(self):
        """Return the currently focused window."""
        return self.application.layout.current_window

    def toggle_help(self):
        """Toggle visibility of the help window."""
        # Toggle state variable.
        self.show_help_window = not self.show_help_window

        # Set the help window in focus.
        if self.show_help_window:
            self.last_focused_pane = self.focused_window()
            self.application.layout.focus(self.help_window)
        # Restore original focus.
        else:
            if self.last_focused_pane:
                self.application.layout.focus(self.last_focused_pane)
            self.last_focused_pane = None

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
            # TODO(tonymd): Add this in when testing log lines with included
            # linebreaks.
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
    repl_startup_message: Optional[str] = None,
    help_text: Optional[str] = None,
    app_title: Optional[str] = None,
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
              app_title='My Awesome Console',
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
    :param app_title: Custom title text displayed in the user interface.
    :type app_title: str, optional
    :param repl_startup_message: Custom text shown by default in the repl output
        pane.
    :type repl_startup_message: str, optional
    :param help_text: Custom text shown at the top of the help window before
        keyboard shortcuts.
    :type help_text: str, optional
    """
    console_app = ConsoleApp(
        global_vars=global_vars,
        local_vars=local_vars,
        repl_startup_message=repl_startup_message,
        help_text=help_text,
        app_title=app_title,
    )

    # Add loggers to the console app log pane.
    if loggers:
        for logger in loggers:
            console_app.add_log_handler(logger)

    # Start a thread for running user code.
    console_app.start_user_code_thread()

    # Start the prompt_toolkit UI app.
    asyncio.run(console_app.run(test_mode=test_mode), debug=test_mode)
