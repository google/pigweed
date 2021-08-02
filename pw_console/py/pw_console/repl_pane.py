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

import concurrent
import functools
import logging
from dataclasses import dataclass
from pathlib import Path
from typing import (
    Any,
    Callable,
    Dict,
    List,
    Optional,
)

from jinja2 import Template
from prompt_toolkit.filters import (
    Condition,
    has_focus,
)
from prompt_toolkit.document import Document
from prompt_toolkit.layout.dimension import AnyDimension
from prompt_toolkit.widgets import TextArea
from prompt_toolkit.layout import (
    ConditionalContainer,
    Dimension,
    FloatContainer,
    FormattedTextControl,
    HSplit,
    VSplit,
    Window,
    WindowAlign,
)
from prompt_toolkit.lexers import PygmentsLexer  # type: ignore
from pygments.lexers.python import PythonLexer  # type: ignore

import pw_console.mouse
import pw_console.style
import pw_console.widgets.focus_on_click_overlay
from pw_console.pw_ptpython_repl import PwPtPythonRepl

_LOG = logging.getLogger(__package__)

_Namespace = Dict[str, Any]
_GetNamespace = Callable[[], _Namespace]

_OUTPUT_TEMPLATE_PATH = (Path(__file__).parent / 'templates' /
                         'repl_output.jinja')
with _OUTPUT_TEMPLATE_PATH.open() as tmpl:
    OUTPUT_TEMPLATE = tmpl.read()


class ReplPaneBottomToolbarBar(ConditionalContainer):
    """Repl pane bottom toolbar."""
    @staticmethod
    def get_left_text_tokens(repl_pane):
        """Return toolbar indicator and title."""

        title = ' Python Input '
        mouse_handler = functools.partial(pw_console.mouse.focus_handler,
                                          repl_pane)
        return pw_console.style.get_pane_indicator(repl_pane, title,
                                                   mouse_handler)

    @staticmethod
    def get_center_text_tokens(repl_pane):
        """Return toolbar text showing if the ReplPane is in focus or not."""
        focused_text = [
            (
                # Style
                '',
                # Text
                ' ',
                # Mouse handler
                functools.partial(pw_console.mouse.focus_handler, repl_pane),
            ),
        ]

        focused_text.extend(
            pw_console.widgets.checkbox.to_keybind_indicator('Enter', 'Run'))

        if has_focus(repl_pane)():
            return focused_text
        return [('', '')]

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
            fragments.append((
                # Style
                'class:keyhelp',
                # Text
                '[click to focus] ',
                # Mouse handler
                functools.partial(pw_console.mouse.focus_handler, repl_pane),
            ))
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

    @property
    def is_running(self):
        return not self.future.done()


class ReplPane:
    """Pane for reading Python input."""

    # pylint: disable=too-many-instance-attributes,too-few-public-methods
    def __init__(
        self,
        application: Any,
        python_repl: PwPtPythonRepl,
        # TODO(tonymd): Make the height of input+output windows match the log
        # pane height. (Using minimum output height of 5 for now).
        output_height: Optional[AnyDimension] = Dimension(min=5, weight=70),
        # TODO(tonymd): Figure out how to resize ptpython input field.
        _input_height: Optional[AnyDimension] = None,
        # Default width and height to 50% of the screen
        height: Optional[AnyDimension] = None,
        width: Optional[AnyDimension] = None,
        startup_message: Optional[str] = None,
    ) -> None:
        self.height = height if height else Dimension(weight=50)
        self.width = width if width else Dimension(weight=50)
        self.show_pane = True

        self.executed_code: List = []
        self.application = application
        self.show_top_toolbar = True
        self.show_bottom_toolbar = True

        self.pw_ptpython_repl = python_repl
        self.pw_ptpython_repl.set_repl_pane(self)

        self.startup_message = startup_message if startup_message else ''

        self.output_field = TextArea(
            height=output_height,
            text=self.startup_message,
            focusable=False,
            scrollbar=True,
            lexer=PygmentsLexer(PythonLexer),
        )

        self.bottom_toolbar = ReplPaneBottomToolbarBar(self)

        # ReplPane root container
        self.container = ConditionalContainer(
            FloatContainer(
                # Horizontal split of all Repl pane sections.
                HSplit(
                    [
                        HSplit([
                            # 1. Repl Output
                            self.output_field,
                            # 2. Static separator toolbar.
                            VSplit(
                                [
                                    Window(
                                        content=FormattedTextControl(
                                            functools.partial(
                                                pw_console.style.
                                                get_pane_indicator, self,
                                                ' Python Results ')),
                                        align=WindowAlign.LEFT,
                                        dont_extend_width=True,
                                        height=1,
                                    ),
                                ],
                                style=functools.partial(
                                    pw_console.style.get_toolbar_style, self),
                            ),
                        ]),
                        HSplit([
                            # 3. Repl Input
                            self.pw_ptpython_repl,
                            # 4. Bottom toolbar
                            self.bottom_toolbar,
                        ]),
                    ],
                    height=lambda: self.height,
                    width=lambda: self.width,
                    style=functools.partial(pw_console.style.get_pane_style,
                                            self),
                ),
                floats=[
                    # Transparent float container that will focus on this
                    # ReplPane when clicked.
                    pw_console.widgets.focus_on_click_overlay.create_overlay(
                        self),
                ]),
            filter=Condition(lambda: self.show_pane))

    def pane_title(self):  # pylint: disable=no-self-use
        return 'Python Repl'

    def menu_title(self):
        """Return the title to display in the Window menu."""
        return self.pane_title()

    def pane_subtitle(self):  # pylint: disable=no-self-use
        return ''

    def __pt_container__(self):
        """Return the prompt_toolkit container for this ReplPane."""
        return self.container

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

    def ctrl_c(self):
        """Ctrl-C keybinding behavior."""
        # If there is text in the input buffer, clear it.
        if self.pw_ptpython_repl.default_buffer.text:
            self.clear_input_buffer()
        else:
            self.interrupt_last_code_execution()

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
        self.pw_ptpython_repl.clear_last_result()
        self.update_output_buffer()

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

    def append_executed_code(self, text, future):
        user_code = UserCodeExecution(input=text,
                                      future=future,
                                      output=None,
                                      stdout=None,
                                      stderr=None)
        self.executed_code.append(user_code)
        self._log_executed_code(user_code, prefix='START')

    def append_result_to_executed_code(self,
                                       _input_text,
                                       future,
                                       result_text,
                                       stdout_text='',
                                       stderr_text=''):
        code = self._get_executed_code(future)
        if code:
            code.output = result_text
            code.stdout = stdout_text
            code.stderr = stderr_text
        self._log_executed_code(code, prefix='FINISH')
        self.update_output_buffer()

    def get_output_buffer_text(self, code_items=None, show_index=True):
        executed_code = code_items or self.executed_code
        template = Template(OUTPUT_TEMPLATE,
                            trim_blocks=True,
                            lstrip_blocks=True)
        return template.render(code_items=executed_code,
                               show_index=show_index).strip()

    def update_output_buffer(self):
        text = self.get_output_buffer_text()
        self.output_field.buffer.document = Document(text=text,
                                                     cursor_position=len(text))
        self.application.redraw_ui()
