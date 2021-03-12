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
"""Tools for configuring Python logging."""

import logging
from pathlib import Path
from typing import NamedTuple, Union, Iterator

import pw_cli.color
import pw_cli.env
import pw_cli.plugins

# Log level used for captured output of a subprocess run through pw.
LOGLEVEL_STDOUT = 21


class _LogLevel(NamedTuple):
    level: int
    color: str
    ascii: str
    emoji: str


# Shorten all the log levels to 3 characters for column-aligned logs.
# Color the logs using ANSI codes.
_LOG_LEVELS = (
    _LogLevel(logging.CRITICAL, 'bold_red', 'CRT', '☠️ '),
    _LogLevel(logging.ERROR,    'red',      'ERR', '❌'),
    _LogLevel(logging.WARNING,  'yellow',   'WRN', '⚠️ '),
    _LogLevel(logging.INFO,     'magenta',  'INF', 'ℹ️ '),
    _LogLevel(LOGLEVEL_STDOUT,  'cyan',     'OUT', '💬'),
    _LogLevel(logging.DEBUG,    'blue',     'DBG', '👾'),
)  # yapf: disable

_LOG = logging.getLogger(__name__)
_STDERR_HANDLER = logging.StreamHandler()


def main() -> None:
    """Shows how logs look at various levels."""

    # Force the log level to make sure all logs are shown.
    _LOG.setLevel(logging.DEBUG)

    # Log one message for every log level.
    _LOG.critical('Something terrible has happened!')
    _LOG.error('There was an error on our last operation')
    _LOG.warning('Looks like something is amiss; consider investigating')
    _LOG.info('The operation went as expected')
    _LOG.log(LOGLEVEL_STDOUT, 'Standard output of subprocess')
    _LOG.debug('Adding 1 to i')


def _setup_handler(handler: logging.Handler, formatter: logging.Formatter,
                   level: int) -> None:
    handler.setLevel(level)
    handler.setFormatter(formatter)
    logging.getLogger().addHandler(handler)


def install(level: int = logging.INFO,
            use_color: bool = None,
            hide_timestamp: bool = False,
            log_file: Union[str, Path] = None) -> None:
    """Configures the system logger for the default pw command log format."""

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

    formatter = logging.Formatter(timestamp_fmt + '%(levelname)s %(message)s',
                                  '%Y%m%d %H:%M:%S')

    # Set the log level on the root logger to 1, so logs that all logs
    # propagated from child loggers are handled.
    logging.getLogger().setLevel(1)

    # Always set up the stderr handler, even if it isn't used.
    _setup_handler(_STDERR_HANDLER, formatter, level)

    if log_file:
        _setup_handler(logging.FileHandler(log_file), formatter, level)
        # Since we're using a file, filter logs out of the stderr handler.
        _STDERR_HANDLER.setLevel(logging.CRITICAL + 1)

    if env.PW_EMOJI:
        name_attr = 'emoji'
        colorize = lambda ll: str
    else:
        name_attr = 'ascii'
        colorize = lambda ll: getattr(colors, ll.color)

    for log_level in _LOG_LEVELS:
        name = getattr(log_level, name_attr)
        logging.addLevelName(log_level.level, colorize(log_level)(name))


def all_loggers() -> Iterator[logging.Logger]:
    """Iterates over all loggers known to Python logging."""
    manager = logging.getLogger().manager  # type: ignore[attr-defined]

    for logger_name in manager.loggerDict:  # pylint: disable=no-member
        yield logging.getLogger(logger_name)


def set_all_loggers_minimum_level(level: int) -> None:
    """Increases the log level to the specified value for all known loggers."""
    for logger in all_loggers():
        if logger.isEnabledFor(level - 1):
            logger.setLevel(level)


if __name__ == '__main__':
    install()
    main()
