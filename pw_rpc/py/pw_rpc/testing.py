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
"""Utilities for testing pw_rpc."""

import argparse
import subprocess
import sys
import time
from typing import Sequence


def parse_test_server_args(
        parser: argparse.ArgumentParser = None) -> argparse.Namespace:
    """Parses arguments for running a Python-based integration test."""
    if parser is None:
        parser = argparse.ArgumentParser(
            description=sys.modules['__main__'].__doc__)

    parser.add_argument('--test-server-command',
                        nargs='+',
                        required=True,
                        help='Command that starts the test server.')
    parser.add_argument(
        '--port',
        type=int,
        required=True,
        help=('The port to use to connect to the test server. This value is '
              'passed to the test server as the last argument.'))
    parser.add_argument('unittest_args',
                        nargs=argparse.REMAINDER,
                        help='Arguments after "--" are passed to unittest.')

    args = parser.parse_args()

    # Append the port number to the test server command.
    args.test_server_command.append(str(args.port))

    # Make the script name argv[0] and drop the "--".
    args.unittest_args = sys.argv[:1] + args.unittest_args[1:]

    return args


def _parse_subprocess_integration_test_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description='Executes a test between two subprocesses')
    parser.add_argument('server_command',
                        nargs='+',
                        help='Command that starts the server')
    parser.add_argument('separator', nargs=1, metavar='--')
    parser.add_argument(
        'client_command',
        nargs='+',
        help='Command that starts the client and runs the test')

    if '-h' in sys.argv or '--help' in sys.argv:
        parser.print_help()
        sys.exit(0)

    try:
        index = sys.argv.index('--')
    except ValueError:
        parser.error('The server and client commands must be separated by --.')

    args = argparse.Namespace()
    args.server_command = sys.argv[1:index]
    args.client_command = sys.argv[index + 1:]
    return args


def execute_integration_test(server_command: Sequence[str],
                             client_command: Sequence[str],
                             setup_time_s: float = 0.1) -> int:
    server = subprocess.Popen(server_command)
    # TODO(pwbug/508): Replace this delay with some sort of IPC.
    time.sleep(setup_time_s)

    result = subprocess.run(client_command).returncode

    server.terminate()
    server.communicate()

    return result


if __name__ == '__main__':
    sys.exit(
        execute_integration_test(
            **vars(_parse_subprocess_integration_test_args())))
