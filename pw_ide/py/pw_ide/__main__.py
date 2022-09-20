# Copyright 2022 The Pigweed Authors
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
"""Configure IDE support for Pigweed projects."""

import argparse
from pathlib import Path
import sys
from typing import (Any, Callable, cast, Dict, Generic, NoReturn, Optional,
                    TypeVar, Union)
from pw_ide.commands import (cmd_info, cmd_init, cmd_cpp, cmd_python,
                             cmd_setup)

# TODO(chadnorvell): Move this docstring-as-argparse-docs functionality
# to pw_cli.
T = TypeVar('T')


class Maybe(Generic[T]):
    """A very rudimentary monadic option type.

    This makes it easier to multiple chain T -> T and T -> T? functions when
    dealing with Optional[T] values.

    For example, rather than doing:

    .. code-block:: python

        def f(x: T) -> Optional[T]: ...
        def g(x: T) -> T: ...

        def foo(i: Optional[T]) -> Optional[T]:
            if i is None:
                return None

            i = f(i)

            if j is None:
                return None

            return g(j)

    ... which becomes tedious as the number of functions increases, instead do:

    .. code-block:: python

        def f(x: T) -> Optional[T]: ...
        def g(x: T) -> T: ...

        def foo(i: Optional[T]) -> Optional[T]:
            return Maybe(i)\
                .and_then(f)\
                .and_then(g)\
                .to_optional()
    """
    def __init__(self, value: Optional[T]) -> None:
        self.value: Optional[T] = value

    def and_then(
        self, func: Union[Callable[[T], T],
                          Callable[[T], 'Maybe[T]']]) -> 'Maybe[T]':
        if self.value is None:
            return self

        result = func(self.value)

        if isinstance(result, self.__class__):
            return cast('Maybe[T]', result)

        return self.__class__(cast(T, result))

    def to_optional(self) -> Optional[T]:
        return self.value


def _reflow_multiline_string(doc: str) -> str:
    """Reflow a multi-line string by removing formatting newlines.

    This works on any string, but is intended for docstrings, where newlines
    are added within sentences and paragraphs to satisfy the whims of line
    length limits.

    Paragraphs are assumed to be surrounded by blank lines. They will be joined
    together without newlines, then each paragraph will be rejoined with blank
    line separators.
    """
    return '\n\n'.join([para.replace('\n', ' ') for para in doc.split('\n\n')])


def _multiline_string_summary(doc: str) -> str:
    """Return the one-line summary from a multi-line string.

    This works on any string, but is intended for docstrings, where you
    typically have a single line summary.
    """
    return doc.split('\n\n', maxsplit=1)[0]


def _reflow_docstring(doc: Optional[str]) -> Optional[str]:
    """Reflow an docstring."""
    return Maybe(doc)\
        .and_then(_reflow_multiline_string)\
        .to_optional()


def _docstring_summary(doc: Optional[str]) -> Optional[str]:
    """Return the one-line summary of a docstring."""
    return Maybe(doc)\
        .and_then(_reflow_multiline_string)\
        .and_then(_multiline_string_summary)\
        .to_optional()


def _parse_args() -> argparse.Namespace:
    parser_root = argparse.ArgumentParser(prog='pw ide', description=__doc__)

    parser_root.set_defaults(
        func=lambda *_args, **_kwargs: parser_root.print_help())

    subcommand_parser = parser_root.add_subparsers(help='Subcommands')

    parser_info = subcommand_parser.add_parser(
        'info',
        description=_docstring_summary(cmd_info.__doc__),
        help=_reflow_docstring(cmd_info.__doc__))
    parser_info.add_argument('--working-dir',
                             dest='working_dir',
                             action='store_true',
                             help='Report Pigweed IDE working directory.')
    parser_info.add_argument('--available-compdbs',
                             dest='available_compdbs',
                             action='store_true',
                             help='Report the compilation databases currently '
                             'in the working directory.')
    parser_info.add_argument('--available-targets',
                             dest='available_targets',
                             action='store_true',
                             help='Report all available targets, which are '
                             'targets that are defined in .pw_ide.yaml '
                             'and have compilation databases in the '
                             'working directory. This is equivalent to: '
                             'pw ide cpp --list')
    parser_info.add_argument('--defined-targets',
                             dest='defined_targets',
                             action='store_true',
                             help='Report all defined targets, which are '
                             'targets that are defined in .pw_ide.yaml.')
    parser_info.add_argument('--compdb-targets',
                             dest='compdb_file_for_targets',
                             type=Path,
                             metavar='COMPILATION_DATABASE',
                             help='Report all of the targets found in the '
                             'provided compilation database.')
    parser_info.set_defaults(func=cmd_info)

    parser_init = subcommand_parser.add_parser(
        'init',
        description=_docstring_summary(cmd_init.__doc__),
        help=_reflow_docstring(cmd_init.__doc__))
    parser_init.add_argument('--dir',
                             dest='make_dir',
                             action='store_true',
                             help='Create the Pigweed IDE working directory.')
    parser_init.add_argument('--clangd-wrapper',
                             dest='make_clangd_wrapper',
                             action='store_true',
                             help='Create a wrapper script for clangd.')
    parser_init.add_argument('--python-symlink',
                             dest='make_python_symlink',
                             action='store_true',
                             help='Create a symlink to the Python virtual '
                             'environment.')
    parser_init.set_defaults(func=cmd_init)

    parser_cpp = subcommand_parser.add_parser(
        'cpp',
        description=_docstring_summary(cmd_cpp.__doc__),
        help=_reflow_docstring(cmd_cpp.__doc__))
    parser_cpp.add_argument('-l',
                            '--list',
                            dest='should_list_targets',
                            action='store_true',
                            help='List the targets available for C/C++ '
                            'language analysis.')
    parser_cpp.add_argument('-s',
                            '--set',
                            dest='target_to_set',
                            metavar='TARGET',
                            help='Set the target to use for C/C++ language '
                            'server analysis.')
    parser_cpp.add_argument('--no-override',
                            dest='override_current_target',
                            action='store_const',
                            const=False,
                            default=True,
                            help='If called with --set, don\'t override the '
                            'current target if one is already set.')
    parser_cpp.add_argument('-p',
                            '--process',
                            dest='compdb_file_path',
                            type=Path,
                            metavar='COMPILATION_DATABASE_FILE',
                            help='Process a file matching the clang '
                            'compilation database format.')
    parser_cpp.set_defaults(func=cmd_cpp)

    parser_python = subcommand_parser.add_parser(
        'python',
        description=_docstring_summary(cmd_python.__doc__),
        help=_reflow_docstring(cmd_python.__doc__))
    parser_python.add_argument('-v',
                               '--virtual-env',
                               dest='should_get_venv_path',
                               action='store_true',
                               help='Return the path to the Pigweed Python '
                               'virtual environment.')
    parser_python.set_defaults(func=cmd_python)

    parser_setup = subcommand_parser.add_parser(
        'setup',
        description=_docstring_summary(cmd_setup.__doc__),
        help=_reflow_docstring(cmd_setup.__doc__))
    parser_setup.set_defaults(func=cmd_setup)

    args = parser_root.parse_args()
    return args


def _dispatch_command(func: Callable, **kwargs: Dict[str, Any]) -> int:
    """Dispatch arguments to a subcommand handler.

    Each CLI subcommand is handled by handler function, which is registered
    with the subcommand parser with `parser.set_defaults(func=handler)`.
    By calling this function with the parsed args, the appropriate subcommand
    handler is called, and the arguments are passed to it as kwargs.
    """
    return func(**kwargs)


def main() -> NoReturn:
    sys.exit(_dispatch_command(**vars(_parse_args())))


if __name__ == '__main__':
    main()
