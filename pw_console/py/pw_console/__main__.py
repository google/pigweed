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
"""Pigweed Console - Warning: This is a work in progress."""

import argparse
import logging
import sys
import tempfile
from datetime import datetime
from typing import List

import pw_cli.log
import pw_cli.argument_types

from pw_console.console_app import embed, FAKE_DEVICE_LOGGER_NAME

_LOG = logging.getLogger(__package__)


def _build_argument_parser() -> argparse.ArgumentParser:
    """Setup argparse."""
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument('-l',
                        '--loglevel',
                        type=pw_cli.argument_types.log_level,
                        default=logging.INFO,
                        help='Set the log level'
                        '(debug, info, warning, error, critical)')

    parser.add_argument('--logfile', help='Pigweed Console debug log file.')

    parser.add_argument('--test-mode',
                        action='store_true',
                        help='Enable fake log messages for testing purposes.')

    return parser


def _create_temp_log_file():
    """Create a unique tempfile for saving logs.

    Example format: /tmp/pw_console_2021-05-04_151807_8hem6iyq
    """

    # Grab the current system timestamp as a string.
    isotime = datetime.now().isoformat(sep='_', timespec='seconds')
    # Timestamp string should not have colons in it.
    isotime = isotime.replace(':', '')

    log_file_name = None
    with tempfile.NamedTemporaryFile(prefix=f'{__package__}_{isotime}_',
                                     delete=False) as log_file:
        log_file_name = log_file.name

    return log_file_name


def main() -> int:
    """Pigweed Console."""

    parser = _build_argument_parser()
    args = parser.parse_args()

    if not args.logfile:
        # Create a temp logfile to prevent logs from appearing over stdout. This
        # would corrupt the prompt toolkit UI.
        args.logfile = _create_temp_log_file()

    pw_cli.log.install(args.loglevel, True, False, args.logfile)

    global_vars = None
    default_loggers: List = []
    if args.test_mode:
        default_loggers = [
            # Don't include pw_console package logs (_LOG) in the log pane UI.
            # Add the fake logger for test_mode.
            logging.getLogger(FAKE_DEVICE_LOGGER_NAME)
        ]
        # Give access to adding log messages from the repl via: `LOG.warning()`
        global_vars = dict(LOG=default_loggers[0])

    embed(global_vars=global_vars,
          loggers=default_loggers,
          test_mode=args.test_mode)

    if args.logfile:
        print(f'Logs saved to: {args.logfile}')

    return 0


if __name__ == '__main__':
    sys.exit(main())
