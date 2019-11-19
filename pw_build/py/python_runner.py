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
"""Script that preprocesses a Python command then runs it."""

import argparse
import os
import pathlib
import subprocess
import sys


def parse_args() -> argparse.Namespace:
    """Parses arguments for this script, splitting out the command to run."""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gn-root',
                        type=str,
                        required=True,
                        help='Path to the root of the GN tree')
    parser.add_argument('--out-dir',
                        type=str,
                        required=True,
                        help='Path to the GN build output directory')
    parser.add_argument('--touch',
                        type=str,
                        help='File to touch after command is run')
    parser.add_argument('command',
                        nargs=argparse.REMAINDER,
                        help='Python script with arguments to run')
    return parser.parse_args()


def find_binary(target: str) -> str:
    """Tries to find a binary for a gn build target.

    Args:
        target: Relative filesystem path to the target's output directory and
            target name, separated by a colon.

    Returns:
        Full path to the target's binary.

    Raises:
        RuntimeError: No binary found for target.
    """

    target_path, target_name = target.split(':')

    for extension in ['', '.elf', '.exe']:
        potential_filename = f'{target_path}/{target_name}{extension}'
        if os.path.isfile(potential_filename):
            return potential_filename

    raise FileNotFoundError(
        f'Could not find output binary for build target {target}')


def _resolve_path(gn_root: str, out_dir: str, string: str) -> str:
    """Resolves a string to a filesystem path if it is a GN path."""
    if not string.startswith('//'):
        return string

    resolved_path = gn_root + string[2:]

    # GN targets have the format '/path/to/directory:target_name'.
    if string.startswith(out_dir) and ':' in string:
        return find_binary(resolved_path)

    return resolved_path


def resolve_path(gn_root: str, out_dir: str, string: str) -> str:
    """Resolves GN paths to filesystem paths in a semicolon-separated string.

    GN paths are assumed to be absolute, starting with "//". This is replaced
    with the relative filesystem path of the GN root directory.

    If the string is not a GN path, it is returned unmodified.

    If a path refers to the GN output directory and a target name is defined,
    attempts to locate a binary file for the target within the out directory.
    """
    return ';'.join(
        _resolve_path(gn_root, out_dir, path) for path in string.split(';'))


def main() -> int:
    """Script entry point."""

    args = parse_args()
    if not args.command or args.command[0] != '--':
        print(f'{sys.argv[0]} requires a command to run', file=sys.stderr)
        return 1

    try:
        resolved_command = [
            resolve_path(args.gn_root, args.out_dir, arg)
            for arg in args.command[1:]
        ]
    except FileNotFoundError as err:
        print(f'{sys.argv[0]}: {err}', file=sys.stderr)
        return 1

    command = [sys.executable] + resolved_command
    print('RUN', ' '.join(command), flush=True)

    try:
        status = subprocess.call(command)
    except subprocess.CalledProcessError as err:
        print(f'{sys.argv[0]}: {err}', file=sys.stderr)
        return 1

    if status == 0 and args.touch:
        # If a touch file is provided, touch it to indicate a successful run of
        # the command.
        touch_file = resolve_path(args.gn_root, args.out_dir, args.touch)
        print('TOUCH', touch_file)
        pathlib.Path(touch_file).touch()

    return status


if __name__ == '__main__':
    sys.exit(main())
