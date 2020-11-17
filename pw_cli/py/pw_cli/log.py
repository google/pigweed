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
from pathlib import Path
from typing import NamedTuple, Union

import pw_cli.color
import pw_cli.env
import pw_cli.plugins

# Log level used for captured output of a subprocess run through pw.
LOGLEVEL_STDOUT = 21


class LogLevel(NamedTuple):
    level: int
    color: str
    ascii: str
    emoji: str


# Shorten all the log levels to 3 characters for column-aligned logs.
# Color the logs using ANSI codes.
_LOG_LEVELS = (
    LogLevel(logging.CRITICAL, 'bold_red', 'CRT', '☠️ '),
    LogLevel(logging.ERROR,    'red',      'ERR', '❌'),
    LogLevel(logging.WARNING,  'yellow',   'WRN', '⚠️ '),
    LogLevel(logging.INFO,     'magenta',  'INF', 'ℹ️ '),
    LogLevel(LOGLEVEL_STDOUT,  'cyan',     'OUT', '💬'),
    LogLevel(logging.DEBUG,    'blue',     'DBG', '👾'),
)  # yapf: disable

_LOG = logging.getLogger(__name__)
_STDERR_HANDLER = logging.StreamHandler()


def main() -> None:
    """Show how logs look at various levels."""

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
            use_color: bool = None,
            hide_timestamp: bool = False,
            log_file: Union[str, Path] = None) -> None:
    """Configure the system logger for the default pw command log format."""

    colors = pw_cli.color.colors(use_color)

    env = pw_cli.env.pigweed_environment()
    if env.PW_SUBPROCESS or hide_timestamp:
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

    handler = logging.FileHandler(log_file) if log_file else _STDERR_HANDLER
    handler.setLevel(level)
    handler.setFormatter(
        logging.Formatter(timestamp_fmt + '%(levelname)s %(message)s',
                          '%Y%m%d %H:%M:%S'))
    root.addHandler(handler)

    if env.PW_EMOJI:
        name_attr = 'emoji'
        colorize = lambda ll: str
    else:
        name_attr = 'ascii'
        colorize = lambda ll: getattr(colors, ll.color)

    for log_level in _LOG_LEVELS:
        name = getattr(log_level, name_attr)
        logging.addLevelName(log_level.level, colorize(log_level)(name))


def set_level(log_level: int):
    """Sets the log level for logs to stderr."""
    _STDERR_HANDLER.setLevel(log_level)


if __name__ == '__main__':
    install()
    main()
