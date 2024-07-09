# Copyright 2024 The Pigweed Authors
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
"""Interactive input prompts with up/down navigation and validation."""

import sys

from prompt_toolkit import PromptSession
from prompt_toolkit.history import InMemoryHistory
from prompt_toolkit.validation import Validator


def interactive_index_select(
    selection_lines: list[str],
    prompt_text: str = (
        '\nEnter an item index or press up/down (Ctrl-C to cancel)\n> '
    ),
) -> tuple[int, str]:
    """Display an interactive prompt to the user with up down navigation.

    Presents an interactive prompt to the user on the terminal where
    the index of and item can be typed in or items selected with the
    up & down keys. If the user enters Ctrl-c or Ctrl-d the process
    will exit with sys.exit(1).

    Args:
      selection_lines: List of strings to present to the user for
          selection. Indexes will be prepended to each line starting
          from 1 before displaying to the user.
      prompt_text: String to display before the prompt cursor.

    Returns: a tuple containing the index of the item selected and the
      selected item text.
    """

    lines_with_indexes = list(
        f'{i+1} - {line}' for i, line in enumerate(selection_lines)
    )

    # Add the items to a prompt_toolkit history so they can be
    # selected with up and down.
    history = InMemoryHistory()
    # Add in reverse order so pressing up selects the first item
    # followed by the second. Otherwise pressing up selects the last
    # item in the printed list.
    for line in reversed(lines_with_indexes):
        history.append_string(line)

    for line in lines_with_indexes:
        print(' ', line)

    def input_index(text: str) -> int:
        """Convert input to a single index.

        Args:
            text: str beginning a single integer followed by whitespace.

        Returns:
            The single integer as an int minus 1 for use as a list index.
        """
        parts = text.split()
        index = int(parts[0].strip())
        return index - 1

    def is_valid_item(text: str) -> bool:
        """Returns true if the input line is a valid entry."""
        return (
            bool(text)
            and any(line.startswith(text) for line in lines_with_indexes)
            and input_index(text) < len(lines_with_indexes)
        )

    validator = Validator.from_callable(
        is_valid_item,
        error_message="Not a valid item.",
        move_cursor_to_end=True,
    )

    session: PromptSession = PromptSession(
        history=history,
        enable_history_search=True,
        validator=validator,
        validate_while_typing=False,
    )

    try:
        selected_text = session.prompt(prompt_text)
    except (EOFError, KeyboardInterrupt):
        # Cancel with Ctrl-c or Ctrl-d
        sys.exit(1)

    selected_index = input_index(selected_text)
    original_text = selection_lines[selected_index]

    return (selected_index, original_text)
