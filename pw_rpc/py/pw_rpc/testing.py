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
import sys


def parse_test_server_args(
        parser: argparse.ArgumentParser = None) -> argparse.Namespace:
    """Parses arguments for the test server."""
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
