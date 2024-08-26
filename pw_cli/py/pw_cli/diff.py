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
"""Print git-style diffs."""


import difflib
import sys
from typing import Iterable, Sequence, Union

from pw_cli.color import colors

_COLOR = colors()


def colorize_diff_line(line: str) -> str:
    if line.startswith('--- ') or line.startswith('+++ '):
        return _COLOR.bold_white(line)
    if line.startswith('-'):
        return _COLOR.red(line)
    if line.startswith('+'):
        return _COLOR.green(line)
    if line.startswith('@@ '):
        return _COLOR.cyan(line)
    return line


def colorize_diff(lines: Union[str, Iterable[str]]) -> str:
    """Takes a diff str or list of str lines and returns a colorized version."""
    if isinstance(lines, str):
        lines = lines.splitlines(True)

    return ''.join(colorize_diff_line(line) for line in lines)


def print_diff(
    a: Sequence[str], b: Sequence[str], fromfile="", tofile="", indent=0
) -> None:
    """Print a git-style diff of two string sequences."""
    indent_str = ' ' * indent
    diff = colorize_diff(difflib.unified_diff(a, b, fromfile, tofile))

    for line in diff.splitlines(True):
        sys.stdout.write(indent_str + line)

    sys.stdout.write("\n")
