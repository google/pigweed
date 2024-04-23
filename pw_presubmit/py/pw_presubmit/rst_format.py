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
from typing import Iterable

from pw_presubmit.tools import colorize_diff

TAB_WIDTH = 8  # Number of spaces to use for \t replacement
CODE_BLOCK_INDENTATION = 3


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)

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


def _reindent(input_text: str, amount: int) -> Iterable[str]:
    for line in textwrap.dedent(input_text).splitlines():
        if len(line.strip()) == 0:
            yield '\n'
            continue
        yield f'{" " * amount}{line}\n'


def _fix_whitespace(line: str) -> str:
    return line.rstrip().replace('\t', ' ' * TAB_WIDTH) + '\n'


@dataclass
class CodeBlock:
    """Store a single code block."""

    directive_lineno: int
    directive_line: str
    first_line_indent: int | None = None
    end_lineno: int | None = None
    option_lines: list[str] = field(default_factory=list)
    code_lines: list[str] = field(default_factory=list)

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
                # In case the first word starts with two colons '::'
                and len(line_words[0]) > 2
            ):
                self.option_lines.append(line.rstrip())
                return

            # Step 1: Check for a blank line
            if len(line.strip()) == 0:
                if (
                    self.option_lines
                    and not self._blank_line_after_options_found
                ):
                    self._blank_line_after_options_found = True
                return

            # Step 2: Check for a line that is a continuation of a previous
            # option.
            if self.option_lines and not self._blank_line_after_options_found:
                self.option_lines.append(line.rstrip())
                return

            # Step 3: Check a line with content.
            if len(line.strip()) > 0:
                # Line is not a directive and not blank: it is content.
                # Flag the end of the options
                self._blank_line_after_options_found = True

            # Set the content indentation amount.
            self.first_line_indent = _indent_amount(line)

        # Save this line as code.
        self.code_lines.append(self._clean_up_line(line))

    def _clean_up_line(self, line: str) -> str:
        line = line.rstrip()
        if not self._keep_codeblock_tabs:
            line = line.replace('\t', ' ' * TAB_WIDTH)
        return line

    @cached_property
    def _keep_codeblock_tabs(self) -> bool:
        """True if tabs should NOT be replaced; keep for 'none' or 'go'."""
        return 'none' in self.directive_line or 'go' in self.directive_line

    @cached_property
    def directive_indent_amount(self) -> int:
        return _indent_amount(self.directive_line)

    def options_block_lines(self) -> Iterable[str]:
        yield from _reindent(
            input_text='\n'.join(self.option_lines),
            amount=self.directive_indent_amount + CODE_BLOCK_INDENTATION,
        )

    def code_block_lines(self) -> Iterable[str]:
        yield from _reindent(
            input_text='\n'.join(self.code_lines),
            amount=self.directive_indent_amount + CODE_BLOCK_INDENTATION,
        )

    def lines(self) -> Iterable[str]:
        """Yields the code block directives's lines."""
        yield self.directive_line
        if self.option_lines:
            yield from self.options_block_lines()
        yield '\n'
        yield from self.code_block_lines()
        yield '\n'

    def __repr__(self) -> str:
        return ''.join(self.lines())


def _parse_and_format_rst(in_text: str) -> Iterable[str]:
    """Reindents code blocks to 3 spaces and fixes whitespace."""
    current_block: CodeBlock | None = None
    for index, line in enumerate(in_text.splitlines(keepends=True)):
        # If a code block is active, process this line.
        if current_block:
            current_block.append_line(index, line)
            if current_block.finished():
                yield from current_block.lines()
                # This line wasn't part of the code block, process as normal.
                yield _fix_whitespace(line)
                # Erase this code_block variable
                current_block = None
        # Check for new code block start
        elif line.lstrip().startswith(('.. code-block::', '.. code::')):
            current_block = CodeBlock(
                directive_lineno=index,
                # Change `.. code::` to Sphinx's `.. code-block::`.
                directive_line=line.replace('code::', 'code-block::'),
            )
            continue
        else:
            yield _fix_whitespace(line)
    # If the document ends with a code block it may still need to be written.
    if current_block is not None:
        yield from current_block.lines()


def reformat_rst(
    file_name: Path,
    diff: bool = False,
    in_place: bool = False,
    suppress_stdout: bool = False,
) -> list[str]:
    """Reformats an rst file and returns a list of diff lines."""
    in_text = file_name.read_text()
    out_lines = list(_parse_and_format_rst(in_text))

    result_diff = list(
        difflib.unified_diff(
            in_text.splitlines(True),
            out_lines,
            f'{file_name}  (original)',
            f'{file_name}  (reformatted)',
        )
    )
    if diff and result_diff:
        if not suppress_stdout:
            print(''.join(colorize_diff(result_diff)))

    if in_place:
        file_name.write_text(''.join(out_lines))

    return result_diff


def rst_format_main(
    rst_files: list[Path],
    diff: bool = False,
    in_place: bool = False,
) -> None:
    for rst_file in rst_files:
        reformat_rst(rst_file, diff, in_place)


if __name__ == '__main__':
    rst_format_main(**vars(_parse_args()))
