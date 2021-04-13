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

from collections import defaultdict
from itertools import chain
import inspect
import textwrap
import types
from typing import Any, Collection, Dict, Iterable, Mapping, NamedTuple

import pw_status
from pw_protobuf_compiler import python_protos

import pw_rpc
from pw_rpc.descriptors import Method
from pw_rpc.console_tools import functions

_INDENT = '    '


class CommandHelper:
    """Used to implement a help command in an RPC console."""
    @classmethod
    def from_methods(cls,
                     methods: Iterable[Method],
                     variables: Mapping[str, object],
                     header: str,
                     footer: str = '') -> 'CommandHelper':
        return cls({m.full_name: m
                    for m in methods}, variables, header, footer)

    def __init__(self,
                 methods: Mapping[str, object],
                 variables: Mapping[str, object],
                 header: str,
                 footer: str = ''):
        self._methods = methods
        self._variables = variables
        self.header = header
        self.footer = footer

    def help(self, item: object = None) -> str:
        """Returns a help string with a command or all commands listed."""

        if item is None:
            all_vars = '\n'.join(sorted(self._variables_without_methods()))
            all_rpcs = '\n'.join(self._methods)
            return (f'{self.header}\n\n'
                    f'All variables:\n\n{textwrap.indent(all_vars, _INDENT)}'
                    '\n\n'
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

    def _variables_without_methods(self) -> Mapping[str, object]:
        packages = frozenset(
            n.split('.', 1)[0] for n in self._methods if '.' in n)

        return {
            name: var
            for name, var in self._variables.items() if name not in packages
        }

    def __call__(self, item: object = None) -> None:
        """Prints the help string."""
        print(self.help(item))

    def __repr__(self) -> str:
        """Returns the help, so foo and foo() are equivalent in a console."""
        return self.help()


class ClientInfo(NamedTuple):
    """Information about an RPC client as it appears in the console."""
    # The name to use in the console to refer to this client.
    name: str

    # An object to use in the console for the client. May be a pw_rpc.Client.
    client: object

    # The pw_rpc.Client; may be the same object as client.
    rpc_client: pw_rpc.Client


class Context:
    """The Context class is used to set up an interactive RPC console.

    The Context manages a set of variables that make it easy to access RPCs and
    protobufs in a REPL.

    As an example, this class can be used to set up a console with IPython:

    .. code-block:: python

      context = console_tools.Context(
          clients, default_client, protos, help_header=WELCOME_MESSAGE)
      IPython.terminal.embed.InteractiveShellEmbed().mainloop(
          module=types.SimpleNamespace(**context.variables()))
    """
    def __init__(self,
                 client_info: Collection[ClientInfo],
                 default_client: Any,
                 protos: python_protos.Library,
                 *,
                 help_header: str = '') -> None:
        """Creates an RPC console context.

        Protos and RPC services are accessible by their proto package and name.
        The target for these can be set with the set_target function.

        Args:
          client_info: ClientInfo objects that represent the clients this
              console uses to communicate with other devices
          default_client: default client object; must be one of the clients
          protos: protobufs to use for RPCs for all clients
          help_header: Message to display for the help command
        """
        assert client_info, 'At least one client must be provided!'

        self.client_info = client_info
        self.current_client = default_client
        self.protos = protos

        # Store objects with references to RPC services, sorted by package.
        self._services: Dict[str, types.SimpleNamespace] = defaultdict(
            types.SimpleNamespace)

        self._variables: Dict[str, object] = dict(
            Status=pw_status.Status,
            set_target=functions.help_as_repr(self.set_target),
            # The original built-in help function is available as 'python_help'.
            python_help=help,
        )

        # Make the RPC clients and protos available in the console.
        self._variables.update((c.name, c.client) for c in self.client_info)

        # Make the proto package hierarchy directly available in the console.
        for package in self.protos.packages:
            self._variables[package._package] = package  # pylint: disable=protected-access

        # Monkey patch the message types to use an improved repr function.
        for message_type in self.protos.messages():
            message_type.__repr__ = python_protos.proto_repr

        # Set up the 'help' command.
        all_methods = chain.from_iterable(c.rpc_client.methods()
                                          for c in self.client_info)
        self._helper = CommandHelper.from_methods(
            all_methods, self._variables, help_header,
            'Type a command and hit Enter to see detailed help information.')

        self._variables['help'] = self._helper

        # Call set_target to set up for the default target.
        self.set_target(self.current_client)

    def variables(self) -> Dict[str, Any]:
        """Returns a mapping of names to variables for use in an RPC console."""
        return self._variables

    def set_target(self,
                   selected_client: object,
                   channel_id: int = None) -> None:
        """Sets the default target for commands."""
        # Make sure the variable is one of the client variables.
        name = ''
        rpc_client: Any = None

        for name, client, rpc_client in self.client_info:
            if selected_client is client:
                print('CURRENT RPC TARGET:', name)
                break
        else:
            raise ValueError('Supported targets :' +
                             ', '.join(c.name for c in self.client_info))

        # Update the RPC services to use the newly selected target.
        for service_client in rpc_client.channel(channel_id).rpcs:
            # Patch all method protos to use the improved __repr__ function too.
            for method in (m.method for m in service_client):
                method.request_type.__repr__ = python_protos.proto_repr
                method.response_type.__repr__ = python_protos.proto_repr

            service = service_client._service  # pylint: disable=protected-access
            setattr(self._services[service.package], service.name,
                    service_client)

        # Add the RPC methods to their proto packages.
        for package_name, rpcs in self._services.items():
            self.protos.packages[package_name]._add_item(rpcs)  # pylint: disable=protected-access

        self.current_client = selected_client
