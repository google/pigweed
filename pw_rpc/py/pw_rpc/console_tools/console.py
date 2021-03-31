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
"""Utilities for creating an interactive console."""

import inspect
import textwrap
from typing import Any, Dict, Iterable

from pw_rpc.descriptors import Method

_INDENT = '    '


class CommandHelper:
    """Used to implement a help command in an RPC console."""
    @classmethod
    def from_methods(cls,
                     methods: Iterable[Method],
                     header: str,
                     footer: str = '') -> 'CommandHelper':
        return cls({m.full_name: m for m in methods}, header, footer)

    def __init__(self, methods: Dict[str, Any], header: str, footer: str = ''):
        self._methods = methods
        self.header = header
        self.footer = footer

    def help(self, item: Any = None) -> str:
        """Returns a help string with a command or all commands listed."""

        if item is None:
            all_rpcs = '\n'.join(self._methods)
            return (f'{self.header}\n\n'
                    f'All commands:\n\n{textwrap.indent(all_rpcs, _INDENT)}'
                    f'\n\n{self.footer}'.strip())

        # If item is a string, find commands matching that.
        if isinstance(item, str):
            matches = {n: m for n, m in self._methods.items() if item in n}
            if not matches:
                return f'No matches found for {item!r}'

            if len(matches) == 1:
                name, method = next(iter(matches.items()))
                return f'{name}\n\n{inspect.getdoc(method)}'

            return f'Multiple matches for {item!r}:\n\n' + textwrap.indent(
                '\n'.join(matches), _INDENT)

        return inspect.getdoc(item) or f'No documentation for {item!r}.'

    def __call__(self, item: Any = None) -> None:
        """Prints the help string."""
        print(self.help(item))

    def __repr__(self) -> str:
        """Returns the help, so foo and foo() are equivalent in a console."""
        return self.help()
