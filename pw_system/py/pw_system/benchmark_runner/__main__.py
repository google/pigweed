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
"""Pigweed System RPC Benchmark runner entry point."""

import argparse
import logging
from pathlib import Path
import sys

from pw_cli import log as pw_benchmark_cli_log
from pw_cli import argument_types
from pw_console.python_logging import create_temp_log_file
from pw_system.benchmark_runner import benchmark_runner
from pw_system.device_connection import (
    add_device_args,
)

_BENCHMARK_ROOT_LOG = logging.getLogger('pw_system.benchmark_runner')


def _build_argument_parser() -> argparse.ArgumentParser:
    """Setup argparse."""
    parser = argparse.ArgumentParser(
        prog="python -m pw_system_benchmark_runner", description=__doc__
    )
    parser = add_device_args(parser)

    parser.add_argument(
        '-l',
        '--loglevel',
        type=argument_types.log_level,
        default=logging.DEBUG,
        help='Set the log level' '(debug, info, warning, error, critical)',
    )

    parser.add_argument(
        '--logfile', help='Pigweed System Benchmark Runner log file.'
    )

    parser.add_argument(
        '--config-file',
        type=Path,
        help='Path to a pw_rpc benchmark yaml config file.',
    )

    return parser


def main() -> int:
    """Pigweed Benchmark CLI."""
    parser = _build_argument_parser()
    result = 0

    # TODO: https://pwbug.dev/380584579 - update with a subparser or different
    # arg handling to enable device-specific args.
    args = parser.parse_args()

    if not args.logfile:
        # Create a temp logfile to prevent logs from appearing over stdout.
        args.logfile = create_temp_log_file(prefix="pw_system_benchmark_runner")

    pw_benchmark_cli_log.install(
        level=args.loglevel,
        use_color=True,
        hide_timestamp=False,
        log_file=args.logfile,
    )

    # TODO: https://pwbug.dev/380584579 - allow users to provide a device
    # directly (instead of just using the sim)
    with benchmark_runner.Runner(
        args=args, logger=_BENCHMARK_ROOT_LOG, device=None
    ) as uut:
        try:
            result = uut.execute_all()
        except Exception as e:  # pylint: disable=broad-exception-caught
            result = 255
            _BENCHMARK_ROOT_LOG.exception(e)
            raise
    return result


if __name__ == '__main__':
    sys.exit(main())
