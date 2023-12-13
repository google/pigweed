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
from dataclasses import dataclass, field
from enum import Enum
import logging
from pathlib import Path
import sys
from typing import List, NoReturn, Optional

from pw_cli import argument_types, plugins
from pw_cli.branding import banner
import pw_cli.env

_HELP_HEADER = '''The Pigweed command line interface (CLI).

Example uses:
    pw logdemo
    pw --loglevel debug watch -C out
'''


def parse_args() -> argparse.Namespace:
    return arg_parser().parse_args()


class ShellCompletionFormat(Enum):
    """Supported shell tab completion modes."""

    BASH = 'bash'
    ZSH = 'zsh'


@dataclass(frozen=True)
class ShellCompletion:
    option_strings: List[str] = field(default_factory=list)
    help: Optional[str] = None
    choices: Optional[List[str]] = None
    flag: bool = True

    def bash_completion(self, text: str) -> List[str]:
        result: List[str] = []
        for option_str in self.option_strings:
            if option_str.startswith(text):
                result.append(option_str)
        return result

    def zsh_completion(self, text: str) -> List[str]:
        result: List[str] = []
        for option_str in self.option_strings:
            if option_str.startswith(text):
                short_and_long_opts = ' '.join(self.option_strings)
                # '(-h --help)-h[Display help message and exit]'
                # '(-h --help)--help[Display help message and exit]'
                help_text = self.help if self.help else ''
                state_str = ''
                if not self.flag:
                    state_str = ': :->' + option_str

                result.append(
                    f'({short_and_long_opts}){option_str}[{help_text}]'
                    f'{state_str}'
                )
        return result


def get_options_and_help(
    parser: argparse.ArgumentParser,
) -> List[ShellCompletion]:
    return list(
        ShellCompletion(
            option_strings=list(action.option_strings),
            help=action.help,
            choices=list(action.choices) if action.choices else [],
            flag=action.nargs == 0,
        )
        for action in parser._actions  # pylint: disable=protected-access
    )


def print_completions_for_option(
    parser: argparse.ArgumentParser,
    text: str = '',
    tab_completion_format: str = ShellCompletionFormat.BASH.value,
) -> None:
    matched_lines: List[str] = []
    for completion in get_options_and_help(parser):
        if tab_completion_format == ShellCompletionFormat.ZSH.value:
            matched_lines.extend(completion.zsh_completion(text))
        else:
            matched_lines.extend(completion.bash_completion(text))

    for line in matched_lines:
        print(line)


def print_banner() -> None:
    """Prints the PIGWEED (or project specific) banner to stderr."""
    parsed_env = pw_cli.env.pigweed_environment()
    if parsed_env.PW_ENVSETUP_NO_BANNER or parsed_env.PW_ENVSETUP_QUIET:
        return
    print(banner() + '\n', file=sys.stderr)


def format_help(registry: plugins.Registry) -> str:
    """Returns the pw help information as a string."""
    return f'{arg_parser().format_help()}\n{registry.short_help()}'


class _ArgumentParserWithBanner(argparse.ArgumentParser):
    """Parser that the Pigweed banner when there are parsing errors."""

    def error(self, message: str) -> NoReturn:
        print_banner()
        self.print_usage(sys.stderr)
        self.exit(2, f'{self.prog}: error: {message}\n')


def add_tab_complete_arguments(
    parser: argparse.ArgumentParser,
) -> argparse.ArgumentParser:
    parser.add_argument(
        '--tab-complete-option',
        nargs='?',
        help='Print tab completions for the supplied option text.',
    )
    parser.add_argument(
        '--tab-complete-format',
        choices=list(shell.value for shell in ShellCompletionFormat),
        default='bash',
        help='Output format for tab completion results.',
    )
    return parser


def arg_parser() -> argparse.ArgumentParser:
    """Creates an argument parser for the pw command."""
    argparser = _ArgumentParserWithBanner(
        prog='pw',
        add_help=False,
        description=_HELP_HEADER,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    # Do not use the built-in help argument so that displaying the help info can
    # be deferred until the pw plugins have been registered.
    argparser.add_argument(
        '-h',
        '--help',
        action='store_true',
        help='Display this help message and exit',
    )
    argparser.add_argument(
        '-C',
        '--directory',
        type=argument_types.directory,
        default=Path.cwd(),
        help='Change to this directory before doing anything',
    )
    argparser.add_argument(
        '-l',
        '--loglevel',
        type=argument_types.log_level,
        default=logging.INFO,
        help='Set the log level (debug, info, warning, error, critical)',
    )
    argparser.add_argument(
        '--debug-log',
        help=(
            'Additionally log to this file at debug level; does not affect '
            'terminal output'
        ),
    )
    argparser.add_argument(
        '--no-banner',
        action='store_true',
        help='Do not print the Pigweed banner',
    )
    argparser.add_argument(
        '--tab-complete-command',
        nargs='?',
        help='Print tab completions for the supplied command text.',
    )
    argparser.add_argument(
        'command',
        nargs='?',
        help='Which command to run; see supported commands below',
    )
    argparser.add_argument(
        'plugin_args',
        metavar='...',
        nargs=argparse.REMAINDER,
        help='Remaining arguments are forwarded to the command',
    )
    return add_tab_complete_arguments(argparser)
