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
"""ReplPane class."""

import asyncio
import concurrent
import functools
import logging
import pprint
from dataclasses import dataclass
from typing import (
    Any,
    Callable,
    Dict,
    List,
    Optional,
)

from prompt_toolkit.filters import (
    Condition,
    has_focus,
)
from prompt_toolkit.document import Document
from prompt_toolkit.key_binding import KeyBindings, KeyPressEvent
from prompt_toolkit.layout.dimension import AnyDimension
from prompt_toolkit.widgets import TextArea
from prompt_toolkit.layout import (
    ConditionalContainer,
    DynamicContainer,
    Dimension,
    FloatContainer,
    FormattedTextControl,
    HSplit,
    VSplit,
    Window,
    WindowAlign,
)
from prompt_toolkit.lexers import PygmentsLexer  # type: ignore
from pygments.lexers.python import PythonConsoleLexer  # type: ignore
# Alternative Formatting
# from IPython.lib.lexers import IPythonConsoleLexer  # type: ignore

import pw_console.mouse
import pw_console.style
import pw_console.widgets.focus_on_click_overlay
from pw_console.pw_ptpython_repl import PwPtPythonRepl
from pw_console.progress_bar.progress_bar_state import TASKS_CONTEXTVAR

_LOG = logging.getLogger(__package__)

_Namespace = Dict[str, Any]
_GetNamespace = Callable[[], _Namespace]


class ReplPaneBottomToolbarBar(ConditionalContainer):
    """Repl pane bottom toolbar."""
    @staticmethod
    def get_left_text_tokens(repl_pane):
        """Return toolbar indicator and title."""

        title = ' Python Input '
        mouse_handler = functools.partial(pw_console.mouse.focus_handler,
                                          repl_pane.pw_ptpython_repl)
        return pw_console.style.get_pane_indicator(repl_pane.pw_ptpython_repl,
                                                   title, mouse_handler)

    @staticmethod
    def get_center_text_tokens(repl_pane):
        """Return toolbar text showing if the ReplPane is in focus or not."""
        focus = functools.partial(pw_console.mouse.focus_handler,
                                  repl_pane.pw_ptpython_repl)
        paste_text = functools.partial(
            pw_console.widgets.mouse_handlers.on_click,
            repl_pane.paste_system_clipboard_to_input_buffer)
        clear_text = functools.partial(
            pw_console.widgets.mouse_handlers.on_click,
            repl_pane.clear_input_buffer)
        run_code = functools.partial(
            pw_console.widgets.mouse_handlers.on_click, repl_pane.run_code)

        button_style = pw_console.style.get_button_style(repl_pane)

        fragments = [
            (
                # Style
                '',
                # Text
                ' ',
                # Mouse handler
                focus,
            ),
        ]
        separator = [('', '  ', focus)]

        fragments.extend(
            pw_console.widgets.checkbox.to_keybind_indicator(
                'Ctrl-v', 'Paste', paste_text, base_style=button_style))
        fragments.extend(separator)

        fragments.extend(
            pw_console.widgets.checkbox.to_keybind_indicator(
                'Ctrl-c', 'Clear', clear_text, base_style=button_style))
        fragments.extend(separator)

        fragments.extend(
            pw_console.widgets.checkbox.to_keybind_indicator(
                'Enter', 'Run', run_code, base_style=button_style))
        fragments.extend(separator)

        return fragments

    @staticmethod
    def get_right_text_tokens(repl_pane):
        """Return right toolbar text."""
        fragments = []
        separator_text = [('', ' ')]
        if has_focus(repl_pane)():
            fragments.extend(
                pw_console.widgets.checkbox.to_keybind_indicator(
                    'F2', 'Settings'))
            fragments.extend(separator_text)
            fragments.extend(
                pw_console.widgets.checkbox.to_keybind_indicator(
                    'F3', 'History'))
        else:
            button_style = pw_console.style.get_button_style(repl_pane)

            focus = functools.partial(pw_console.mouse.focus_handler,
                                      repl_pane)
            fragments.append(
                (button_style + ' class:toolbar-button-decoration', ' ',
                 focus))
            fragments.append((
                # Style
                button_style + ' class:keyhelp',
                # Text
                'click to focus',
                # Mouse handler
                focus,
            ))
            fragments.append(
                (button_style + ' class:toolbar-button-decoration', ' ',
                 focus))
        return fragments

    def __init__(self, repl_pane):
        left_section_window = Window(
            content=FormattedTextControl(
                # Callable to get formatted text tuples.
                functools.partial(
                    ReplPaneBottomToolbarBar.get_left_text_tokens, repl_pane)),
            align=WindowAlign.LEFT,
            dont_extend_width=True,
        )

        center_section_window = Window(
            content=FormattedTextControl(
                # Callable to get formatted text tuples.
                functools.partial(
                    ReplPaneBottomToolbarBar.get_center_text_tokens,
                    repl_pane)),
            # Center text is left justified to appear just right of the left
            # section text.
            align=WindowAlign.LEFT,
            # Expand center section to fill space between the left and right
            # side of the toolbar.
            dont_extend_width=False,
        )

        right_section_window = Window(
            content=FormattedTextControl(
                # Callable to get formatted text tuples.
                functools.partial(
                    ReplPaneBottomToolbarBar.get_right_text_tokens,
                    repl_pane)),
            # Right side text should appear at the far right of the toolbar
            align=WindowAlign.RIGHT,
            dont_extend_width=True,
        )

        toolbar_vsplit = VSplit(
            [
                left_section_window,
                center_section_window,
                right_section_window,
            ],
            height=1,
            style=functools.partial(pw_console.style.get_toolbar_style,
                                    repl_pane),
            align=WindowAlign.LEFT,
        )

        super().__init__(
            # Content
            toolbar_vsplit,
            filter=Condition(lambda: repl_pane.show_bottom_toolbar))


@dataclass
class UserCodeExecution:
    """Class to hold a single user repl execution event."""
    input: str
    future: concurrent.futures.Future
    output: str
    stdout: str
    stderr: str
    stdout_check_task: Optional[concurrent.futures.Future] = None
    result_object: Optional[Any] = None
    exception_text: Optional[str] = None

    @property
    def is_running(self):
        return not self.future.done()

    def update_stdout(self, text: Optional[str]):
        if text:
            self.stdout = text

    def update_stderr(self, text: Optional[str]):
        if text:
            self.stderr = text


class ReplHSplit(HSplit):
    """PromptToolkit HSplit class with a write_to_screen function that saves the
    width and height of the container to be rendered.
    """
    def __init__(self, parent_window_pane, *args, **kwargs):
        # Save a reference to the parent LogPane.
        self.parent_window_pane = parent_window_pane
        super().__init__(*args, **kwargs)

    def write_to_screen(
        self,
        screen,
        mouse_handlers,
        write_position,
        parent_style: str,
        erase_bg: bool,
        z_index: Optional[int],
    ) -> None:
        # Save the width and height for the current render pass. This will be
        # used by the log pane to render the correct amount of log lines.
        self.parent_window_pane.update_pane_size(write_position.width,
                                                 write_position.height)
        # Continue writing content to the screen.
        super().write_to_screen(screen, mouse_handlers, write_position,
                                parent_style, erase_bg, z_index)


class ReplPane:
    """Pane for reading Python input."""

    # pylint: disable=too-many-instance-attributes,too-few-public-methods
    def __init__(
        self,
        application: Any,
        python_repl: PwPtPythonRepl,
        height: Optional[AnyDimension] = None,
        width: Optional[AnyDimension] = None,
        startup_message: Optional[str] = None,
    ) -> None:
        # Default width and height to 50% of the screen
        self.height = height if height else Dimension(weight=50)
        self.width = width if width else Dimension(weight=50)
        self.show_pane = True

        self.current_pane_width = 0
        self.current_pane_height = 0
        self.last_pane_width = 0
        self.last_pane_height = 0

        self.executed_code: List = []
        self.application = application
        self.show_top_toolbar = True
        self.show_bottom_toolbar = True

        self.pw_ptpython_repl = python_repl
        self.pw_ptpython_repl.set_repl_pane(self)

        self.startup_message = startup_message if startup_message else ''

        self.output_field = TextArea(
            text=self.startup_message,
            focusable=True,
            focus_on_click=True,
            scrollbar=True,
            wrap_lines=False,
            lexer=PygmentsLexer(PythonConsoleLexer),
        )

        # Additional keybindings for the text area.
        key_bindings = KeyBindings()

        @key_bindings.add('c-c')
        def _copy_selection(_event: KeyPressEvent) -> None:
            """Copy selected text."""
            self.copy_output_selection()

        self.output_field.control.key_bindings = key_bindings

        self.bottom_toolbar = ReplPaneBottomToolbarBar(self)

        self.results_toolbar = self._create_output_toolbar()

        self.progress_state = TASKS_CONTEXTVAR.get()

        # ReplPane root container
        self.container = ConditionalContainer(
            FloatContainer(
                # Horizontal split of all Repl pane sections.
                ReplHSplit(
                    self,
                    [
                        HSplit(
                            [
                                # 1. Repl Output
                                self.output_field,
                                # 2. Progress bars if any
                                ConditionalContainer(
                                    DynamicContainer(
                                        self.get_progress_bar_task_container),
                                    filter=Condition(
                                        lambda: not self.progress_state.
                                        all_tasks_complete)),
                                # 3. Static separator toolbar.
                                self.results_toolbar,
                            ],
                            # Output area only dimensions
                            height=self.get_output_height,
                        ),
                        HSplit(
                            [
                                # 3. Repl Input
                                self.pw_ptpython_repl,
                                # 4. Bottom toolbar
                                self.bottom_toolbar,
                            ],
                            # Input area only dimensions
                            height=self.get_input_height,
                        ),
                    ],
                    # Repl pane dimensions
                    height=lambda: self.height,
                    width=lambda: self.width,
                    style=functools.partial(pw_console.style.get_pane_style,
                                            self),
                ),
                floats=[
                    # Transparent float container that will focus on this
                    # ReplPane when clicked.
                    pw_console.widgets.focus_on_click_overlay.create_overlay(
                        self.pw_ptpython_repl,
                        self.output_field,
                    ),
                ]),
            filter=Condition(lambda: self.show_pane))

    def get_progress_bar_task_container(self):
        bar_container = self.progress_state.get_container()
        if bar_container:
            return bar_container
        return Window()

    def get_output_height(self) -> AnyDimension:
        # pylint: disable=no-self-use
        return Dimension(min=1)

    def get_input_height(self) -> AnyDimension:
        desired_max_height = 10
        # Check number of line breaks in the input buffer.
        input_line_count = self.pw_ptpython_repl.line_break_count()
        if input_line_count > desired_max_height:
            desired_max_height = input_line_count
        # Check if it's taller than the available space
        if desired_max_height > self.current_pane_height:
            # Leave space for minimum of
            #   1 line of content in the output
            #   + 1 for output toolbar
            #   + 1 for input toolbar
            desired_max_height = self.current_pane_height - 3

        if desired_max_height > 1:
            return Dimension(min=1, max=desired_max_height)
        # Fall back to at least a height of 1
        return Dimension(min=1)

    def update_pane_size(self, width, height):
        """Save width and height of the repl pane for the current UI render
        pass."""
        if width:
            self.last_pane_width = self.current_pane_width
            self.current_pane_width = width
        if height:
            self.last_pane_height = self.current_pane_height
            self.current_pane_height = height

    def _get_output_toolbar_fragments(self):
        toolbar_fragments = []

        focus = functools.partial(pw_console.mouse.focus_handler,
                                  self.output_field)
        copy_output = functools.partial(
            pw_console.widgets.mouse_handlers.on_click, self.copy_text)
        copy_selection = functools.partial(
            pw_console.widgets.mouse_handlers.on_click,
            self.copy_output_selection)

        separator = [('', '  ', focus)]

        # Title
        toolbar_fragments.extend(
            pw_console.style.get_pane_indicator(self.output_field,
                                                ' Python Results ', focus))
        toolbar_fragments.extend(separator)

        # Keybinds and functions
        button_style = pw_console.style.get_button_style(self)

        toolbar_fragments.extend(
            pw_console.widgets.checkbox.to_keybind_indicator(
                'Ctrl-Alt-c',
                'Copy All Output',
                copy_output,
                base_style=button_style))
        toolbar_fragments.extend(separator)

        if has_focus(self.output_field)():
            toolbar_fragments.extend(
                pw_console.widgets.checkbox.to_keybind_indicator(
                    'Ctrl-c',
                    'Copy Selected Text',
                    copy_selection,
                    base_style=button_style))
            toolbar_fragments.extend(separator)

            toolbar_fragments.extend(
                pw_console.widgets.checkbox.to_keybind_indicator(
                    'Shift+Arrows / Mouse Drag', 'Select Text'))
        else:
            toolbar_fragments.append((
                # Style
                button_style + ' class:keyhelp',
                # Text
                ' click to focus ',
                # Mouse handler
                functools.partial(pw_console.mouse.focus_handler,
                                  self.output_field),
            ))
        toolbar_fragments.extend(separator)

        return toolbar_fragments

    def _create_output_toolbar(self):
        toolbar_control = FormattedTextControl(
            self._get_output_toolbar_fragments)

        container = VSplit(
            [
                Window(
                    content=toolbar_control,
                    align=WindowAlign.LEFT,
                    dont_extend_width=False,
                    height=1,
                ),
            ],
            style=functools.partial(pw_console.style.get_toolbar_style, self),
        )
        return container

    def pane_title(self):  # pylint: disable=no-self-use
        return 'Python Repl'

    def menu_title(self):
        """Return the title to display in the Window menu."""
        return self.pane_title()

    def pane_subtitle(self):  # pylint: disable=no-self-use
        return ''

    def __pt_container__(self):
        """Return the prompt_toolkit container for this ReplPane.

        This allows self to be used wherever prompt_toolkit expects a container
        object."""
        return self.container

    def copy_output_selection(self):
        """Copy the highlighted text the python output buffer to the system
        clipboard."""
        clipboard_data = self.output_field.buffer.copy_selection()
        self.application.application.clipboard.set_data(clipboard_data)

    def copy_text(self):
        """Copy visible text in this window pane to the system clipboard."""
        self.application.application.clipboard.set_text(
            self.output_field.buffer.text)

    # pylint: disable=no-self-use
    def get_all_key_bindings(self) -> List:
        """Return all keybinds for this plugin."""
        # ptpython native bindings:
        # return [load_python_bindings(self.pw_ptpython_repl)]

        # Hand-crafted bindings for display in the HelpWindow:
        return [{
            'Execute code': ['Enter', 'Option-Enter', 'Meta-Enter'],
            'Reverse search history': ['Ctrl-R'],
            'Erase input buffer.': ['Ctrl-C'],
            'Show settings.': ['F2'],
            'Show history.': ['F3'],
        }]

    def get_all_menu_options(self):
        return []

    def after_render_hook(self):
        """Run tasks after the last UI render."""

    def run_code(self):
        """Trigger a repl code execution on mouse click."""
        self.pw_ptpython_repl.default_buffer.validate_and_handle()

    def ctrl_c(self):
        """Ctrl-C keybinding behavior."""
        # If there is text in the input buffer, clear it.
        if self.pw_ptpython_repl.default_buffer.text:
            self.clear_input_buffer()
        else:
            self.interrupt_last_code_execution()

    def paste_system_clipboard_to_input_buffer(self, erase_buffer=False):
        if erase_buffer:
            self.clear_input_buffer()

        clip_data = self.application.application.clipboard.get_data()
        self.pw_ptpython_repl.default_buffer.paste_clipboard_data(clip_data)

    def clear_input_buffer(self):
        # Erase input buffer.
        self.pw_ptpython_repl.default_buffer.reset()
        # Clear any displayed function signatures.
        self.pw_ptpython_repl.on_reset()

    def interrupt_last_code_execution(self):
        code = self._get_currently_running_code()
        if code:
            code.future.cancel()
            code.output = 'Canceled'
            self.progress_state.cancel_all_tasks()
        self.pw_ptpython_repl.clear_last_result()
        self.update_output_buffer('repl_pane.interrupt_last_code_execution')

    def _get_currently_running_code(self):
        for code in self.executed_code:
            if not code.future.done():
                return code
        return None

    def _get_executed_code(self, future):
        for code in self.executed_code:
            if code.future == future:
                return code
        return None

    def _log_executed_code(self, code, prefix=''):
        """Log repl command input text along with a prefix string."""
        text = self.get_output_buffer_text([code], show_index=False)
        _LOG.debug('[PYTHON] %s\n%s', prefix, text)

    async def periodically_check_stdout(self, user_code: UserCodeExecution,
                                        stdout_proxy, stderr_proxy):
        while not user_code.future.done():
            await asyncio.sleep(0.3)
            stdout_text_so_far = stdout_proxy.getvalue()
            stderr_text_so_far = stderr_proxy.getvalue()
            if stdout_text_so_far:
                user_code.update_stdout(stdout_text_so_far)
            if stderr_text_so_far:
                user_code.update_stderr(stderr_text_so_far)

            # if stdout_text_so_far or stderr_text_so_far:
            self.update_output_buffer('repl_pane.periodic_check')

    def append_executed_code(self, text, future, temp_stdout, temp_stderr):
        user_code = UserCodeExecution(input=text,
                                      future=future,
                                      output=None,
                                      stdout=None,
                                      stderr=None)

        background_stdout_check = asyncio.create_task(
            self.periodically_check_stdout(user_code, temp_stdout,
                                           temp_stderr))
        user_code.stdout_check_task = background_stdout_check
        self.executed_code.append(user_code)
        self._log_executed_code(user_code, prefix='START')

    def append_result_to_executed_code(
        self,
        _input_text,
        future,
        result_text,
        stdout_text='',
        stderr_text='',
        exception_text='',
        result_object=None,
    ):

        code = self._get_executed_code(future)
        if code:
            code.output = result_text
            code.stdout = stdout_text
            code.stderr = stderr_text
            code.exception_text = exception_text
            code.result_object = result_object
        self._log_executed_code(code, prefix='FINISH')
        self.update_output_buffer('repl_pane.append_result_to_executed_code')

    def get_output_buffer_text(self, code_items=None, show_index=True):
        content_width = (self.current_pane_width
                         if self.current_pane_width else 80)
        pprint_respecting_width = pprint.PrettyPrinter(
            indent=2, width=content_width).pformat

        executed_code = code_items or self.executed_code
        template = self.application.get_template('repl_output.jinja')
        return template.render(code_items=executed_code,
                               pprint_respecting_width=pprint_respecting_width,
                               show_index=show_index)

    def update_output_buffer(self, *unused_args):
        text = self.get_output_buffer_text()
        # Add an extra line break so the last cursor position is in column 0
        # instead of the end of the last line.
        text += '\n'
        self.output_field.buffer.set_document(
            Document(text=text, cursor_position=len(text)))

        self.application.redraw_ui()

    def input_or_output_has_focus(self) -> Condition:
        @Condition
        def test() -> bool:
            if has_focus(self.output_field)() or has_focus(
                    self.pw_ptpython_repl)():
                return True
            return False

        return test
