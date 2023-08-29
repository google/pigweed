#!/usr/bin/env python3

# Copyright 2023 The Pigweed Authors
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
"""Restructured Text Formatting."""

import argparse
from dataclasses import dataclass, field
import difflib
from functools import cached_property
from pathlib import Path
import textwrap
from typing import List, Optional

from pw_presubmit.tools import colorize_diff

DEFAULT_TAB_WIDTH = 8
CODE_BLOCK_INDENTATION = 3


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument(
        '--tab-width',
        default=DEFAULT_TAB_WIDTH,
        help='Number of spaces to use when converting tab characters.',
    )
    parser.add_argument(
        '--diff',
        action='store_true',
        help='Print a diff of formatting changes.',
    )
    parser.add_argument(
        '-i',
        '--in-place',
        action='store_true',
        help='Replace existing file with the reformatted copy.',
    )
    parser.add_argument(
        'rst_files',
        nargs='+',
        default=[],
        type=Path,
        help='Paths to rst files.',
    )

    return parser.parse_args()


def _indent_amount(line: str) -> int:
    return len(line) - len(line.lstrip())


def _reindent(input_text: str, amount: int) -> str:
    text = ''
    for line in textwrap.dedent(input_text).splitlines():
        if len(line.strip()) == 0:
            text += '\n'
            continue
        text += ' ' * amount
        text += line
        text += '\n'
    return text


def _strip_trailing_whitespace(line: str) -> str:
    return line.rstrip() + '\n'


@dataclass
class CodeBlock:
    """Store a single code block."""

    directive_lineno: int
    directive_line: str
    first_line_indent: Optional[int] = None
    end_lineno: Optional[int] = None
    option_lines: List[str] = field(default_factory=list)
    code_lines: List[str] = field(default_factory=list)

    def __post_init__(self) -> None:
        self._blank_line_after_options_found = False

    def finished(self) -> bool:
        return self.end_lineno is not None

    def append_line(self, index: int, line: str) -> None:
        """Process a line for this code block."""
        # Check if outside the code block (indentation is less):
        if (
            self.first_line_indent is not None
            and len(line.strip()) > 0
            and _indent_amount(line) < self.first_line_indent
        ):
            # Code block ended
            self.end_lineno = index
            return

        # If first line indent hasn't been found
        if self.first_line_indent is None:
            # Check if the first word is a directive option.
            # E.g. :caption:
            line_words = line.split()
            if (
                line_words
                and line_words[0].startswith(':')
                and line_words[0].endswith(':')
            ):
                self.option_lines.append(line.rstrip())
                return
            # Check for a blank line
            if len(line.strip()) == 0:
                if (
                    self.option_lines
                    and not self._blank_line_after_options_found
                ):
                    self._blank_line_after_options_found = True
                return
            # Check for a line that is a continuation of a previous option.
            if self.option_lines and not self._blank_line_after_options_found:
                self.option_lines.append(line.rstrip())
                return

            self.first_line_indent = _indent_amount(line)

        # Save this line as code.
        self.code_lines.append(line.rstrip())

    @cached_property
    def directive_indent_amount(self) -> int:
        return _indent_amount(self.directive_line)

    def options_block_text(self) -> str:
        return _reindent(
            input_text='\n'.join(self.option_lines),
            amount=self.directive_indent_amount + CODE_BLOCK_INDENTATION,
        )

    def code_block_text(self) -> str:
        return _reindent(
            input_text='\n'.join(self.code_lines),
            amount=self.directive_indent_amount + CODE_BLOCK_INDENTATION,
        )

    def to_text(self) -> str:
        text = ''
        text += self.directive_line
        if self.option_lines:
            text += self.options_block_text()
        text += '\n'
        text += self.code_block_text()
        text += '\n'
        return text

    def __repr__(self) -> str:
        return self.to_text()


def reindent_code_blocks(in_text: str) -> str:
    """Reindent code blocks to 3 spaces."""
    out_text = ''
    current_block: Optional[CodeBlock] = None
    for index, line in enumerate(in_text.splitlines(keepends=True)):
        # If a code block is active, process this line.
        if current_block:
            current_block.append_line(index, line)
            if current_block.finished():
                out_text += current_block.to_text()
                # This line wasn't part of the code block, process as normal.
                out_text += _strip_trailing_whitespace(line)
                # Erase this code_block variable
                current_block = None
        # Check for new code block start
        elif line.lstrip().startswith('.. code') and line.rstrip().endswith(
            '::'
        ):
            current_block = CodeBlock(
                directive_lineno=index, directive_line=line
            )
            continue
        else:
            out_text += _strip_trailing_whitespace(line)
    # If the document ends with a code block it may still need to be written.
    if current_block is not None:
        out_text += current_block.to_text()
    return out_text


def reformat_rst(
    file_name: Path,
    diff: bool = False,
    in_place: bool = False,
    tab_width: int = DEFAULT_TAB_WIDTH,
) -> List[str]:
    """Reformat an rst file.

    Returns a list of diff lines."""
    in_text = file_name.read_text()

    # Replace tabs with spaces
    out_text = in_text.replace('\t', ' ' * tab_width)

    # Indent .. code-block:: directives.
    out_text = reindent_code_blocks(in_text)

    result_diff = list(
        difflib.unified_diff(
            in_text.splitlines(True),
            out_text.splitlines(True),
            f'{file_name}  (original)',
            f'{file_name}  (reformatted)',
        )
    )
    if diff and result_diff:
        print(''.join(colorize_diff(result_diff)))

    if in_place:
        file_name.write_text(out_text)

    return result_diff


def rst_format_main(
    rst_files: List[Path],
    diff: bool = False,
    in_place: bool = False,
    tab_width: int = DEFAULT_TAB_WIDTH,
) -> None:
    for rst_file in rst_files:
        reformat_rst(rst_file, diff, in_place, tab_width)


if __name__ == '__main__':
    rst_format_main(**vars(_parse_args()))
