# Copyright 2022 The Pigweed Authors
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

# Inspired by
# https://fuchsia.googlesource.com/infra/recipes/+/336933647862a1a9718b4ca18f0a67e89c2419f8/recipe_modules/ninja/resources/ninja_wrapper.py
"""Extracts a concise error from a ninja log."""

import argparse
import logging
from pathlib import Path
import re
from typing import List, IO
import sys

_LOG: logging.Logger = logging.getLogger(__name__)

# Assume any of these lines could be prefixed with ANSI color codes.
_COLOR_CODES_PREFIX = r'^(?:\x1b)?(?:\[\d+m\s*)?'


def _parse_ninja(ins: IO) -> str:
    failure_lines: List[str] = []
    last_line: str = ''

    for line in ins:
        _LOG.debug('processing %r', line)
        # Trailing whitespace isn't significant, as it doesn't affect the
        # way the line shows up in the logs. However, leading whitespace may
        # be significant, especially for compiler error messages.
        line = line.rstrip()

        if failure_lines:
            _LOG.debug('inside failure block')

            if re.match(_COLOR_CODES_PREFIX + r'\[\d+/\d+\] (\S+)', line):
                _LOG.debug('next rule started, ending failure block')
                break

            if re.match(_COLOR_CODES_PREFIX + r'ninja: build stopped:.*', line):
                _LOG.debug('ninja build stopped, ending failure block')
                break
            failure_lines.append(line)

        else:
            if re.match(_COLOR_CODES_PREFIX + r'FAILED: (.*)$', line):
                _LOG.debug('starting failure block')
                failure_lines.extend([last_line, line])
        last_line = line

    # Remove "Requirement already satisfied:" lines since many of those might
    # be printed during Python installation, and they usually have no relevance
    # to the actual error.
    failure_lines = [
        x
        for x in failure_lines
        if not x.lstrip().startswith('Requirement already satisfied:')
    ]

    result: str = '\n'.join(failure_lines)
    return re.sub(r'\n+', '\n', result)


def parse_ninja_stdout(ninja_stdout: Path) -> str:
    """Extract an error summary from ninja output."""
    with ninja_stdout.open() as ins:
        return _parse_ninja(ins)


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument('input', type=argparse.FileType('r'))
    parser.add_argument('output', type=argparse.FileType('w'))
    args = parser.parse_args(argv)

    for line in _parse_ninja(args.input):
        args.output.write(line)


if __name__ == '__main__':
    sys.exit(main())
