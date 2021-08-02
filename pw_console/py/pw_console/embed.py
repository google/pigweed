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
"""pw_console embed class."""

import asyncio
import logging
from typing import Dict, Iterable, Optional, Union

from pw_console.console_app import ConsoleApp


class PwConsoleEmbed:
    """Embed class for customizing the console before startup."""
    def __init__(
        self,
        global_vars=None,
        local_vars=None,
        loggers: Optional[Union[Dict[str, Iterable[logging.Logger]],
                                Iterable]] = None,
        test_mode=False,
        repl_startup_message: Optional[str] = None,
        help_text: Optional[str] = None,
        app_title: Optional[str] = None,
    ) -> None:
        """Call this to embed pw console at the call point within your program.
        It's similar to `ptpython.embed` and `IPython.embed`. ::

            import logging

            from pw_console import PwConsoleEmbed

            # Create the pw_console embed instance
            console = PwConsoleEmbed(
                global_vars=globals(),
                local_vars=locals(),
                loggers={
                    'Host Logs': [
                        logging.getLogger(__package__),
                        logging.getLogger(__file__)
                    ],
                    'Device Logs': [
                        logging.getLogger('usb_gadget')
                    ],
                },
                app_title='My Awesome Console',
            )
            # Then run the console with:
            console.embed()

        Args:
            global_vars: Dictionary representing the desired global symbol
                table. Similar to what is returned by `globals()`.
            local_vars: Dictionary representing the desired local symbol
                table. Similar to what is returned by `locals()`.
            loggers: Dict with keys of log window titles and values of
                `logging.getLogger()` instances in lists. Each key that should
                be shown in the pw console user interface.
            app_title: Custom title text displayed in the user interface.
            repl_startup_message: Custom text shown by default in the repl
                output pane.
            help_text: Custom text shown at the top of the help window before
                keyboard shortcuts.
        """

        self.global_vars = global_vars
        self.local_vars = local_vars
        self.loggers = loggers
        self.test_mode = test_mode
        self.repl_startup_message = repl_startup_message
        self.help_text = help_text
        self.app_title = app_title

        self.console_app = None

    def _setup_log_panes(self):
        """Add loggers to the console app log pane(s)."""
        if self.loggers:
            if isinstance(self.loggers, list):
                self.console_app.add_log_handler('Logs', self.loggers)
            elif isinstance(self.loggers, dict):
                for window_title, logger_instances in self.loggers.items():
                    self.console_app.add_log_handler(window_title,
                                                     logger_instances)

    def embed(self):
        """Start the console."""

        # Create the ConsoleApp instance.
        self.console_app = ConsoleApp(
            global_vars=self.global_vars,
            local_vars=self.local_vars,
            repl_startup_message=self.repl_startup_message,
            help_text=self.help_text,
            app_title=self.app_title,
        )

        self._setup_log_panes()

        # Start a thread for running user code.
        self.console_app.start_user_code_thread()

        # Start the prompt_toolkit UI app.
        asyncio.run(self.console_app.run(test_mode=self.test_mode),
                    debug=self.test_mode)
