#!/usr/bin/env python3

# Copyright 2019 The Pigweed Authors
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
"""Checks that C and C++ source files match clang-format's formatting."""

import argparse
import difflib
import os
import subprocess
import sys
from typing import Container, Iterable, List, Optional

SOURCE_EXTENSIONS = frozenset(['.h', '.hh', '.hpp', '.c', '.cc', '.cpp'])
DEFAULT_FORMATTER = 'clang-format'


def _make_color(*codes: int):
    start = ''.join(f'\033[{code}m' for code in codes)
    return f'{start}{{}}\033[0m'.format if os.name == 'posix' else str


color_green = _make_color(32)
color_red = _make_color(31)


def _find_extensions(directory, extensions) -> Iterable[str]:
    for root, _, files in os.walk(directory):
        for file in files:
            if os.path.splitext(file)[1] in extensions:
                yield os.path.join(root, file)


def list_files(paths: Iterable[str], extensions: Container[str]) -> List[str]:
    """Lists files with C or C++ extensions."""
    files = set()

    for path in paths:
        if os.path.isfile(path):
            files.add(path)
        else:
            files.update(_find_extensions(path, extensions))

    return sorted(files)


def clang_format(*args: str, formatter='clang-format') -> bytes:
    """Returns the output of clang-format with the provided arguments."""
    return subprocess.run([formatter, *args],
                          stdout=subprocess.PIPE,
                          check=True).stdout


def _colorize_diff_line(line: str) -> str:
    if line.startswith('-') and not line.startswith('--- '):
        return color_red(line)
    if line.startswith('+') and not line.startswith('+++ '):
        return color_green(line)
    return line


def colorize_diff(lines: Iterable[str]) -> str:
    """Takes a diff str or list of str lines and returns a colorized version."""
    if isinstance(lines, str):
        lines = lines.splitlines(True)

    return ''.join(_colorize_diff_line(line) for line in lines)


def clang_format_diff(path, formatter=DEFAULT_FORMATTER) -> Optional[str]:
    """Returns a diff comparing clang-format's output to the path's contents."""
    with open(path, 'rb') as fd:
        current = fd.read()

    formatted = clang_format(path, formatter=formatter)

    if formatted != current:
        diff = difflib.unified_diff(
            current.decode(errors='replace').splitlines(True),
            formatted.decode(errors='replace').splitlines(True),
            f'{path}  (original)', f'{path}  (reformatted)')

        return colorize_diff(diff)

    return None


def check_format(files, formatter=DEFAULT_FORMATTER) -> List[str]:
    """Diffs files against clang-format; returns paths that did not match."""
    errors = []

    for path in files:
        difference = clang_format_diff(path, formatter)
        if difference:
            errors.append(path)
            print(difference)

    if errors:
        print(f'--> Files with formatting errors: {len(errors)}',
              file=sys.stderr)
        print('   ', '\n    '.join(errors), file=sys.stderr)

    return errors


def _main(paths, fix, formatter) -> int:
    """Checks or fixes formatting."""
    files = list_files(paths, SOURCE_EXTENSIONS)

    if fix:
        for path in files:
            clang_format(path, '-i', formatter=formatter)
        return 0

    errors = check_format(files, formatter)
    return 1 if errors else 0


def _parse_args() -> argparse.Namespace:
    """Parse and return command line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument(
        'paths',
        default=[os.getcwd()],
        nargs='*',
        help=('Files or directories to check. '
              'Within a directory, only C or C++ files are checked.'))
    parser.add_argument('--fix',
                        action='store_true',
                        help='Apply clang-format fixes in place.')
    parser.add_argument('--formatter',
                        default=DEFAULT_FORMATTER,
                        help='The clang-format binary to use.')

    return parser.parse_args()


if __name__ == '__main__':
    sys.exit(_main(**vars(_parse_args())))
