# Copyright 2023 The Pigweed Authors
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
"""Attractive status output to the terminal (and other places if you want)."""

import logging
from typing import Callable, Tuple, Union

from pw_cli.color import colors


def _no_color(msg: str) -> str:
    return msg


def _split_lines(msg: Union[str, list[str]]) -> Tuple[str, list[str]]:
    """Turn a list of strings into a tuple of the first and list of rest."""
    if isinstance(msg, str):
        return (msg, [])

    return (msg[0], msg[1:])


class StatusReporter:
    """Print user-friendly status reports to the terminal for CLI tools.

    You can instead redirect these lines to logs without formatting by
    substituting ``LoggingStatusReporter``. Consumers of this should be
    designed to take any subclass and not make assumptions about where the
    output will go. But the reason you would choose this over plain logging is
    because you want to support pretty-printing to the terminal.

    This is also "themable" in the sense that you can subclass this, override
    the methods with whatever formatting you want, and supply the subclass to
    anything that expects an instance of this.

    Key:

    - info: Plain ol' informational status.
    - ok: Something was checked and it was okay.
    - new: Something needed to be changed/updated and it was successfully.
    - wrn: Warning, non-critical.
    - err: Error, critical.

    This doesn't expose the %-style string formatting that is used in idiomatic
    Python logging, but this shouldn't be used for performance-critical logging
    situations anyway.
    """

    def _report(  # pylint: disable=no-self-use
        self,
        msg: Union[str, list[str]],
        color: Callable[[str], str],
        char: str,
        func: Callable,
        silent: bool,
    ) -> None:
        """Actually print/log/whatever the status lines."""
        first_line, rest_lines = _split_lines(msg)
        first_line = color(f'{char} {first_line}')
        spaces = ' ' * len(char)
        rest_lines = [color(f'{spaces} {line}') for line in rest_lines]

        if not silent:
            for line in [first_line, *rest_lines]:
                func(line)

    def demo(self):
        """Run this to see what your status reporter output looks like."""
        self.info(
            [
                'FYI, here\'s some information:',
                'Lorem ipsum dolor sit amet, consectetur adipiscing elit.',
                'Donec condimentum metus molestie metus maximus ultricies '
                'ac id dolor.',
            ]
        )
        self.ok('This is okay, no changes needed.')
        self.new('We changed some things successfully!')
        self.wrn('Uh oh, you might want to be aware of this.')
        self.err('This is bad! Things might be broken!')

    def info(self, msg: Union[str, list[str]], silent: bool = False) -> None:
        self._report(msg, _no_color, '\u2022', print, silent)

    def ok(self, msg: Union[str, list[str]], silent: bool = False) -> None:
        self._report(msg, colors().blue, '\u2713', print, silent)

    def new(self, msg: Union[str, list[str]], silent: bool = False) -> None:
        self._report(msg, colors().green, '\u2713', print, silent)

    def wrn(self, msg: Union[str, list[str]], silent: bool = False) -> None:
        self._report(msg, colors().yellow, '\u26A0\uFE0F ', print, silent)

    def err(self, msg: Union[str, list[str]], silent: bool = False) -> None:
        self._report(msg, colors().red, '\U0001F525', print, silent)


class LoggingStatusReporter(StatusReporter):
    """Print status lines to logs instead of to the terminal."""

    def __init__(self, logger: logging.Logger) -> None:
        self.logger = logger
        super().__init__()

    def _report(
        self,
        msg: Union[str, list[str]],
        color: Callable[[str], str],
        char: str,
        func: Callable,
        silent: bool,
    ) -> None:
        first_line, rest_lines = _split_lines(msg)

        if not silent:
            for line in [first_line, *rest_lines]:
                func(line)

    def info(self, msg: Union[str, list[str]], silent: bool = False) -> None:
        self._report(msg, _no_color, '', self.logger.info, silent)

    def ok(self, msg: Union[str, list[str]], silent: bool = False) -> None:
        self._report(msg, _no_color, '', self.logger.info, silent)

    def new(self, msg: Union[str, list[str]], silent: bool = False) -> None:
        self._report(msg, _no_color, '', self.logger.info, silent)

    def wrn(self, msg: Union[str, list[str]], silent: bool = False) -> None:
        self._report(msg, _no_color, '', self.logger.warning, silent)

    def err(self, msg: Union[str, list[str]], silent: bool = False) -> None:
        self._report(msg, _no_color, '', self.logger.error, silent)
