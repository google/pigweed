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
"""Float container that will focus on a given container on click."""

import functools

from prompt_toolkit.filters import (
    Condition,
    has_focus,
)
from prompt_toolkit.layout import (
    ConditionalContainer,
    Float,
    FormattedTextControl,
    Window,
    WindowAlign,
)

import pw_console.mouse


class FocusOnClickFloatContainer(ConditionalContainer):
    """Empty container rendered if the repl_pane is not in focus.

    This container should be rendered with transparent=True so nothing is shown
    to the user. Container is not rendered if the repl_pane is already in focus.
    """
    def __init__(self, target_container, target_container2=None):

        display_condition = Condition(lambda: not has_focus(target_container)
                                      ())
        if target_container2:
            display_condition = Condition(lambda: not (has_focus(
                target_container)() or has_focus(target_container2)()))
        empty_text = FormattedTextControl([(
            'class:pane_inactive',  # Style
            # Text here must be a printable character or the mouse handler won't
            # work.
            ' ',
            functools.partial(pw_console.mouse.focus_handler,
                              target_container),  # Mouse handler
        )])

        super().__init__(
            Window(
                empty_text,
                # Draw the empty space in the upper right corner
                align=WindowAlign.RIGHT,
                # Make sure window fills all available space.
                dont_extend_width=False,
                dont_extend_height=False,
            ),
            filter=display_condition,
        )


def create_overlay(target_container, target_container2=None):
    """Create a transparent FocusOnClickFloatContainer.

    The target_container will be focused when clicked. The overlay float will be
    hidden if target_container is already in focus.
    """
    return Float(
        # This is drawn as the full size of the ReplPane
        FocusOnClickFloatContainer(target_container, target_container2),
        transparent=True,
        # Draw the empty space in the bottom right corner.
        # Distance to each edge, fill the whole container.
        left=0,
        top=0,
        right=0,
        bottom=0,
    )
