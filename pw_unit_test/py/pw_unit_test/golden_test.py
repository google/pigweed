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
"""Runs a binary and compares its output to an expected output file."""

import argparse
import difflib
import os
from pathlib import Path
import shlex
import subprocess
import sys
from typing import Any, Iterable


def _parse_args() -> dict[str, Any]:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--executable', required=True, type=Path, help='The binary to execute'
    )
    parser.add_argument(
        '--expected',
        required=True,
        type=Path,
        help='File with expected output',
    )
    parser.add_argument(
        'executable_args',
        nargs=argparse.REMAINDER,
        help='Arguments after "--" are forwarded to the executable',
    )

    accept = parser.add_mutually_exclusive_group()
    accept.add_argument(
        '--accept',
        action=argparse.BooleanOptionalAction,
        help='Update the golden file to match the output',
    )
    accept.add_argument(
        '--update-command',
        help='Suggest updating the golden file with this command',
    )

    args = parser.parse_args()

    # If there are extra arguments, drop the leading '--'.
    if args.executable_args:
        assert args.executable_args[0] == '--', 'Unexpected first arg!'
        del args.executable_args[0]

    return vars(args)


def run_and_compare(
    executable: Path,
    executable_args: Iterable[str],
    expected: Path,
    *,
    update_command: str = '',
    accept: bool = False,
) -> bool:
    """Runs a binary and compares its output to an expected output file."""

    err = lambda *a, **kw: print(*a, **kw, file=sys.stderr)

    try:
        # Run the command. Combine stderr and stdout so the output matches what
        # the users sees when they run the binary.
        actual_output = subprocess.run(
            [executable, *executable_args],
            stderr=subprocess.STDOUT,
            stdout=subprocess.PIPE,
            check=True,
        ).stdout
    except subprocess.CalledProcessError as e:
        err(f'Error running executable: {e}')
        err(f'Output:\n{e.stdout.decode(errors="replace")}')
        raise

    expected_output = expected.read_bytes()

    if actual_output == expected_output:
        if accept:
            print('✅ Executable output already matches golden file.')

        return True

    err('❌ Golden file test failed!\n')
    err('Executable output does not match golden file.')

    err()
    err('🤖 Command:')
    err(shlex.join([str(executable.resolve()), *executable_args]))
    err(f'⭐ Golden file with expected output:\n{expected.resolve()}')
    err("🔄 Diff:\n")

    diff = difflib.unified_diff(
        expected_output.decode(errors='replace').splitlines(keepends=True),
        actual_output.decode(errors='replace').splitlines(keepends=True),
        fromfile='expected',
        tofile='actual',
    )
    sys.stderr.writelines(diff)
    err()

    if accept:
        expected.resolve().write_bytes(actual_output)
        print(f'✅ Updated golden file {expected.resolve()}.')
        return True

    if update_command:
        err(
            'If the golden file is out of date, run this command to update it '
            "to the latest output:\n"
        )
        err(' ', update_command)
        err()
    return False


if __name__ == '__main__':
    # Change to working directory if running from Bazel.
    if 'BUILD_WORKING_DIRECTORY' in os.environ:
        os.chdir(os.environ['BUILD_WORKING_DIRECTORY'])

    sys.exit(0 if run_and_compare(**_parse_args()) else 1)
