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
"""Configure the system logger for the default pw command log format."""

import logging
from typing import Optional

import pw_cli.color
import pw_cli.env
import pw_cli.plugins

# Log level used for captured output of a subprocess run through pw.
LOGLEVEL_STDOUT = 21

_LOG = logging.getLogger(__name__)
_STDERR_HANDLER = logging.StreamHandler()


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


def install(level: int = logging.INFO,
            use_color: Optional[bool] = None) -> None:
    """Configure the system logger for the default pw command log format."""

    colors = pw_cli.color.colors(use_color)

    env = pw_cli.env.pigweed_environment()
    if env.PW_SUBPROCESS:
        # If the logger is being run in the context of a pw subprocess, the
        # time and date are omitted (since pw_cli.process will provide them).
        timestamp_fmt = ''
    else:
        # This applies a gray background to the time to make the log lines
        # distinct from other input, in a way that's easier to see than plain
        # colored text.
        timestamp_fmt = colors.black_on_white('%(asctime)s') + ' '

    # Set log level on root logger to debug, otherwise any higher levels
    # elsewhere are ignored.
    root = logging.getLogger()
    root.setLevel(logging.DEBUG)

    _STDERR_HANDLER.setLevel(level)
    _STDERR_HANDLER.setFormatter(
        logging.Formatter(timestamp_fmt + '%(levelname)s %(message)s',
                          '%Y%m%d %H:%M:%S'))
    root.addHandler(_STDERR_HANDLER)

    # Shorten all the log levels to 3 characters for column-aligned logs.
    # Color the logs using ANSI codes.
    # pylint: disable=bad-whitespace
    # yapf: disable
    logging.addLevelName(logging.CRITICAL, colors.bold_red('CRT'))
    logging.addLevelName(logging.ERROR,    colors.red     ('ERR'))
    logging.addLevelName(logging.WARNING,  colors.yellow  ('WRN'))
    logging.addLevelName(logging.INFO,     colors.magenta ('INF'))
    logging.addLevelName(LOGLEVEL_STDOUT,  colors.cyan    ('OUT'))
    logging.addLevelName(logging.DEBUG,    colors.blue    ('DBG'))
    # yapf: enable
    # pylint: enable=bad-whitespace


def set_level(log_level: int):
    """Sets the log level for logs to stderr."""
    _STDERR_HANDLER.setLevel(log_level)


# Note: normally this shouldn't be done at the top level without a try/catch
# around the pw_cli.plugins registry import, since pw_cli might not be
# installed.
pw_cli.plugins.register(
    name='logdemo',
    short_help='Show how how logs look at various levels',
    command_function=main,
)

if __name__ == '__main__':
    install()
    main()
