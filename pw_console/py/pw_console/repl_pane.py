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
from prompt_toolkit.mouse_events import MouseEvent, MouseEventType
from prompt_toolkit.layout.dimension import AnyDimension
from prompt_toolkit.widgets import TextArea
from prompt_toolkit.layout import (
    ConditionalContainer,
    Dimension,
    Float,
    FloatContainer,
    FormattedTextControl,
    HSplit,
    VSplit,
    Window,
    WindowAlign,
)
from prompt_toolkit.lexers import PygmentsLexer  # type: ignore
from pygments.lexers.python import PythonLexer  # type: ignore

from pw_console.helpers import (
    get_pane_indicator,
    get_pane_style,
    get_toolbar_style,
)
from pw_console.pw_ptpython_repl import PwPtPythonRepl

_LOG = logging.getLogger(__package__)

_Namespace = Dict[str, Any]
_GetNamespace = Callable[[], _Namespace]

_OUTPUT_TEMPLATE_PATH = (Path(__file__).parent / 'templates' /
                         'repl_output.jinja')
with _OUTPUT_TEMPLATE_PATH.open() as tmpl:
    OUTPUT_TEMPLATE = tmpl.read()


def mouse_focus_handler(repl_pane, mouse_event: MouseEvent):
    """Focus the repl_pane on click."""
    if not has_focus(repl_pane)():
        if mouse_event.event_type == MouseEventType.MOUSE_UP:
            repl_pane.application.application.layout.focus(repl_pane)
            return None
    return NotImplemented


class FocusOnClickFloatContainer(ConditionalContainer):
    """Empty container rendered if the repl_pane is not in focus.

    This container should be rendered with transparent=True so nothing is shown
    to the user. Container is not rendered if the repl_pane is already in focus.
    """
    def __init__(self, repl_pane):

        empty_text = FormattedTextControl([(
            # Style
            'class:default',
            # Text
            ' ',
            # Mouse handler
            functools.partial(mouse_focus_handler, repl_pane),
        )])

        super().__init__(
            Window(empty_text),
            filter=Condition(lambda: not has_focus(repl_pane)()),
        )


class ReplPaneBottomToolbarBar(ConditionalContainer):
    """Repl pane bottom toolbar."""
    @staticmethod
    def get_left_text_tokens(repl_pane):
        """Return toolbar indicator and title."""

        title = ' Python Input '
        mouse_handler = functools.partial(mouse_focus_handler, repl_pane)
        return get_pane_indicator(repl_pane, title, mouse_handler)

    @staticmethod
    def get_center_text_tokens(repl_pane):
        """Return toolbar text showing if the ReplPane is in focus or not."""
        focused_text = [
            (
                # Style
                '',
                # Text
                '[FOCUSED] ',
                # Mouse handler
                functools.partial(mouse_focus_handler, repl_pane),
            ),
            ('class:keybind', 'enter'),
            ('class:keyhelp', ':Run code'),
        ]

        out_of_focus_text = [(
            # Style
            'class:keyhelp',
            # Text
            '[click to focus] ',
            # Mouse handler
            functools.partial(mouse_focus_handler, repl_pane),
        )]

        if has_focus(repl_pane)():
            return focused_text
        return out_of_focus_text

    @staticmethod
    def get_right_text_tokens(repl_pane):
        """Return right toolbar text."""
        if has_focus(repl_pane)():
            return [
                ('class:keybind', 'F2'),
                ('class:keyhelp', ':Settings '),
                ('class:keybind', 'F3'),
                ('class:keyhelp', ':History '),
            ]
        return []

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
            style=functools.partial(get_toolbar_style, repl_pane),
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
        output_height: Optional[AnyDimension] = Dimension(preferred=5),
        # TODO(tonymd): Figure out how to resize ptpython input field.
        _input_height: Optional[AnyDimension] = None,
        # Default width and height to 50% of the screen
        height: Optional[AnyDimension] = Dimension(weight=50),
        width: Optional[AnyDimension] = Dimension(weight=50),
        startup_message: Optional[str] = None,
    ) -> None:
        self.height = height
        self.width = width

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
        self.container = FloatContainer(
            # Horizontal split of all Repl pane sections.
            HSplit(
                [
                    # 1. Repl Output
                    self.output_field,
                    # 2. Static separator toolbar.
                    VSplit(
                        [
                            Window(
                                content=FormattedTextControl(
                                    functools.partial(get_pane_indicator, self,
                                                      ' Python Results ')),
                                align=WindowAlign.LEFT,
                                dont_extend_width=True,
                                height=1,
                            ),
                        ],
                        style=functools.partial(get_toolbar_style, self),
                    ),
                    # 3. Repl Input
                    self.pw_ptpython_repl,
                    # 4. Bottom toolbar
                    self.bottom_toolbar,
                ],
                height=self.height,
                width=self.width,
                style=functools.partial(get_pane_style, self),
            ),
            floats=[
                # Transparent float container that will focus on the repl_pane
                # when clicked. It is hidden if already in focus.
                Float(
                    # This is drawn as the full size of the ReplPane
                    FocusOnClickFloatContainer(self),
                    transparent=True,
                    # Draw the empty space in the bottom right corner.
                    right=1,
                    left=0,
                    top=0,
                    bottom=1,
                ),
            ])

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
            'Erase input buffer.': ['ControlC'],
            'Show ptpython settings.': ['F2'],
            'Show ptpython history.': ['F3'],
            'Execute code': ['Enter', 'OptionEnter', 'MetaEnter'],
            'Reverse search history': ['ControlR'],
        }]

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
        text = self.get_output_buffer_text([code], show_index=False)
        _LOG.info('[PYTHON] %s\n%s', prefix, text)

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
