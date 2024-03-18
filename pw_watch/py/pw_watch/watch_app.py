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
import functools
import logging
import os
import re
import time
from typing import Callable, Iterable, NoReturn, Optional

from prompt_toolkit.application import Application
from prompt_toolkit.clipboard.pyperclip import PyperclipClipboard
from prompt_toolkit.filters import Condition
from prompt_toolkit.history import (
    InMemoryHistory,
    History,
    ThreadedHistory,
)
from prompt_toolkit.key_binding import (
    KeyBindings,
    KeyBindingsBase,
    merge_key_bindings,
)
from prompt_toolkit.layout import (
    DynamicContainer,
    Float,
    FloatContainer,
    FormattedTextControl,
    HSplit,
    Layout,
    Window,
)
from prompt_toolkit.layout.controls import BufferControl
from prompt_toolkit.styles import (
    ConditionalStyleTransformation,
    DynamicStyle,
    SwapLightAndDarkStyleTransformation,
    merge_style_transformations,
    merge_styles,
    style_from_pygments_cls,
)
from prompt_toolkit.formatted_text import StyleAndTextTuples
from prompt_toolkit.lexers import PygmentsLexer
from pygments.lexers.markup import MarkdownLexer  # type: ignore

from pw_config_loader import yaml_config_loader_mixin

from pw_console.console_app import get_default_colordepth, MIN_REDRAW_INTERVAL
from pw_console.get_pw_console_app import PW_CONSOLE_APP_CONTEXTVAR
from pw_console.help_window import HelpWindow
from pw_console.key_bindings import DEFAULT_KEY_BINDINGS
from pw_console.log_pane import LogPane
from pw_console.plugin_mixin import PluginMixin
import pw_console.python_logging
from pw_console.quit_dialog import QuitDialog
from pw_console.style import generate_styles, get_theme_colors
from pw_console.pigweed_code_style import PigweedCodeStyle
from pw_console.widgets import (
    FloatingWindowPane,
    ToolbarButton,
    WindowPaneToolbar,
    create_border,
    mouse_handlers,
    to_checkbox,
)
from pw_console.window_list import DisplayMode
from pw_console.window_manager import WindowManager

from pw_build.project_builder_prefs import ProjectBuilderPrefs
from pw_build.project_builder_context import get_project_builder_context


_LOG = logging.getLogger('pw_build.watch')

BUILDER_CONTEXT = get_project_builder_context()

_HELP_TEXT = """
Mouse Keys
==========

- Click on a line in the bottom progress bar to switch to that tab.
- Click on any tab, or button to activate.
- Scroll wheel in the the log windows moves back through the history.


Global Keys
===========

Quit with confirmation dialog. --------------------  Ctrl-D
Quit without confirmation. ------------------------  Ctrl-X Ctrl-C
Toggle user guide window. -------------------------  F1
Trigger a rebuild. --------------------------------  Enter


Window Management Keys
======================

Switch focus to the next window pane or tab. ------  Ctrl-Alt-N
Switch focus to the previous window pane or tab. --  Ctrl-Alt-P
Move window pane left. ----------------------------  Ctrl-Alt-Left
Move window pane right. ---------------------------  Ctrl-Alt-Right
Move window pane down. ----------------------------  Ctrl-Alt-Down
Move window pane up. ------------------------------  Ctrl-Alt-Up
Balance all window sizes. -------------------------  Ctrl-U


Bottom Toolbar Controls
=======================

Rebuild Enter --------------- Click or press Enter to trigger a rebuild.
[x] Auto Rebuild ------------ Click to globaly enable or disable automatic
                              rebuilding when files change.
Help F1 --------------------- Click or press F1 to open this help window.
Quit Ctrl-d ----------------- Click or press Ctrl-d to quit pw_watch.
Next Tab Ctrl-Alt-n --------- Switch to the next log tab.
Previous Tab Ctrl-Alt-p ----- Switch to the previous log tab.


Build Status Bar
================

The build status bar shows the current status of all build directories outlined
in a colored frame.

  ┏━━ BUILDING ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓
  ┃ [✓] out_directory  Building  Last line of standard out.                ┃
  ┃ [✓] out_dir2       Waiting   Last line of standard out.                ┃
  ┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛

Each checkbox on the far left controls whether that directory is built when
files change and manual builds are run.


Copying Text
============

- Click drag will select whole lines in the log windows.
- `Ctrl-c` will copy selected lines to your system clipboard.

If running over SSH you will need to use your terminal's built in text
selection.

Linux
-----

- Holding `Shift` and dragging the mouse in most terminals.

Mac
---

- Apple Terminal:

  Hold `Fn` and drag the mouse

- iTerm2:

  Hold `Cmd+Option` and drag the mouse

Windows
-------

- Git CMD (included in `Git for Windows)

  1. Click on the Git window icon in the upper left of the title bar
  2. Click `Edit` then `Mark`
  3. Drag the mouse to select text and press Enter to copy.

- Windows Terminal

  1. Hold `Shift` and drag the mouse to select text
  2. Press `Ctrl-Shift-C` to copy.

"""


class WatchAppPrefs(ProjectBuilderPrefs):
    """Add pw_console specific prefs standard ProjectBuilderPrefs."""

    def __init__(self, *args, **kwargs) -> None:
        super().__init__(*args, **kwargs)

        self.registered_commands = DEFAULT_KEY_BINDINGS
        self.registered_commands.update(self.user_key_bindings)

        new_config_settings = {
            'key_bindings': DEFAULT_KEY_BINDINGS,
            'show_python_logger': True,
        }
        self.default_config.update(new_config_settings)
        self._update_config(
            new_config_settings,
            yaml_config_loader_mixin.Stage.DEFAULT,
        )

    # Required pw_console preferences for key bindings and themes
    @property
    def user_key_bindings(self) -> dict[str, list[str]]:
        return self._config.get('key_bindings', {})

    @property
    def ui_theme(self) -> str:
        return self._config.get('ui_theme', '')

    @ui_theme.setter
    def ui_theme(self, new_ui_theme: str) -> None:
        self._config['ui_theme'] = new_ui_theme

    @property
    def theme_colors(self):
        return get_theme_colors(self.ui_theme)

    @property
    def swap_light_and_dark(self) -> bool:
        return self._config.get('swap_light_and_dark', False)

    def get_function_keys(self, name: str) -> list:
        """Return the keys for the named function."""
        try:
            return self.registered_commands[name]
        except KeyError as error:
            raise KeyError('Unbound key function: {}'.format(name)) from error

    def register_named_key_function(
        self, name: str, default_bindings: list[str]
    ) -> None:
        self.registered_commands[name] = default_bindings

    def register_keybinding(
        self, name: str, key_bindings: KeyBindings, **kwargs
    ) -> Callable:
        """Apply registered keys for the given named function."""

        def decorator(handler: Callable) -> Callable:
            "`handler` is a callable or Binding."
            for keys in self.get_function_keys(name):
                key_bindings.add(*keys.split(' '), **kwargs)(handler)
            return handler

        return decorator

    # Required pw_console preferences for using a log window pane.
    @property
    def spaces_between_columns(self) -> int:
        return 2

    @property
    def window_column_split_method(self) -> str:
        return 'vertical'

    @property
    def hide_date_from_log_time(self) -> bool:
        return True

    @property
    def column_order(self) -> list:
        return []

    def column_style(  # pylint: disable=no-self-use
        self,
        _column_name: str,
        _column_value: str,
        default='',
    ) -> str:
        return default

    @property
    def show_python_file(self) -> bool:
        return self._config.get('show_python_file', False)

    @property
    def show_source_file(self) -> bool:
        return self._config.get('show_source_file', False)

    @property
    def show_python_logger(self) -> bool:
        return self._config.get('show_python_logger', False)


class WatchWindowManager(WindowManager):
    def update_root_container_body(self):
        self.application.window_manager_container = self.create_root_container()


class WatchApp(PluginMixin):
    """Pigweed Watch main window application."""

    # pylint: disable=too-many-instance-attributes

    NINJA_FAILURE_TEXT = '\033[31mFAILED: '

    NINJA_BUILD_STEP = re.compile(
        r"^\[(?P<step>[0-9]+)/(?P<total_steps>[0-9]+)\] (?P<action>.*)$"
    )

    def __init__(
        self,
        event_handler,
        prefs: WatchAppPrefs,
    ):
        self.event_handler = event_handler

        self.color_depth = get_default_colordepth()

        # Necessary for some of pw_console's window manager features to work
        # such as mouse drag resizing.
        PW_CONSOLE_APP_CONTEXTVAR.set(self)  # type: ignore

        self.prefs = prefs

        self.quit_dialog = QuitDialog(self, self.exit)  # type: ignore

        self.search_history: History = ThreadedHistory(InMemoryHistory())

        self.window_manager = WatchWindowManager(self)

        self._build_error_count = 0
        self._errors_in_output = False

        self.log_ui_update_frequency = 0.1  # 10 FPS
        self._last_ui_update_time = time.time()

        self.recipe_name_to_log_pane: dict[str, LogPane] = {}
        self.recipe_index_to_log_pane: dict[int, LogPane] = {}

        debug_logging = (
            event_handler.project_builder.default_log_level == logging.DEBUG
        )
        level_name = 'DEBUG' if debug_logging else 'INFO'

        no_propagation_loggers = []

        if event_handler.separate_logfiles:
            pane_index = len(event_handler.project_builder.build_recipes) - 1
            for recipe in reversed(event_handler.project_builder.build_recipes):
                log_pane = self.add_build_log_pane(
                    recipe.display_name,
                    loggers=[recipe.log],
                    level_name=level_name,
                )
                if recipe.log.propagate is False:
                    no_propagation_loggers.append(recipe.log)

                self.recipe_name_to_log_pane[recipe.display_name] = log_pane
                self.recipe_index_to_log_pane[pane_index] = log_pane
                pane_index -= 1

        pw_console.python_logging.setup_python_logging(
            loggers_with_no_propagation=no_propagation_loggers
        )

        self.root_log_pane = self.add_build_log_pane(
            'Root Log',
            loggers=[
                logging.getLogger('pw_build'),
            ],
            level_name=level_name,
        )
        # Repeat the Attaching filesystem watcher message for the full screen
        # interface. The original log in watch.py will be hidden from view.
        _LOG.info('Attaching filesystem watcher...')

        self.window_manager.window_lists[0].display_mode = DisplayMode.TABBED

        self.window_manager_container = (
            self.window_manager.create_root_container()
        )

        self.status_bar_border_style = 'class:command-runner-border'

        self.status_bar_control = FormattedTextControl(self.get_status_bar_text)

        self.status_bar_container = create_border(
            HSplit(
                [
                    # Result Toolbar.
                    Window(
                        content=self.status_bar_control,
                        height=len(self.event_handler.project_builder),
                        wrap_lines=False,
                        style='class:pane_active',
                    ),
                ]
            ),
            content_height=len(self.event_handler.project_builder),
            title=BUILDER_CONTEXT.get_title_bar_text,
            border_style=(BUILDER_CONTEXT.get_title_style),
            base_style='class:pane_active',
            left_margin_columns=1,
            right_margin_columns=1,
        )

        self.floating_window_plugins: list[FloatingWindowPane] = []

        self.user_guide_window = HelpWindow(
            self,  # type: ignore
            title='Pigweed Watch',
            disable_ctrl_c=True,
        )
        self.user_guide_window.set_help_text(
            _HELP_TEXT, lexer=PygmentsLexer(MarkdownLexer)
        )

        self.help_toolbar = WindowPaneToolbar(
            title='Pigweed Watch',
            include_resize_handle=False,
            focus_action_callable=self.switch_to_root_log,
            click_to_focus_text='',
        )
        self.help_toolbar.add_button(
            ToolbarButton('Enter', 'Rebuild', self.run_build)
        )
        self.help_toolbar.add_button(
            ToolbarButton(
                description='Auto Rebuild',
                mouse_handler=self.toggle_restart_on_filechange,
                is_checkbox=True,
                checked=lambda: self.restart_on_changes,
            )
        )
        self.help_toolbar.add_button(
            ToolbarButton('F1', 'Help', self.user_guide_window.toggle_display)
        )
        self.help_toolbar.add_button(ToolbarButton('Ctrl-d', 'Quit', self.exit))
        self.help_toolbar.add_button(
            ToolbarButton(
                'Ctrl-Alt-n', 'Next Tab', self.window_manager.focus_next_pane
            )
        )
        self.help_toolbar.add_button(
            ToolbarButton(
                'Ctrl-Alt-p',
                'Previous Tab',
                self.window_manager.focus_previous_pane,
            )
        )

        self.root_container = FloatContainer(
            HSplit(
                [
                    # Window pane content:
                    DynamicContainer(lambda: self.window_manager_container),
                    self.status_bar_container,
                    self.help_toolbar,
                ]
            ),
            floats=[
                Float(
                    content=self.user_guide_window,
                    top=2,
                    left=4,
                    bottom=4,
                    width=self.user_guide_window.content_width,
                ),
                Float(
                    content=self.quit_dialog,
                    top=2,
                    left=2,
                ),
            ],
        )

        key_bindings = KeyBindings()

        @key_bindings.add('enter', filter=self.input_box_not_focused())
        def _run_build(_event):
            "Rebuild."
            self.run_build()

        register = self.prefs.register_keybinding

        @register('global.exit-no-confirmation', key_bindings)
        def _quit_no_confirm(_event):
            """Quit without confirmation."""
            _LOG.info('Got quit signal; exiting...')
            self.exit(0)

        @register('global.exit-with-confirmation', key_bindings)
        def _quit_with_confirm(_event):
            """Quit with confirmation dialog."""
            self.quit_dialog.open_dialog()

        @register(
            'global.open-user-guide',
            key_bindings,
            filter=Condition(lambda: not self.modal_window_is_open()),
        )
        def _show_help(_event):
            """Toggle user guide window."""
            self.user_guide_window.toggle_display()

        self.key_bindings = merge_key_bindings(
            [
                self.window_manager.key_bindings,
                key_bindings,
            ]
        )

        self.current_theme = generate_styles(self.prefs.ui_theme)

        self.style_transformation = merge_style_transformations(
            [
                ConditionalStyleTransformation(
                    SwapLightAndDarkStyleTransformation(),
                    filter=Condition(lambda: self.prefs.swap_light_and_dark),
                ),
            ]
        )

        self.code_theme = style_from_pygments_cls(PigweedCodeStyle)

        self.layout = Layout(
            self.root_container,
            focused_element=self.root_log_pane,
        )

        self.application: Application = Application(
            layout=self.layout,
            key_bindings=self.key_bindings,
            mouse_support=True,
            color_depth=self.color_depth,
            clipboard=PyperclipClipboard(),
            style=DynamicStyle(
                lambda: merge_styles(
                    [
                        self.current_theme,
                        self.code_theme,
                    ]
                )
            ),
            style_transformation=self.style_transformation,
            full_screen=True,
            min_redraw_interval=MIN_REDRAW_INTERVAL,
        )

        self.plugin_init(
            plugin_callback=self.check_build_status,
            plugin_callback_frequency=0.5,
            plugin_logger_name='pw_watch_stdout_checker',
        )

    def add_build_log_pane(
        self, title: str, loggers: list[logging.Logger], level_name: str
    ) -> LogPane:
        """Setup a new build log pane."""
        new_log_pane = LogPane(application=self, pane_title=title)
        for logger in loggers:
            new_log_pane.add_log_handler(logger, level_name=level_name)

        # Set python log format to just the message itself.
        new_log_pane.log_view.log_store.formatter = logging.Formatter(
            '%(message)s'
        )

        new_log_pane.table_view = False

        # Disable line wrapping for improved error visibility.
        if new_log_pane.wrap_lines:
            new_log_pane.toggle_wrap_lines()

        # Blank right side toolbar text
        new_log_pane._pane_subtitle = ' '  # pylint: disable=protected-access

        # Make tab and shift-tab search for next and previous error
        next_error_bindings = KeyBindings()

        @next_error_bindings.add('s-tab')
        def _previous_error(_event):
            self.jump_to_error(backwards=True)

        @next_error_bindings.add('tab')
        def _next_error(_event):
            self.jump_to_error()

        existing_log_bindings: Optional[
            KeyBindingsBase
        ] = new_log_pane.log_content_control.key_bindings

        key_binding_list: list[KeyBindingsBase] = []
        if existing_log_bindings:
            key_binding_list.append(existing_log_bindings)
        key_binding_list.append(next_error_bindings)
        new_log_pane.log_content_control.key_bindings = merge_key_bindings(
            key_binding_list
        )

        # Only show a few buttons in the log pane toolbars.
        new_buttons = []
        for button in new_log_pane.bottom_toolbar.buttons:
            if button.description in [
                'Search',
                'Save',
                'Follow',
                'Wrap',
                'Clear',
            ]:
                new_buttons.append(button)
        new_log_pane.bottom_toolbar.buttons = new_buttons

        self.window_manager.add_pane(new_log_pane)
        return new_log_pane

    def logs_redraw(self):
        emit_time = time.time()
        # Has enough time passed since last UI redraw due to new logs?
        if emit_time > self._last_ui_update_time + self.log_ui_update_frequency:
            # Update last log time
            self._last_ui_update_time = emit_time

            # Trigger Prompt Toolkit UI redraw.
            self.redraw_ui()

    def jump_to_error(self, backwards: bool = False) -> None:
        if not self.root_log_pane.log_view.search_text:
            self.root_log_pane.log_view.set_search_regex(
                '^FAILED: ', False, None
            )
        if backwards:
            self.root_log_pane.log_view.search_backwards()
        else:
            self.root_log_pane.log_view.search_forwards()
        self.root_log_pane.log_view.log_screen.reset_logs(
            log_index=self.root_log_pane.log_view.log_index
        )

        self.root_log_pane.log_view.move_selected_line_to_top()

    def refresh_layout(self) -> None:
        self.window_manager.update_root_container_body()

    def update_menu_items(self):
        """Required by the Window Manager Class."""

    def redraw_ui(self):
        """Redraw the prompt_toolkit UI."""
        if hasattr(self, 'application'):
            self.application.invalidate()

    def focus_on_container(self, pane):
        """Set application focus to a specific container."""
        # Try to focus on the given pane
        try:
            self.application.layout.focus(pane)
        except ValueError:
            # If the container can't be focused, focus on the first visible
            # window pane.
            self.window_manager.focus_first_visible_pane()

    def focused_window(self):
        """Return the currently focused window."""
        return self.application.layout.current_window

    def focus_main_menu(self):
        """Focus on the main menu.

        Currently pw_watch has no main menu so focus on the first visible pane
        instead."""
        self.window_manager.focus_first_visible_pane()

    def switch_to_root_log(self) -> None:
        (
            window_list,
            pane_index,
        ) = self.window_manager.find_window_list_and_pane_index(
            self.root_log_pane
        )
        window_list.switch_to_tab(pane_index)

    def switch_to_build_log(self, log_index: int) -> None:
        pane = self.recipe_index_to_log_pane.get(log_index, None)
        if not pane:
            return

        (
            window_list,
            pane_index,
        ) = self.window_manager.find_window_list_and_pane_index(pane)
        window_list.switch_to_tab(pane_index)

    def command_runner_is_open(self) -> bool:
        # pylint: disable=no-self-use
        return False

    def all_log_panes(self) -> Iterable[LogPane]:
        for pane in self.window_manager.active_panes():
            if isinstance(pane, LogPane):
                yield pane

    def clear_log_panes(self) -> None:
        """Erase all log pane content and turn on follow.

        This is called whenever rebuilds occur. Either a manual build from
        self.run_build or on file changes called from
        pw_watch._handle_matched_event."""
        for pane in self.all_log_panes():
            pane.log_view.clear_visual_selection()
            pane.log_view.clear_filters()
            pane.log_view.log_store.clear_logs()
            pane.log_view.view_mode_changed()
            # Re-enable follow if needed
            if not pane.log_view.follow:
                pane.log_view.toggle_follow()

    def run_build(self) -> None:
        """Manually trigger a rebuild from the UI."""
        self.clear_log_panes()
        self.event_handler.rebuild()

    @property
    def restart_on_changes(self) -> bool:
        return self.event_handler.restart_on_changes

    def toggle_restart_on_filechange(self) -> None:
        self.event_handler.restart_on_changes = (
            not self.event_handler.restart_on_changes
        )

    def get_status_bar_text(self) -> StyleAndTextTuples:
        """Return formatted text for build status bar."""
        formatted_text: StyleAndTextTuples = []

        separator = ('', ' ')
        name_width = self.event_handler.project_builder.max_name_width

        # pylint: disable=protected-access
        (
            _window_list,
            pane,
        ) = self.window_manager._get_active_window_list_and_pane()
        # pylint: enable=protected-access
        restarting = BUILDER_CONTEXT.restart_flag

        for i, cfg in enumerate(self.event_handler.project_builder):
            # The build directory
            name_style = ''
            if not pane:
                formatted_text.append(('', '\n'))
                continue

            # Dim the build name if disabled
            if not cfg.enabled:
                name_style = 'class:theme-fg-inactive'

            # If this build tab is selected, highlight with cyan.
            if pane.pane_title() == cfg.display_name:
                name_style = 'class:theme-fg-cyan'

            formatted_text.append(
                to_checkbox(
                    cfg.enabled,
                    functools.partial(
                        mouse_handlers.on_click,
                        cfg.toggle_enabled,
                    ),
                    end=' ',
                    unchecked_style='class:checkbox',
                    checked_style='class:checkbox-checked',
                )
            )
            formatted_text.append(
                (
                    name_style,
                    f'{cfg.display_name}'.ljust(name_width),
                    functools.partial(
                        mouse_handlers.on_click,
                        functools.partial(self.switch_to_build_log, i),
                    ),
                )
            )
            formatted_text.append(separator)
            # Status
            formatted_text.append(cfg.status.status_slug(restarting=restarting))
            formatted_text.append(separator)
            # Current stdout line
            formatted_text.extend(cfg.status.current_step_formatted())
            formatted_text.append(('', '\n'))

        if not formatted_text:
            formatted_text = [('', 'Loading...')]

        self.set_tab_bar_colors()

        return formatted_text

    def set_tab_bar_colors(self) -> None:
        restarting = BUILDER_CONTEXT.restart_flag

        for cfg in BUILDER_CONTEXT.recipes:
            pane = self.recipe_name_to_log_pane.get(cfg.display_name, None)
            if not pane:
                continue

            pane.extra_tab_style = None
            if not restarting and cfg.status.failed():
                pane.extra_tab_style = 'class:theme-fg-red'

    def exit(
        self,
        exit_code: int = 1,
        log_after_shutdown: Optional[Callable[[], None]] = None,
    ) -> None:
        _LOG.info('Exiting...')
        BUILDER_CONTEXT.ctrl_c_pressed = True

        # Shut everything down after the prompt_toolkit app exits.
        def _really_exit(future: asyncio.Future) -> NoReturn:
            BUILDER_CONTEXT.restore_logging_and_shutdown(log_after_shutdown)
            os._exit(future.result())  # pylint: disable=protected-access

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
            return not isinstance(
                self.application.layout.current_control, BufferControl
            )

        return _test

    def modal_window_is_open(self):
        """Return true if any modal window or dialog is open."""
        floating_window_is_open = (
            self.user_guide_window.show_window or self.quit_dialog.show_dialog
        )

        floating_plugin_is_open = any(
            plugin.show_pane for plugin in self.floating_window_plugins
        )

        return floating_window_is_open or floating_plugin_is_open
