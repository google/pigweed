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

_ANSI_SEQUENCE_REGEX = re.compile(r'\x1b[^m]*m')


def strip_ansi(text: str):
    """Strip out ANSI escape sequences."""
    return _ANSI_SEQUENCE_REGEX.sub('', text)


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
