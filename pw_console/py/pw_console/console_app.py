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
"""ConsoleApp control class."""

import builtins
import logging
from typing import Iterable, Optional

_LOG = logging.getLogger(__package__)


class ConsoleApp:
    """The main ConsoleApp class containing the whole console."""
    def __init__(self, global_vars=None, local_vars=None):
        # Create a default global and local symbol table. Values are the same
        # structure as what is returned by globals():
        #   https://docs.python.org/3/library/functions.html#globals
        if global_vars is None:
            global_vars = {
                '__name__': '__main__',
                '__package__': None,
                '__doc__': None,
                '__builtins__': builtins,
            }

        local_vars = local_vars or global_vars

    def add_log_handler(self, logger_instance):
        """Add the Log pane as a handler for this logger instance."""
        # TODO: Add log pane to addHandler call.
        # logger_instance.addHandler(...)


def embed(
    global_vars=None,
    local_vars=None,
    loggers: Optional[Iterable] = None,
) -> None:
    """Call this to embed pw console at the call point within your program.
    It's similar to `ptpython.embed` and `IPython.embed`. ::

        import logging

        from pw_console.console_app import embed

        embed(global_vars=globals(),
              local_vars=locals(),
              loggers=[
                  logging.getLogger(__package__),
                  logging.getLogger('device logs'),
              ],
        )

    :param global_vars: Dictionary representing the desired global symbol
        table. Similar to what is returned by `globals()`.
    :type global_vars: dict, optional
    :param local_vars: Dictionary representing the desired local symbol
        table. Similar to what is returned by `locals()`.
    :type local_vars: dict, optional
    :param loggers: List of `logging.getLogger()` instances that should be shown
        in the pw console log pane user interface.
    :type loggers: list, optional
    """
    console_app = ConsoleApp(
        global_vars=global_vars,
        local_vars=local_vars,
    )

    # Add loggers to the console app log pane.
    if loggers:
        for logger in loggers:
            console_app.add_log_handler(logger)

    # TODO: Start prompt_toolkit app here
    _LOG.debug('Pigweed Console Start')
