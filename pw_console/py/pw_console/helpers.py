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

    # While the remaining character count won't fit on the screen:
    while (remaining_width + prefix_width) > screen_width:
        remaining_width += prefix_width
        remaining_width -= screen_width
        total_height += 1

    # Add one for the last line that is < screen_width
    return total_height + 1
