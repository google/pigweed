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
"""Helper functions."""

from prompt_toolkit.filters import has_focus


def remove_formatting(formatted_text):
    """Throw away style info from formatted text tuples."""
    return ''.join([formatted_tuple[1] for formatted_tuple in formatted_text])  # pylint: disable=not-an-iterable


def get_line_height(text_width, screen_width, prefix_width):
    """Calculates line height for a string with line wrapping enabled."""
    if text_width == 0:
        return 0
    # If text will fit on the screen without wrapping.
    if text_width < screen_width:
        return 1

    # Start with height of 1 row.
    total_height = 1
    # One screen_width of characters has been displayed.
    remaining_width = text_width - screen_width

    # Assume zero width prefix if it's >= width of the screen.
    if prefix_width >= screen_width:
        prefix_width = 0

    # While the remaining character count won't fit on the screen:
    while (remaining_width + prefix_width) > screen_width:
        remaining_width += prefix_width
        remaining_width -= screen_width
        total_height += 1

    # Add one for the last line that is < screen_width
    return total_height + 1


def get_toolbar_style(pt_container) -> str:
    """Return the style class for a toolbar if pt_container is in focus."""
    if has_focus(pt_container.__pt_container__())():
        return 'class:toolbar_active'
    return 'class:toolbar_inactive'


def get_pane_style(pt_container) -> str:
    """Return the style class for a pane title if pt_container is in focus."""
    if has_focus(pt_container.__pt_container__())():
        return 'class:pane_active'
    return 'class:pane_inactive'


def get_pane_indicator(pt_container, title, mouse_handler=None):
    """Return formatted text for a pane indicator and title."""
    if mouse_handler:
        inactive_indicator = ('class:pane_indicator_inactive', ' ',
                              mouse_handler)
        active_indicator = ('class:pane_indicator_active', ' ', mouse_handler)
        inactive_title = ('class:pane_title_inactive', title, mouse_handler)
        active_title = ('class:pane_title_active', title, mouse_handler)
    else:
        inactive_indicator = ('class:pane_indicator_inactive', ' ')
        active_indicator = ('class:pane_indicator_active', ' ')
        inactive_title = ('class:pane_title_inactive', title)
        active_title = ('class:pane_title_active', title)

    if has_focus(pt_container.__pt_container__())():
        return [active_indicator, active_title]
    return [inactive_indicator, inactive_title]


def to_checkbox(checked: bool, mouse_handler=None):
    default_style = 'class:checkbox'
    checked_style = 'class:checkbox-checked'
    text = '[x] ' if checked else '[ ] '
    style = checked_style if checked else default_style
    if mouse_handler:
        return (style, text, mouse_handler)
    return (style, text)


def to_checkbox_text(checked: bool):
    return to_checkbox(checked)[1]
