# Copyright 2020 The Pigweed Authors
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
"""Prints an error message and exits unsuccessfully."""

import argparse
import logging
import sys

import pw_cli.log

_LOG = logging.getLogger(__name__)


def _parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--message', help='Error message to print')
    parser.add_argument('--target', help='GN target in which error occurred')
    return parser.parse_args()


def main(message: str, target: str) -> int:
    _LOG.error('')
    _LOG.error('Build error:')
    _LOG.error('')
    for line in message.split('\\n'):
        _LOG.error('  %s', line)
    _LOG.error('')
    _LOG.error('(in %s)', target)
    _LOG.error('')
    return 1


if __name__ == '__main__':
    pw_cli.log.install()
    sys.exit(main(**vars(_parse_args())))
