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
"""Text formatting functions."""

import re
from typing import Iterable

from prompt_toolkit.formatted_text import StyleAndTextTuples
from prompt_toolkit.formatted_text.base import OneStyleAndTextTuple

_ANSI_SEQUENCE_REGEX = re.compile(r'\x1b[^m]*m')


def strip_ansi(text: str):
    """Strip out ANSI escape sequences."""
    return _ANSI_SEQUENCE_REGEX.sub('', text)


def fill_character_width(line_fragments: StyleAndTextTuples,
                         fragment_width: int,
                         window_width: int,
                         remaining_width: int,
                         line_wrapping: bool,
                         horizontal_scroll_amount: int = 0,
                         add_cursor: bool = False) -> StyleAndTextTuples:
    """Fill line to the width of the window using spaces."""
    if add_cursor:
        # Add a cursor to this line by adding SetCursorPosition fragment.
        line_fragments_remainder = line_fragments
        line_fragments = [('[SetCursorPosition]', '')]
        # Use extend to keep types happy.
        line_fragments.extend(line_fragments_remainder)

    # Calculate the number of spaces to add at the end.
    empty_characters = window_width - fragment_width
    # If wrapping is on, use remaining_width
    if line_wrapping and (fragment_width > window_width):
        empty_characters = remaining_width

    empty_characters += horizontal_scroll_amount

    if empty_characters > 0:
        # Replace line ending tuple ('', '\n') with additional empty
        # spaces to inherit the selected line background.
        line_fragments[-1] = ('', ' ' * empty_characters + '\n')

    return line_fragments


def flatten_formatted_text_tuples(
        lines: Iterable[StyleAndTextTuples]) -> StyleAndTextTuples:
    """Flatten a list of lines of FormattedTextTuples

    This function will also remove trailing newlines to avoid displaying extra
    empty lines in prompt_toolkit containers.
    """
    fragments: StyleAndTextTuples = []

    # Return empty list if lines is empty.
    if not lines:
        return fragments

    for line_fragments in lines:
        # Append all FormattedText tuples for this line.
        for fragment in line_fragments:
            fragments.append(fragment)

    # Strip off any trailing line breaks
    last_fragment: OneStyleAndTextTuple = fragments[-1]
    style = last_fragment[0]
    text = last_fragment[1].rstrip('\n')
    fragments[-1] = (style, text)
    return fragments


def remove_formatting(formatted_text):
    """Throw away style info from prompt_toolkit formatted text tuples."""
    return ''.join([formatted_tuple[1] for formatted_tuple in formatted_text])  # pylint: disable=not-an-iterable


def get_line_height(text_width, screen_width, prefix_width):
    """Calculates line height for a string with line wrapping enabled."""
    if text_width == 0:
        return 0

    # If text will fit on the screen without wrapping.
    if text_width <= screen_width:
        return 1, screen_width - text_width

    # Assume zero width prefix if it's >= width of the screen.
    if prefix_width >= screen_width:
        prefix_width = 0

    # Start with height of 1 row.
    total_height = 1

    # One screen_width of characters (with no prefix) is displayed first.
    remaining_width = text_width - screen_width

    # While we have caracters remaining to be displayed
    while remaining_width > 0:
        # Add the new indentation prefix
        remaining_width += prefix_width
        # Display this line
        remaining_width -= screen_width
        # Add a line break
        total_height += 1

    # Remaining characters is what's left below zero.
    return (total_height, abs(remaining_width))
