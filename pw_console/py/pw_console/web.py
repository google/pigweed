# Copyright 2024 The Pigweed Authors
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
"""pw_console web browser repl and log viewer startup code."""

import importlib.resources
import logging
from pathlib import Path
from typing import Iterable

import asyncio

from pw_console.console_app import PW_CONSOLE_MODULE
from pw_console.test_mode import start_fake_logger
from pw_console.web_server import pw_console_http_server


class PwConsoleWeb:
    """Class for starting console in the browser."""

    def __init__(
        self,
        global_vars=None,
        local_vars=None,
        test_mode=False,
        loggers: dict[str, Iterable[logging.Logger]] | Iterable | None = None,
        sentence_completions: dict[str, str] | None = None,
    ) -> None:
        self.test_mode = test_mode
        self.test_mode_log_loop = asyncio.new_event_loop()
        html_package_path = f'{PW_CONSOLE_MODULE}.html'
        self.html_files = {
            '/{}'.format(resource.name): resource.read_text()
            for resource in importlib.resources.files(
                html_package_path
            ).iterdir()
            if resource.is_file()
            and Path(resource.name).suffix in ['.css', '.html', '.js', '.json']
        }
        self.global_vars = global_vars
        self.loggers = loggers
        self.local_vars = local_vars
        self.sentence_completions = sentence_completions

    def _test_mode_log_thread_entry(self) -> None:
        """Entry point for the user code thread."""
        asyncio.set_event_loop(self.test_mode_log_loop)
        self.test_mode_log_loop.run_forever()

    def start(self) -> None:
        if self.test_mode:
            rstdoc_text = (
                importlib.resources.files(f'{PW_CONSOLE_MODULE}.docs')
                .joinpath('user_guide.rst')
                .read_text()
            )

            start_fake_logger(
                lines=rstdoc_text.splitlines(),
                log_thread_entry=self._test_mode_log_thread_entry,
                log_thread_loop=self.test_mode_log_loop,
            )
        pw_console_http_server(
            3000,
            self.html_files,
            kernel_params={
                'global_vars': self.global_vars,
                'local_vars': self.local_vars,
                'loggers': self.loggers,
                'sentence_completions': self.sentence_completions,
            },
        )
