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
    parser.add_argument('test', type=str, help='Path to unit test binary')
    return parser.parse_args()


def main() -> int:
    """Runs some unit tests."""

    args = parse_args()

    try:
        exit_status = subprocess.call([args.test])
    except subprocess.CalledProcessError as err:
        print(f'{sys.argv[0]}: {err}', file=sys.stderr)
        return 1

    return exit_status


if __name__ == '__main__':
    sys.exit(main())
