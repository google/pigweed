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
"""Defines argument types for use with argparse."""

import argparse
from collections import defaultdict
import logging
from pathlib import Path


def directory(arg: str) -> Path:
    path = Path(arg)
    if path.is_dir():
        return path.resolve()

    raise argparse.ArgumentTypeError(f'"{path}" is not a directory')


def log_level(arg: str) -> int:
    try:
        return getattr(logging, arg.upper())
    except AttributeError:
        raise argparse.ArgumentTypeError(
            f'"{arg.upper()}" is not a valid log level'
        )


class DictOfListsAction(argparse.Action):
    """A custom action for dict[str, list[str]] arguments.

    This action expects repeated arguments of the following form:

        --main-arg=key=--arg1
        --main-arg key=--arg2
        --main-arg key_b=--baz

    And produces a in a dictionary of this form:
        main_arg = {
            'key': ['--arg1', '--arg2'],
            'key_b': ['--baz'],
        }
    """

    def __init__(self, option_strings, dest, nargs=None, **kwargs):
        if nargs is not None:
            raise ValueError('nargs is not allowed for DictOfListsAction')
        metavar = kwargs.get('metavar')
        if metavar:
            if isinstance(metavar, (tuple, list)):
                if len(metavar) != 2:
                    raise ValueError(
                        'metavar for DictOfListsAction as a tuple must have '
                        'two strings'
                    )
                kwargs['metavar'] = f'{metavar[0]}={metavar[1]}'
            elif isinstance(metavar, str):
                if metavar.count('=') != 1:
                    raise ValueError(
                        'metavar for DictOfListsAction as a string must '
                        'contain exactly one "="'
                    )
            else:
                raise ValueError(
                    'metavar for DictOfListsAction must be a string or a '
                    'tuple of two strings'
                )
        else:
            kwargs['metavar'] = 'KEY=VALUE'

        super().__init__(option_strings, dest, nargs=nargs, **kwargs)

    def __call__(self, parser, namespace, values, option_string=None):
        dest_dict = getattr(namespace, self.dest, None)
        if dest_dict is None:
            dest_dict = defaultdict(list)
            setattr(namespace, self.dest, dest_dict)

        if not isinstance(values, str) or '=' not in values:
            parser.error(
                f'argument {"/".join(self.option_strings)}: '
                f'expected {self.metavar} format, but got '
                f'value: `{values}`'
            )

        key, value = values.split('=', 1)
        dest_dict[key].append(value)
