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

import collections
import collections.abc
import builtins
import asyncio
import logging
import functools
from pathlib import Path
from threading import Thread
from typing import Iterable, Union

from prompt_toolkit.layout.menus import CompletionsMenu
from prompt_toolkit.application import Application
from prompt_toolkit.filters import Condition, has_focus
from prompt_toolkit.styles import (
    DynamicStyle,
    merge_styles,
)
from prompt_toolkit.layout import (
    ConditionalContainer,
    Dimension,
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


# Weighted amount for adjusting window dimensions when enlarging and shrinking.
_WINDOW_SIZE_ADJUST = 2


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
        self.vertical_split = False
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
        )
        self.input_history = self.pw_ptpython_repl.history

        self.repl_pane = ReplPane(
            application=self,
            python_repl=self.pw_ptpython_repl,
            startup_message=repl_startup_message,
        )

        # List of enabled panes.
        self.active_panes: collections.deque = collections.deque()
        self.active_panes.append(self.repl_pane)

        # Reference to the current prompt_toolkit window split for the current
        # set of active_panes.
        self.active_pane_split = None

        # Top of screen menu items
        self.menu_items = self._create_menu_items()

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
                    MenuItem('{check} Vertical Window Spliting'.format(
                        check=pw_console.widgets.checkbox.to_checkbox_text(
                            self.vertical_split)),
                             handler=self.toggle_vertical_split),
                    MenuItem('Rotate Window Order', handler=self.rotate_panes),
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
                                    self.toggle_pane, pane),
                            ),
                        ] + [
                            MenuItem(text,
                                     handler=functools.partial(
                                         self._run_pane_menu_option, handler))
                            for text, handler in pane.get_all_menu_options()
                        ],
                    ) for index, pane in enumerate(self.active_panes)
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

    def _get_current_active_pane(self):
        """Return the current active window pane."""
        focused_pane = None
        for pane in self.active_panes:
            if has_focus(pane)():
                focused_pane = pane
                break
        return focused_pane

    def add_pane(self, new_pane, existing_pane=None):
        existing_pane_index = None
        if existing_pane:
            try:
                existing_pane_index = self.active_panes.index(existing_pane)
            except ValueError:
                # Ignore ValueError which can be raised by the self.active_panes
                # deque if existing_pane can't be found.
                pass
        if existing_pane_index:
            self.active_panes.insert(new_pane, existing_pane_index + 1)
        else:
            self.active_panes.append(new_pane)

        self.update_menu_items()
        self._update_root_container_body()

        self.redraw_ui()

    def remove_pane(self, existing_pane):
        existing_pane_index = 0
        if not existing_pane:
            return
        try:
            existing_pane_index = self.active_panes.index(existing_pane)
            self.active_panes.remove(existing_pane)
        except ValueError:
            # Ignore ValueError which can be raised by the self.active_panes
            # deque if existing_pane can't be found.
            pass

        self.update_menu_items()
        self._update_root_container_body()
        if len(self.active_panes) > 0:
            existing_pane_index -= 1
            try:
                self.focus_on_container(self.active_panes[existing_pane_index])
            except ValueError:
                # ValueError will be raised if the the pane at
                # existing_pane_index can't be accessed.
                # Focus on the main menu if the existing pane is hidden.
                self.focus_main_menu()

        self.redraw_ui()

    def enlarge_pane(self):
        """Enlarge the currently focused window pane."""
        pane = self._get_current_active_pane()
        if pane:
            self.adjust_pane_size(pane, _WINDOW_SIZE_ADJUST)

    def shrink_pane(self):
        """Shrink the currently focused window pane."""
        pane = self._get_current_active_pane()
        if pane:
            self.adjust_pane_size(pane, -_WINDOW_SIZE_ADJUST)

    def adjust_pane_size(self, pane, diff: int = _WINDOW_SIZE_ADJUST):
        """Increase or decrease a given pane's width or height weight."""
        # Placeholder next_pane value to allow setting width and height without
        # any consequences if there is no next visible pane.
        next_pane = HSplit([],
                           height=Dimension(weight=50),
                           width=Dimension(weight=50))  # type: ignore
        # Try to get the next visible pane to subtract a weight value from.
        next_visible_pane = self._get_next_visible_pane_after(pane)
        if next_visible_pane:
            next_pane = next_visible_pane

        # If the last pane is selected, and there are at least 2 panes, make
        # next_pane the previous pane.
        try:
            if len(self.active_panes) >= 2 and (self.active_panes.index(pane)
                                                == len(self.active_panes) - 1):
                next_pane = self.active_panes[-2]
        except ValueError:
            # Ignore ValueError raised if self.active_panes[-2] doesn't exist.
            pass

        # Get current weight values
        if self.vertical_split:
            old_weight = pane.width.weight
            next_old_weight = next_pane.width.weight  # type: ignore
        else:  # Horizontal split
            old_weight = pane.height.weight
            next_old_weight = next_pane.height.weight  # type: ignore

        # Add to the current pane
        new_weight = old_weight + diff
        if new_weight <= 0:
            new_weight = old_weight

        # Subtract from the next pane
        next_new_weight = next_old_weight - diff
        if next_new_weight <= 0:
            next_new_weight = next_old_weight

        # Set new weight values
        if self.vertical_split:
            pane.width.weight = new_weight
            next_pane.width.weight = next_new_weight  # type: ignore
        else:  # Horizontal split
            pane.height.weight = new_weight
            next_pane.height.weight = next_new_weight  # type: ignore

    def reset_pane_sizes(self):
        """Reset all active pane width and height to 50%"""
        for pane in self.active_panes:
            pane.height = Dimension(weight=50)
            pane.width = Dimension(weight=50)

    def rotate_panes(self, steps=1):
        """Rotate the order of all active window panes."""
        self.active_panes.rotate(steps)
        self.update_menu_items()
        self._update_root_container_body()

    def toggle_pane(self, pane):
        """Toggle a pane on or off."""
        pane.show_pane = not pane.show_pane
        self.update_menu_items()
        self._update_root_container_body()

        # Set focus to the top level menu. This has the effect of keeping the
        # menu open if it's already open.
        self.focus_main_menu()

    def focus_main_menu(self):
        """Set application focus to the main menu."""
        self.application.layout.focus(self.root_container.window)

    def focus_on_container(self, pane):
        """Set application focus to a specific container."""
        self.application.layout.focus(pane)

    def _get_next_visible_pane_after(self, target_pane):
        """Return the next visible pane that appears after the target pane."""
        try:
            target_pane_index = self.active_panes.index(target_pane)
        except ValueError:
            # If pane can't be found, focus on the main menu.
            return None

        # Loop through active panes (not including the target_pane).
        for i in range(1, len(self.active_panes)):
            next_pane_index = (target_pane_index + i) % len(self.active_panes)
            next_pane = self.active_panes[next_pane_index]
            if next_pane.show_pane:
                return next_pane
        return None

    def focus_next_visible_pane(self, pane):
        """Focus on the next visible window pane if possible."""
        next_visible_pane = self._get_next_visible_pane_after(pane)
        if next_visible_pane:
            self.application.layout.focus(next_visible_pane)
            return
        self.focus_main_menu()

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
        self.active_panes.appendleft(
            LogPane(application=self, pane_title=title))
        return self.active_panes[0]

    def add_log_handler(self,
                        window_title: str,
                        logger_instances: Iterable[logging.Logger],
                        separate_log_panes=False):
        """Add the Log pane as a handler for this logger instance."""

        existing_log_pane = None
        # Find an existing LogPane with the same window_title.
        for pane in self.active_panes:
            if isinstance(pane, LogPane) and pane.pane_title() == window_title:
                existing_log_pane = pane
                break

        if not existing_log_pane or separate_log_panes:
            existing_log_pane = self._create_log_pane(title=window_title)

        for logger in logger_instances:
            _add_log_handler_to_pane(logger, existing_log_pane)

        self._update_root_container_body()
        self.update_menu_items()
        self._update_help_window()

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

        self.keybind_help_window.add_custom_keybinds_help_text(
            'Global Mouse', mouse_functions)

        # Add global key bindings to the help text.
        self.keybind_help_window.add_keybind_help_text('Global',
                                                       self.key_bindings)

        # Add activated plugin key bindings to the help text.
        for pane in self.active_panes:
            for key_bindings in pane.get_all_key_bindings():
                help_section_title = pane.__class__.__name__
                if isinstance(key_bindings, KeyBindings):
                    self.keybind_help_window.add_keybind_help_text(
                        help_section_title, key_bindings)
                elif isinstance(key_bindings, dict):
                    self.keybind_help_window.add_custom_keybinds_help_text(
                        help_section_title, key_bindings)

        self.keybind_help_window.generate_help_text()

    def _update_split_orientation(self):
        if self.vertical_split:
            self.active_pane_split = VSplit(
                list(pane for pane in self.active_panes if pane.show_pane),
                # Add a vertical separator between each active window pane.
                padding=1,
                padding_char='│',
                padding_style='class:pane_separator',
            )
        else:
            self.active_pane_split = HSplit(self.active_panes)

    def _create_root_split(self):
        """Create a vertical or horizontal split container for all active
        panes."""
        self._update_split_orientation()
        return HSplit([
            self.active_pane_split,
        ])

    def _update_root_container_body(self):
        # Replace the root MenuContainer body with the new split.
        self.root_container.container.content.children[
            1] = self._create_root_split()

    def toggle_log_line_wrapping(self):
        """Menu item handler to toggle line wrapping of all log panes."""
        for pane in self.active_panes:
            if isinstance(pane, LogPane):
                pane.toggle_wrap_lines()

    def toggle_vertical_split(self):
        """Toggle visibility of the help window."""
        self.vertical_split = not self.vertical_split

        self.update_menu_items()
        self._update_root_container_body()

        self.redraw_ui()

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
        self.reset_pane_sizes()

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
