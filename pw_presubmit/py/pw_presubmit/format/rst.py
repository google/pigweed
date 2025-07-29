# Copyright 2025 The Pigweed Authors
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
"""Formatter for reStructuredText files."""

from dataclasses import dataclass, field
from functools import cached_property
from pathlib import Path
import textwrap
from typing import Iterable

from pw_cli.file_filter import FileFilter
from pw_presubmit.format.core import (
    FileFormatter,
    FormattedFileContents,
    FormatFixStatus,
)

DEFAULT_RST_FILE_PATTERNS = FileFilter(endswith=['.rst'])
TAB_WIDTH = 8  # Number of spaces to use for \t replacement
CODE_BLOCK_INDENTATION = 3


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
class _CodeBlock:
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


def _parse_and_format_rst(in_text: str) -> str:
    """Reindents code blocks to 3 spaces and fixes whitespace."""
    output_lines: list[str] = []
    current_block: _CodeBlock | None = None
    for index, line in enumerate(in_text.splitlines(keepends=True)):
        # If a code block is active, process this line.
        if current_block:
            current_block.append_line(index, line)
            if current_block.finished():
                output_lines.extend(current_block.lines())
                # This line wasn't part of the code block, process as normal.
                output_lines.append(_fix_whitespace(line))
                # Erase this code_block variable
                current_block = None
        # Check for new code block start
        elif line.lstrip().startswith(('.. code-block::', '.. code::')):
            current_block = _CodeBlock(
                directive_lineno=index,
                # Change `.. code::` to Sphinx's `.. code-block::`.
                directive_line=line.replace('code::', 'code-block::'),
            )
            continue
        else:
            output_lines.append(_fix_whitespace(line))
    # If the document ends with a code block it may still need to be written.
    if current_block is not None:
        output_lines.extend(current_block.lines())

    # Remove blank lines from the end of the output, if any.
    while output_lines and not output_lines[-1].strip():
        output_lines.pop()

    # Add a trailing \n if needed.
    if output_lines and not output_lines[-1].endswith('\n'):
        output_lines[-1] += '\n'

    return ''.join(output_lines)


class RstFormatter(FileFormatter):
    """A custom reStructuredText formatter."""

    def __init__(self, **kwargs):
        kwargs.setdefault('mnemonic', 'reStructuredText')
        kwargs.setdefault('file_patterns', DEFAULT_RST_FILE_PATTERNS)
        super().__init__(**kwargs)

    def format_file_in_memory(
        self, file_path: Path, file_contents: bytes
    ) -> FormattedFileContents:
        """Formats the provided file contents in-memory."""
        formatted_text = _parse_and_format_rst(file_contents.decode())
        return FormattedFileContents(
            ok=True,
            formatted_file_contents=formatted_text.encode(),
            error_message=None,
        )

    def format_file(self, file_path: Path) -> FormatFixStatus:
        """Formats the provided file in-place."""
        original_contents = file_path.read_bytes()
        result = self.format_file_in_memory(file_path, original_contents)
        if result.formatted_file_contents != original_contents:
            file_path.write_bytes(result.formatted_file_contents)
        return FormatFixStatus(ok=True, error_message=None)
