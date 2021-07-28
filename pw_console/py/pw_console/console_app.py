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
from pathlib import Path
from threading import Thread
from typing import Iterable, Union

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
    Layout,
)
from prompt_toolkit.widgets import FormattedTextToolbar
from prompt_toolkit.widgets import (
    MenuContainer,
    MenuItem,
)
from prompt_toolkit.key_binding import KeyBindings, merge_key_bindings
from prompt_toolkit.history import (
    FileHistory,
    History,
    ThreadedHistory,
)
from ptpython.layout import CompletionVisualisation  # type: ignore
from ptpython.key_bindings import (  # type: ignore
    load_python_bindings, load_sidebar_bindings,
)

import pw_console.key_bindings
import pw_console.widgets.checkbox
import pw_console.style
from pw_console.help_window import HelpWindow
from pw_console.log_pane import LogPane
from pw_console.pw_ptpython_repl import PwPtPythonRepl
from pw_console.repl_pane import ReplPane
from pw_console.window_manager import WindowManager

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


def _add_log_handler_to_pane(logger: Union[str, logging.Logger],
                             pane: 'LogPane') -> None:
    """A log pane handler for a given logger instance."""
    if not pane:
        return

    if isinstance(logger, logging.Logger):
        logger_instance = logger
    elif isinstance(logger, str):
        logger_instance = logging.getLogger(logger)

    logger_instance.addHandler(pane.log_view.log_store  # type: ignore
                               )
    pane.append_pane_subtitle(  # type: ignore
        logger_instance.name)


class ConsoleApp:
    """The main ConsoleApp class that glues everything together."""

    # pylint: disable=too-many-instance-attributes
    def __init__(
        self,
        global_vars=None,
        local_vars=None,
        repl_startup_message=None,
        help_text=None,
        app_title=None,
        color_depth=None,
        extra_completers=None,
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

        # TODO(tonymd): Make these configurable per project.
        self.repl_history_filename = Path.home() / '.pw_console_history'
        self.search_history_filename = Path.home() / '.pw_console_search'
        # History instance for search toolbars.
        self.search_history: History = ThreadedHistory(
            FileHistory(self.search_history_filename))

        # Event loop for executing user repl code.
        self.user_code_loop = asyncio.new_event_loop()

        self.app_title = app_title if app_title else 'Pigweed Console'

        # Top title message
        self.message = [
            ('class:logo', self.app_title),
            ('class:menu-bar', '  '),
        ]
        self.message.extend(
            pw_console.widgets.checkbox.to_keybind_indicator('F1', 'Help '))
        self.message.extend(
            pw_console.widgets.checkbox.to_keybind_indicator(
                'Ctrl-W', 'Quit '))

        # Top level UI state toggles.
        self.load_theme()

        # Pigweed upstream RST user guide
        self.user_guide_window = HelpWindow(self)
        self.user_guide_window.load_user_guide()

        # Auto-generated keybindings list for all active panes
        self.keybind_help_window = HelpWindow(self)

        # Downstream project specific help text
        self.app_help_text = help_text if help_text else None
        self.app_help_window = HelpWindow(self, additional_help_text=help_text)
        self.app_help_window.generate_help_text()

        # Used for tracking which pane was in focus before showing help window.
        self.last_focused_pane = None

        # Create a ptpython repl instance.
        self.pw_ptpython_repl = PwPtPythonRepl(
            get_globals=lambda: global_vars,
            get_locals=lambda: local_vars,
            color_depth=color_depth,
            history_filename=self.repl_history_filename,
            extra_completers=extra_completers,
        )
        self.input_history = self.pw_ptpython_repl.history

        self.repl_pane = ReplPane(
            application=self,
            python_repl=self.pw_ptpython_repl,
            startup_message=repl_startup_message,
        )

        # Window panes are added via the window_manager
        self.window_manager = WindowManager(self)
        self.window_manager.add_window(self.repl_pane)

        # Top of screen menu items
        self.menu_items = self._create_menu_items()

        # Key bindings registry.
        self.key_bindings = pw_console.key_bindings.create_key_bindings(self)

        # Create help window text based global key_bindings and active panes.
        self._update_help_window()

        # prompt_toolkit root container.
        self.root_container = MenuContainer(
            body=self.window_manager.create_root_split(),
            menu_items=self.menu_items,
            floats=[
                # Top message bar
                Float(
                    content=FloatingMessageBar(self),
                    top=0,
                    right=0,
                    height=1,
                ),
                # Centered floating help windows
                Float(
                    content=self.app_help_window,
                    top=2,
                    bottom=2,
                    # Callable to get width
                    width=self.app_help_window.content_width,
                ),
                Float(
                    content=self.user_guide_window,
                    top=2,
                    bottom=2,
                    # Callable to get width
                    width=self.user_guide_window.content_width,
                ),
                Float(
                    content=self.keybind_help_window,
                    top=2,
                    bottom=2,
                    # Callable to get width
                    width=self.keybind_help_window.content_width,
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
            color_depth=color_depth)

    def _run_pane_menu_option(self, function_to_run):
        function_to_run()
        self.update_menu_items()
        self.focus_main_menu()

    def update_menu_items(self):
        self.root_container.menu_items = self._create_menu_items()

    def _create_menu_items(self):
        file_and_view_menu = [
            # File menu
            MenuItem(
                '[File]',
                children=[
                    MenuItem('Exit', handler=self.exit_console),
                ],
            ),
            # View menu
            MenuItem(
                '[View]',
                children=[
                    MenuItem(
                        '{check} Vertical Window Spliting'.format(
                            check=pw_console.widgets.checkbox.to_checkbox_text(
                                self.window_manager.vertical_split)),
                        handler=self.window_manager.toggle_vertical_split),
                    MenuItem('Rotate Window Order',
                             handler=self.window_manager.rotate_panes),
                    MenuItem('-'),
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
                ],
            ),
        ]

        window_menu = [
            # Window pane menu
            MenuItem(
                '[Windows]',
                children=[
                    MenuItem(
                        '{index}: {title} {subtitle}'.format(
                            index=index + 1,
                            title=pane.menu_title(),
                            subtitle=pane.pane_subtitle()),
                        children=[
                            MenuItem(
                                '{check} Show Window'.format(
                                    check=pw_console.widgets.checkbox.
                                    to_checkbox_text(pane.show_pane, end='')),
                                handler=functools.partial(
                                    self.window_manager.toggle_pane, pane),
                            ),
                        ] + [
                            MenuItem(text,
                                     handler=functools.partial(
                                         self._run_pane_menu_option, handler))
                            for text, handler in pane.get_all_menu_options()
                        ],
                    ) for index, pane in enumerate(
                        self.window_manager.active_panes)
                ],
            )
        ]

        help_menu_items = [
            MenuItem('User Guide',
                     handler=self.user_guide_window.toggle_display),
            MenuItem('Keyboard Shortcuts',
                     handler=self.keybind_help_window.toggle_display),
        ]

        if self.app_help_text:
            help_menu_items.extend([
                MenuItem('-'),
                MenuItem(self.app_title + ' Help',
                         handler=self.app_help_window.toggle_display)
            ])

        help_menu = [
            # Info / Help
            MenuItem(
                '[Help]',
                children=help_menu_items,
            ),
        ]

        return file_and_view_menu + window_menu + help_menu

    def focus_main_menu(self):
        """Set application focus to the main menu."""
        self.application.layout.focus(self.root_container.window)

    def focus_on_container(self, pane):
        """Set application focus to a specific container."""
        self.application.layout.focus(pane)

    def toggle_light_theme(self):
        """Toggle light and dark theme colors."""
        # Use ptpython's style_transformation to swap dark and light colors.
        self.pw_ptpython_repl.swap_light_and_dark = (
            not self.pw_ptpython_repl.swap_light_and_dark)

    def load_theme(self, theme_name=None):
        """Regenerate styles for the current theme_name."""
        self._current_theme = pw_console.style.generate_styles(theme_name)

    def _create_log_pane(self, title=None) -> 'LogPane':
        # Create one log pane.
        log_pane = LogPane(application=self, pane_title=title)
        self.window_manager.add_pane(log_pane, add_at_beginning=True)
        return log_pane

    def add_log_handler(self,
                        window_title: str,
                        logger_instances: Iterable[logging.Logger],
                        separate_log_panes=False):
        """Add the Log pane as a handler for this logger instance."""

        existing_log_pane = None
        # Find an existing LogPane with the same window_title.
        for pane in self.window_manager.active_panes:
            if isinstance(pane, LogPane) and pane.pane_title() == window_title:
                existing_log_pane = pane
                break

        if not existing_log_pane or separate_log_panes:
            existing_log_pane = self._create_log_pane(title=window_title)

        for logger in logger_instances:
            _add_log_handler_to_pane(logger, existing_log_pane)

        self.window_manager.update_root_container_body()
        self.update_menu_items()
        self._update_help_window()

    def _user_code_thread_entry(self):
        """Entry point for the user code thread."""
        asyncio.set_event_loop(self.user_code_loop)
        self.user_code_loop.run_forever()

    def run_after_render_hooks(self, *unused_args, **unused_kwargs):
        """Run each active pane's `after_render_hook` if defined."""
        for pane in self.window_manager.active_panes:
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

        self.keybind_help_window.add_custom_keybinds_help_text(
            'Global Mouse', mouse_functions)

        # Add global key bindings to the help text.
        self.keybind_help_window.add_keybind_help_text('Global',
                                                       self.key_bindings)

        # Add activated plugin key bindings to the help text.
        for pane in self.window_manager.active_panes:
            for key_bindings in pane.get_all_key_bindings():
                help_section_title = pane.__class__.__name__
                if isinstance(key_bindings, KeyBindings):
                    self.keybind_help_window.add_keybind_help_text(
                        help_section_title, key_bindings)
                elif isinstance(key_bindings, dict):
                    self.keybind_help_window.add_custom_keybinds_help_text(
                        help_section_title, key_bindings)

        self.keybind_help_window.generate_help_text()

    def toggle_log_line_wrapping(self):
        """Menu item handler to toggle line wrapping of all log panes."""
        for pane in self.window_manager.active_panes:
            if isinstance(pane, LogPane):
                pane.toggle_wrap_lines()

    def focused_window(self):
        """Return the currently focused window."""
        return self.application.layout.current_window

    def modal_window_is_open(self):
        if self.app_help_text:
            return (self.app_help_window.show_window
                    or self.keybind_help_window.show_window
                    or self.user_guide_window.show_window)
        return (self.keybind_help_window.show_window
                or self.user_guide_window.show_window)

    def exit_console(self):
        """Quit the console prompt_toolkit application UI."""
        self.application.exit()

    def redraw_ui(self):
        """Redraw the prompt_toolkit UI."""
        if hasattr(self, 'application'):
            # Thread safe way of sending a repaint trigger to the input event
            # loop.
            self.application.invalidate()

    async def run(self, test_mode=False):
        """Start the prompt_toolkit UI."""
        self.window_manager.reset_pane_sizes()

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
        # Sample log line format:
        # Log message [=         ] # 100

        # Fake module column names.
        module_names = ['APP', 'RADIO', 'BAT', 'USB', 'CPU']
        while True:
            await asyncio.sleep(1)
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

            module_name = module_names[message_count % len(module_names)]
            _FAKE_DEVICE_LOG.info(
                new_log_line,
                extra=dict(extra_metadata_fields=dict(module=module_name)))
            message_count += 1


# TODO(tonymd): Remove this alias when not used by downstream projects.
def embed(
    *args,
    **kwargs,
) -> None:
    """PwConsoleEmbed().embed() alias."""
    # Import here to avoid circular dependency
    from pw_console.embed import PwConsoleEmbed  # pylint: disable=import-outside-toplevel
    console = PwConsoleEmbed(*args, **kwargs)
    console.embed()
