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

"""Script which runs Pigweed unit tests built using GN.

Currently, only a single test can be run at a time. The build path and GN target
name of the test are given to the script.
"""

import argparse
import pathlib
import os
import subprocess
import sys


def parse_args() -> argparse.Namespace:
    """Parses command-line arguments."""

    parser = argparse.ArgumentParser('Run Pigweed unit tests')
    parser.add_argument('--touch', type=str,
                        help='File to touch after test run')
    parser.add_argument('test', type=str, help='Path to unit test binary')
    return parser.parse_args()


# TODO(frolv): This should be extracted into a script-running script which
# performs path resolution before calling another script.
def find_binary(target: str) -> str:
    """Tries to find a binary for a gn build target.

    Args:
        target: Relative path to the target's output directory and target name,
            separated by a colon.

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
        f'could not find output binary for build target {target}')


def main() -> int:
    """Runs some unit tests."""

    args = parse_args()

    try:
        test_binary = find_binary(args.test)
    except FileNotFoundError as err:
        print(f'{sys.argv[0]}: {err}', file=sys.stderr)
        return 1

    exit_status = subprocess.call([test_binary])

    # GN expects "action" targets to output a file, and uses that to determine
    # whether the target should be run again. Touching an empty file allows GN
    # to only run unit tests which have been affected by code changes since the
    # previous run, taking advantage of its dependency resolution.
    if args.touch is not None:
        pathlib.Path(args.touch).touch()

    return exit_status


if __name__ == '__main__':
    sys.exit(main())
