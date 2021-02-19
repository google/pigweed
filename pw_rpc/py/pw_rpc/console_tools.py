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
"""Utilities for building tools that interact with pw_rpc."""

import inspect
import textwrap
import threading
from typing import Any, Callable, Dict, Iterable

from pw_rpc.descriptors import Method


class Watchdog:
    """Simple class that times out unless reset.

    This class could be used, for example, to track a device's connection state
    for devices that send a periodic heartbeat packet.
    """
    def __init__(self,
                 on_reset: Callable[[], Any],
                 on_expiration: Callable[[], Any],
                 while_expired: Callable[[], Any] = lambda: None,
                 timeout_s: float = 1,
                 expired_timeout_s: float = None):
        """Creates a watchdog; start() must be called to start it.

        Args:
          on_reset: Function called when the watchdog is reset after having
              expired.
          on_expiration: Function called when the timeout expires.
          while_expired: Function called repeatedly while the watchdog is
              expired.
          timeout_s: If reset() is not called for timeout_s, the watchdog
              expires and calls the on_expiration callback.
          expired_timeout_s: While expired, the watchdog calls the
              while_expired callback every expired_timeout_s.
        """
        self._on_reset = on_reset
        self._on_expiration = on_expiration
        self._while_expired = while_expired

        self.timeout_s = timeout_s

        if expired_timeout_s is None:
            self.expired_timeout_s = self.timeout_s * 10
        else:
            self.expired_timeout_s = expired_timeout_s

        self.expired: bool = False
        self._watchdog = threading.Timer(0, self._timeout_expired)

    def start(self) -> None:
        """Starts the watchdog; must be called for the watchdog to work."""
        self._watchdog.cancel()
        self._watchdog = threading.Timer(
            self.expired_timeout_s if self.expired else self.timeout_s,
            self._timeout_expired)
        self._watchdog.daemon = True
        self._watchdog.start()

    def reset(self) -> None:
        """Resets the timeout; calls the on_reset callback if expired."""
        if self.expired:
            self.expired = False
            self._on_reset()

        self.start()

    def _timeout_expired(self) -> None:
        if self.expired:
            self._while_expired()
        else:
            self.expired = True
            self._on_expiration()

        self.start()


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
