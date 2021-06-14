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

import logging
from functools import partial
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

_LOG = logging.getLogger(__package__)

_Namespace = Dict[str, Any]
_GetNamespace = Callable[[], _Namespace]


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
            '',
            # Text
            ' ',
            # Mouse handler
            partial(mouse_focus_handler, repl_pane),
        )])

        super().__init__(
            Window(empty_text),
            filter=Condition(lambda: not has_focus(repl_pane)()),
        )


class ReplPaneBottomToolbarBar(ConditionalContainer):
    """Repl pane bottom toolbar."""
    @staticmethod
    def get_center_text_tokens(repl_pane):
        """Return toolbar text showing if the ReplPane is in focus or not."""
        focused_text = (
            # Style
            "",
            # Text
            " [FOCUSED] ",
            # Mouse handler
            partial(mouse_focus_handler, repl_pane),
        )

        out_of_focus_text = (
            # Style
            'class:keyhelp',
            # Text
            ' [click to focus] ',
            # Mouse handler
            partial(mouse_focus_handler, repl_pane),
        )

        if has_focus(repl_pane)():
            return [focused_text]
        return [out_of_focus_text]

    def __init__(self, repl_pane):
        left_section_text = FormattedTextControl([(
            # Style
            'class:logo',
            # Text
            ' Python Input ',
            # Mouse handler
            partial(mouse_focus_handler, repl_pane),
        )])

        center_section_text = FormattedTextControl(
            # Callable to get formatted text tuples.
            partial(ReplPaneBottomToolbarBar.get_center_text_tokens,
                    repl_pane))

        right_section_text = FormattedTextControl([(
            # Style
            'class:bottom_toolbar_colored_text',
            # Text
            ' [Enter]: run code ',
        )])

        left_section_window = Window(
            content=left_section_text,
            align=WindowAlign.LEFT,
            dont_extend_width=True,
        )

        center_section_window = Window(
            content=center_section_text,
            # Center text is left justified to appear just right of the left
            # section text.
            align=WindowAlign.LEFT,
            # Expand center section to fill space between the left and right
            # side of the toolbar.
            dont_extend_width=False,
        )

        right_section_window = Window(
            content=right_section_text,
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
            style='class:bottom_toolbar',
            align=WindowAlign.LEFT,
        )

        super().__init__(
            # Content
            toolbar_vsplit,
            filter=Condition(lambda: repl_pane.show_bottom_toolbar))


class ReplPane:
    """Pane for reading Python input."""

    # pylint: disable=too-many-instance-attributes,too-few-public-methods
    def __init__(
            self,
            application: Any,
            # TODO: Include ptpython repl.
            # python_repl: PwPtPythonRepl,
            # TODO: Make the height of input+output windows match the log pane
            # height. (Using minimum output height of 5 for now).
            output_height: Optional[AnyDimension] = Dimension(preferred=5),
            # TODO: Figure out how to resize ptpython input field.
            _input_height: Optional[AnyDimension] = None,
            # Default width and height to 50% of the screen
            height: Optional[AnyDimension] = Dimension(weight=50),
            width: Optional[AnyDimension] = Dimension(weight=50),
    ) -> None:
        self.height = height
        self.width = width

        self.executed_code: List = []
        self.application = application
        self.show_top_toolbar = True
        self.show_bottom_toolbar = True

        # TODO: Include ptpython repl.
        self.pw_ptpython_repl = Window(content=FormattedTextControl(
            [('', '>>> Repl input buffer')], focusable=True))
        self.last_error_output = ""

        self.output_field = TextArea(
            text='Repl output buffer',
            style='class:output-field',
            height=output_height,
            # text=help_text,
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
                    Window(
                        content=FormattedTextControl([(
                            # Style
                            'class:logo',
                            # Text
                            ' Python Results ',
                        )]),
                        height=1,
                        style='class:menu-bar'),
                    # 3. Repl Input
                    self.pw_ptpython_repl,
                    # 4. Bottom toolbar
                    self.bottom_toolbar,
                ],
                height=self.height,
                width=self.width,
            ),
            floats=[
                # Transparent float container that will focus on the repl_pane
                # when clicked. It is hidden if already in focus.
                Float(
                    FocusOnClickFloatContainer(self),
                    transparent=True,
                    # Full size of the ReplPane minus one line for the bottom
                    # toolbar.
                    right=0,
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
        return []

    def after_render_hook(self):
        """Run tasks after the last UI render."""
