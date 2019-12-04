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

import logging
import os

import pw_cli.plugins
from pw_cli.color import Color as _Color

# Log level used for captured output of a subprocess run through pw.
LOGLEVEL_STDOUT = 21

_LOG = logging.getLogger(__name__)


def main():
    # Force the log level to make sure all logs are shown.
    _LOG.setLevel(logging.DEBUG)

    # Log one message for every log level.
    _LOG.critical('Something terrible has happened!')
    _LOG.error('There was an error on our last operation')
    _LOG.warning('Looks like something is amiss; consider investigating')
    _LOG.info('The operation went as expected')
    _LOG.log(LOGLEVEL_STDOUT, 'Standard output of subprocess')
    _LOG.debug('Adding 1 to i')


def install():
    """Configure the system logger for the default pw command log format."""

    PW_SUBPROCESS = os.getenv('PW_SUBPROCESS')
    if PW_SUBPROCESS is None:
        # This applies a gray background to the time to make the log lines
        # distinct from other input, in a way that's easier to see than plain
        # colored text.
        timestamp_fmt = _Color.black_on_white('%(asctime)s') + ' '
    elif PW_SUBPROCESS == '1':
        # If the logger is being run in the context of a pw subprocess, the
        # time and date are omitted (since pw_cli.process will provide them).
        timestamp_fmt = ''
    else:
        raise ValueError(
            f'Invalid environment variable PW_SUBPROCESS={PW_SUBPROCESS}')

    logging.basicConfig(format=timestamp_fmt + '%(levelname)s %(message)s',
                        datefmt='%Y%m%d %H:%M:%S',
                        level=logging.INFO)

    # Shorten all the log levels to 3 characters for column-aligned logs.
    # Color the logs using ANSI codes.
    # yapf: disable
    logging.addLevelName(logging.CRITICAL, _Color.bold_red('CRT'))
    logging.addLevelName(logging.ERROR,    _Color.red     ('ERR'))
    logging.addLevelName(logging.WARNING,  _Color.yellow  ('WRN'))
    logging.addLevelName(logging.INFO,     _Color.magenta ('INF'))
    logging.addLevelName(LOGLEVEL_STDOUT,  _Color.cyan    ('OUT'))
    logging.addLevelName(logging.DEBUG,    _Color.blue    ('DBG'))
    # yapf: enable


# Note: normally this shouldn't be done at the top level without a try/catch
# around the pw_cli.plugins registry import, since pw_cli might not be
# installed.
pw_cli.plugins.register(
    name='logdemo',
    help='Show how how logs look at various levels',
    command_function=main,
)

if __name__ == '__main__':
    install()
    main()
