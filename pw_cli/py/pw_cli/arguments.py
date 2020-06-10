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
"""Defines arguments for the pw command."""

import argparse
import logging
from pathlib import Path
import sys
from typing import NoReturn

from pw_cli import plugins
from pw_cli.branding import banner

_HELP_HEADER = '''The Pigweed command line interface (CLI).

Example uses:
    pw logdemo
    pw --loglevel debug watch out/clang
'''


def parse_args() -> argparse.Namespace:
    return _parser().parse_args()


def print_banner() -> None:
    """Prints the PIGWEED (or project specific) banner to stderr."""
    print(banner(), file=sys.stderr)


def format_help() -> str:
    """Returns the pw help information as a string."""
    return f'{_parser().format_help()}\n{plugins.command_help()}'


class _ArgumentParserWithBanner(argparse.ArgumentParser):
    """Parser that the Pigweed banner when there are parsing errors."""
    def error(self, message: str) -> NoReturn:
        print_banner()
        self.print_usage(sys.stderr)
        self.exit(2, f'{self.prog}: error: {message}\n')


def _parser() -> argparse.ArgumentParser:
    """Creates an argument parser for the pw command."""
    argparser = _ArgumentParserWithBanner(
        prog='pw',
        add_help=False,
        description=_HELP_HEADER,
        formatter_class=argparse.RawDescriptionHelpFormatter)

    def directory(arg: str) -> Path:
        path = Path(arg)
        if path.is_dir():
            return path.resolve()

        raise argparse.ArgumentTypeError(f'{path} is not a directory')

    def log_level(arg: str) -> int:
        try:
            return getattr(logging, arg.upper())
        except AttributeError:
            raise argparse.ArgumentTypeError(
                f'{arg.upper()} is not a valid log level')

    # Do not use the built-in help argument so that displaying the help info can
    # be deferred until the pw plugins have been registered.
    argparser.add_argument('-h',
                           '--help',
                           action='store_true',
                           help='Display this help message and exit')
    argparser.add_argument(
        '-C',
        '--directory',
        type=directory,
        default=Path.cwd(),
        help='Change to this directory before doing anything')
    argparser.add_argument(
        '-l',
        '--loglevel',
        type=log_level,
        default=logging.INFO,
        help='Set the log level (debug, info, warning, error, critical)')
    argparser.add_argument('--no-banner',
                           action='store_true',
                           help='Do not print the Pigweed banner')
    argparser.add_argument(
        'command',
        nargs='?',
        help='Which command to run; see supported commands below')
    argparser.add_argument(
        'plugin_args',
        metavar='...',
        nargs=argparse.REMAINDER,
        help='Remaining arguments are forwarded to the command')

    return argparser
