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
"""Python wrapper that runs a program. For use in GN."""

import argparse
import os
import re
import subprocess
import sys
from typing import Dict, Optional


def argument_parser(
    parser: Optional[argparse.ArgumentParser] = None
) -> argparse.ArgumentParser:
    """Registers the script's arguments on an argument parser."""

    if parser is None:
        parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument(
        '--args-file',
        type=argparse.FileType('r'),
        help='File containing extra positional arguments to the program',
    )
    parser.add_argument(
        '-e',
        '--env',
        action='append',
        default=[],
        help='key=value environment pair for the process',
    )
    parser.add_argument(
        '--env-file',
        type=argparse.FileType('r'),
        help='File defining environment variables for the process',
    )
    parser.add_argument(
        '--skip-empty-args',
        action='store_true',
        help='Don\'t run the program if --args-file is empty',
    )
    parser.add_argument(
        'command',
        nargs=argparse.REMAINDER,
        help='Program to run with arguments',
    )

    return parser


_ENV_REGEX = re.compile(r'(\w+)(\+)?=([\w/.]+)')


def apply_env_var(string: str, env: Dict[str, str]) -> None:
    """Update an environment map with provided a key-value pair.

    Pairs are accepted in two forms:

      KEY=value    sets environment variable "KEY" to "value"
      KEY+=value   appends OS-specific PATH separator and "value" to
                   environment variable "KEY"
    """
    result = _ENV_REGEX.search(string.strip())
    if not result:
        return

    key, append, val = result.groups()
    if append is not None:
        curr = env.get(key)
        val = f'{curr}{os.path.pathsep}{val}' if curr else val

    env[key] = val


def main() -> int:
    args = argument_parser().parse_args()
    if not args.command or args.command[0] != '--':
        return 1

    env = os.environ.copy()

    # Command starts after the "--".
    command = args.command[1:]

    if args.args_file is not None:
        empty = True
        for line in args.args_file:
            empty = False
            command.append(line.strip())

        if args.skip_empty_args and empty:
            return 0

    if args.env_file is not None:
        for line in args.env_file:
            apply_env_var(line, env)

    # Apply command-line overrides at a higher priority than the env file.
    for string in args.env:
        apply_env_var(string, env)

    return subprocess.call(command, env=env)


if __name__ == '__main__':
    sys.exit(main())
