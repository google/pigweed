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
"""Pigweed Project Builder Common argparse."""

import argparse
from pathlib import Path


def add_project_builder_arguments(
    parser: argparse.ArgumentParser,
) -> argparse.ArgumentParser:
    """Add ProjectBuilder.main specific arguments."""
    parser.add_argument(
        'default_build_targets',
        nargs='*',
        metavar='target',
        default=[],
        help=(
            'Automatically locate a build directory and build these '
            'targets. For example, `host docs` searches for a Ninja '
            'build directory at out/ and builds the `host` and `docs` '
            'targets. To specify one or more directories, use the '
            '-C / --build_directory option.'
        ),
    )

    parser.add_argument(
        '-C',
        '--build_directory',
        dest='build_directories',
        nargs='+',
        action='append',
        default=[],
        metavar=('directory', 'target'),
        help=(
            'Specify a build directory and optionally targets to '
            'build. `pw watch -C out tgt` is equivalent to `ninja '
            '-C out tgt`'
        ),
    )

    build_options_group = parser.add_argument_group(
        title='Build system control options'
    )
    build_options_group.add_argument(
        '-j',
        '--jobs',
        type=int,
        help=(
            'Specify the number of cores to use for each build system.'
            'This is passed to ninja, bazel and make as "-j"'
        ),
    )
    build_options_group.add_argument(
        '-k',
        '--keep-going',
        action='store_true',
        help=(
            'Keep building past the first failure. This is equivalent to '
            'running "ninja -k 0" or "bazel build -k".'
        ),
    )

    logfile_group = parser.add_argument_group(title='Log file options')
    logfile_group.add_argument(
        '--logfile',
        type=Path,
        help='Global build output log file.',
    )

    logfile_group.add_argument(
        '--separate-logfiles',
        action='store_true',
        help='Create separate log files per build directory.',
    )

    logfile_group.add_argument(
        '--debug-logging',
        action='store_true',
        help='Enable Python build execution tool debug logging.',
    )

    output_group = parser.add_argument_group(title='Display output options')

    # TODO(b/248257406) Use argparse.BooleanOptionalAction when Python 3.8 is
    # no longer supported.
    output_group.add_argument(
        '--banners',
        action='store_true',
        default=True,
        help='Show pass/fail banners.',
    )
    output_group.add_argument(
        '--no-banners',
        action='store_false',
        dest='banners',
        help='Hide pass/fail banners.',
    )

    # TODO(b/248257406) Use argparse.BooleanOptionalAction when Python 3.8 is
    # no longer supported.
    output_group.add_argument(
        '--colors',
        action='store_true',
        default=True,
        help='Force color output from ninja.',
    )
    output_group.add_argument(
        '--no-colors',
        action='store_false',
        dest='colors',
        help="Don't force ninja to use color output.",
    )

    parser.add_argument(
        '--build-system-command',
        nargs=2,
        action='append',
        default=[],
        dest='build_system_commands',
        metavar=('directory', 'command'),
        help='Build system command for . Default: ninja',
    )

    parser.add_argument(
        '--run-command',
        action='append',
        default=[],
        help=(
            'Additional commands to run. These are run before any -C '
            'arguments and may be repeated. For example: '
            "--run-command 'bazel build //pw_cli/...'"
            "--run-command 'bazel test //pw_cli/...'"
            "-C out python.lint python.test"
        ),
    )

    parser.add_argument(
        '--parallel',
        action='store_true',
        help='Run all builds in parallel.',
    )
    parser.add_argument(
        '--parallel-workers',
        default=0,
        type=int,
        help=(
            'How many builds may run at the same time when --parallel is '
            'enabled. Default: 0 meaning run all in parallel.'
        ),
    )

    return parser
