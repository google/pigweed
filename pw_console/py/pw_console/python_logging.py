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
"""Python logging helper fuctions."""

import copy
import logging
import tempfile
from datetime import datetime
from typing import Iterable, Iterator, Optional


def all_loggers() -> Iterator[logging.Logger]:
    """Iterates over all loggers known to Python logging."""
    manager = logging.getLogger().manager  # type: ignore[attr-defined]

    for logger_name in manager.loggerDict:  # pylint: disable=no-member
        yield logging.getLogger(logger_name)


def create_temp_log_file(prefix: Optional[str] = None,
                         add_time: bool = True) -> str:
    """Create a unique tempfile for saving logs.

    Example format: /tmp/pw_console_2021-05-04_151807_8hem6iyq
    """
    if not prefix:
        prefix = str(__package__)

    # Grab the current system timestamp as a string.
    isotime = datetime.now().isoformat(sep='_', timespec='seconds')
    # Timestamp string should not have colons in it.
    isotime = isotime.replace(':', '')

    if add_time:
        prefix += f'_{isotime}'

    log_file_name = None
    with tempfile.NamedTemporaryFile(prefix=f'{prefix}_',
                                     delete=False) as log_file:
        log_file_name = log_file.name

    return log_file_name


def set_logging_last_resort_file_handler(
        file_name: Optional[str] = None) -> None:
    log_file = file_name if file_name else create_temp_log_file()
    logging.lastResort = logging.FileHandler(log_file)


def disable_stdout_handlers(logger: logging.Logger) -> None:
    """Remove all stdout and stdout & stderr logger handlers."""
    for handler in copy.copy(logger.handlers):
        # Must use type() check here since this returns True:
        #   isinstance(logging.FileHandler, logging.StreamHandler)
        if type(handler) == logging.StreamHandler:  # pylint: disable=unidiomatic-typecheck
            logger.removeHandler(handler)


def setup_python_logging(
    last_resort_filename: Optional[str] = None,
    loggers_with_no_propagation: Optional[Iterable[logging.Logger]] = None
) -> None:
    """Disable log handlers for full screen prompt_toolkit applications."""
    if not loggers_with_no_propagation:
        loggers_with_no_propagation = []
    disable_stdout_handlers(logging.getLogger())

    if logging.lastResort is not None:
        set_logging_last_resort_file_handler(last_resort_filename)

    for logger in list(all_loggers()):
        # Prevent stdout handlers from corrupting the prompt_toolkit UI.
        disable_stdout_handlers(logger)
        if logger in loggers_with_no_propagation:
            continue
        # Make sure all known loggers propagate to the root logger.
        logger.propagate = True

    # Prevent these loggers from propagating to the root logger.
    hidden_host_loggers = [
        'pw_console',
        'pw_console.plugins',

        # prompt_toolkit triggered debug log messages
        'prompt_toolkit',
        'prompt_toolkit.buffer',
        'parso.python.diff',
        'parso.cache',
        'pw_console.serial_debug_logger',
    ]
    for logger_name in hidden_host_loggers:
        logging.getLogger(logger_name).propagate = False

    # Set asyncio log level to WARNING
    logging.getLogger('asyncio').setLevel(logging.WARNING)

    # Always set DEBUG level for serial debug.
    logging.getLogger('pw_console.serial_debug_logger').setLevel(logging.DEBUG)
