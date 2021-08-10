#!/usr/bin/env python3
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
"""Invoke clang-tidy.

Implements additional features compared to directly calling
clang-tidy:
  - add option `--source-filter` to select which sources to run the
    clang-tidy on. Similar to `--header-filter` implemented by
    clang-tidy.
  - inputs the full compile command, with the cc binary name
  - TODO: infer platform options from the full compile command
"""

import argparse
import logging
import pathlib
import re
import subprocess
import sys

from pathlib import Path
from typing import List, Optional

_LOG = logging.getLogger(__name__)


def _parse_args() -> argparse.Namespace:
    """Parses arguments for this script, splitting out the command to run."""

    parser = argparse.ArgumentParser()
    parser.add_argument('-v',
                        '--verbose',
                        action='store_true',
                        help='Run clang_tidy with extra debug output.')

    parser.add_argument('--clang-tidy',
                        default='clang-tidy',
                        help='Path to clang-tidy executable.')

    parser.add_argument('--source-file',
                        required=True,
                        help='Source file to analyze with clang-tidy.')

    parser.add_argument('--export-fixes',
                        required=False,
                        help='YAML file to store suggested fixes in. The ' +
                        'stored fixes can be applied to the input source ' +
                        'code with clang-apply-replacements.')

    parser.add_argument('--source-filter',
                        default='.*',
                        help='Regular expression matching the names of' +
                        ' the source files to output diagnostics from')

    # Add a silent placeholder arg for everything that was left over.
    parser.add_argument('extra_args',
                        nargs=argparse.REMAINDER,
                        help=argparse.SUPPRESS)

    parsed_args = parser.parse_args()

    assert parsed_args.extra_args[0] == '--', 'arguments not correctly split'
    parsed_args.extra_args = parsed_args.extra_args[1:]
    return parsed_args


def run_clang_tidy(clang_tidy: str, verbose: bool, source_file: Path,
                   export_fixes: Optional[Path], extra_args: List[str]) -> int:
    """Executes clang_tidy via subprocess. Returns true if no failures."""
    command = [clang_tidy, str(source_file)]

    if not verbose:
        command.append('--quiet')

    if export_fixes is not None:
        command.extend(['--export-fixes', str(export_fixes)])

    # Append extra compilation flags. extra_args[0] is skipped as it contains
    # the compiler binary name.
    command.append('--')
    command.extend(extra_args[1:])

    process = subprocess.run(command,
                             stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE)

    if process.stdout:
        _LOG.warning(process.stdout.decode('utf-8').strip())

    if process.stderr and process.returncode != 0:
        _LOG.error(process.stderr.decode('utf-8').strip())

    return process.returncode


def main(
    verbose: bool,
    clang_tidy: str,
    source_file: str,
    export_fixes: Optional[str],
    source_filter: str,
    extra_args: List[str],
) -> int:

    if not re.search(source_filter, source_file):
        return 0

    source_file_path = pathlib.Path(source_file).resolve()
    export_fixes_path = pathlib.Path(
        export_fixes).resolve() if export_fixes is not None else None
    return run_clang_tidy(clang_tidy, verbose, source_file_path,
                          export_fixes_path, extra_args)


if __name__ == '__main__':
    sys.exit(main(**vars(_parse_args())))
