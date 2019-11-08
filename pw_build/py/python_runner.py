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
import subprocess
import sys

from typing import List


def parse_args() -> argparse.Namespace:
    """Parses arguments for this script, splitting out the command to run."""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gn-root', type=str, required=True,
                        help='Path to the root of the GN tree')
    parser.add_argument('--out-dir', type=str, required=True,
                        help='Path to the GN build output directory')
    parser.add_argument('command', nargs=argparse.REMAINDER,
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

    extensions = ['', '.elf']
    for extension in extensions:
        potential_filename = f'{target_path}/{target_name}{extension}'
        if os.path.isfile(potential_filename):
            return potential_filename

    raise FileNotFoundError(
        f'Could not find output binary for build target {target}')


def resolve_paths(gn_root: str, out_dir: str, command: List[str]) -> None:
    """Runs through an argument list, replacing GN paths with filesystem paths.

    GN paths are assumed to be absolute, starting with "//". This is replaced
    with the relative filesystem path of the GN root directory.

    If a path refers to the GN output directory and a target name is defined,
    attempts to locate a binary file for the target within the out directory.
    """

    for i, arg in enumerate(command):
        if not arg.startswith('//'):
            continue

        resolved_path = gn_root + arg[2:]

        # GN targets have the format '/path/to/directory:target_name'.
        if arg.startswith(out_dir) and ':' in arg:
            command[i] = find_binary(resolved_path)
        else:
            command[i] = resolved_path


def main() -> int:
    """Script entry point."""

    args = parse_args()
    if not args.command or args.command[0] != '--':
        print(f'{sys.argv[0]} requires a command to run', file=sys.stderr)
        return 1

    command = [sys.executable] + args.command[1:]

    try:
        resolve_paths(args.gn_root, args.out_dir, command)
    except FileNotFoundError as err:
        print(f'{sys.argv[0]} {err}', file=sys.stderr)
        return 1

    return subprocess.call(command)


if __name__ == '__main__':
    sys.exit(main())
