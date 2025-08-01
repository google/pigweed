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
"""A tool to canonicalize bazel command arguments."""

import argparse
import os
import sys

from pw_ide.compile_commands_generator import parse_bazel_build_command


def main() -> None:
    """Command-line interface for the bazel command util."""
    parser = argparse.ArgumentParser(
        description='Canonicalize bazel command arguments.'
    )
    parser.add_argument(
        'cmd',
        help='The bazel command to parse, e.g. "build --config=foo //bar:baz"',
    )
    parser.add_argument(
        '--bazelCmd',
        default='bazelisk',
        help='Path to the bazel executable.',
    )
    parser.add_argument(
        '--cwd',
        default=os.getcwd(),
        help='Working directory for bazel.',
    )
    args = parser.parse_args()

    try:
        parsed_command = parse_bazel_build_command(
            args.cmd, args.bazelCmd, args.cwd
        )
        print(' '.join(parsed_command.args))
    except (ValueError, RuntimeError) as e:
        print(f'Error: {e}', file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
