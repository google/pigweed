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
"""Registry for plugins."""

import argparse
import collections
import importlib
import inspect
import logging
from pathlib import Path
import sys
from textwrap import TextWrapper
from typing import Callable, Dict, List, Iterable, Optional, Union

from pw_cli import arguments

_LOG = logging.getLogger(__name__)

REGISTRY_FILE = 'PW_PLUGINS'


class Error(Exception):
    """Failed to register a Pigweed plugin."""
    def __str__(self):
        """Displays the error as a string, including the __cause__ if present.

        Adding __cause__ gives useful context without displaying a backtrace.
        """
        if self.__cause__ is None:
            return super().__str__()

        return (f'{super().__str__()} '
                f'({type(self.__cause__).__name__}: {self.__cause__})')


class _Plugin:
    """A plugin for the pw command."""
    def __init__(self, name: str, module: str, function: str,
                 source: Union[Path, str]):
        self.name = name
        self.source = source

        # Attempt to access the module and function. Catch any errors that might
        # occur, since a bad plugin shouldn't break the rest of the pw command.
        try:
            self._module = importlib.import_module(module)
        except Exception as err:
            raise Error(f'Failed to import module "{module}"') from err

        try:
            self._function: Callable[[], int] = getattr(self._module, function)
        except AttributeError as err:
            raise Error(
                f'The function "{module}.{function}" does not exist') from err

        try:
            params = inspect.signature(self._function).parameters
        except TypeError:
            raise Error(
                'Plugin functions must be callable, but '
                f'{module}.{function} is a {type(self._function).__name__}')

        positional_args = sum(p.default == p.empty for p in params.values())
        if positional_args:
            raise Error(
                f'Plugin functions cannot have any required positional '
                f'arguments, but {module}.{function} has {positional_args}')

    def run(self, args: List[str]) -> int:
        original_sys_argv = sys.argv
        sys.argv = [f'pw {self.name}', *args]

        try:
            return self._function()
        finally:
            sys.argv = original_sys_argv

    def help(self) -> str:
        """Returns a brief description of this plugin."""
        return self._function.__doc__ or self._module.__doc__ or ''

    def details(self) -> List[str]:
        return [
            f'help      {self.help()}',
            f'module    {self._module.__name__}',
            f'function  {self._function.__name__}',
            f'source    {self.source}',
        ]


# This is the global CLI plugin registry.
_registry: Dict[str, _Plugin] = {}
_sources: List[Path] = []  # Paths to PW_PLUGINS files
_errors: Dict[str, List[Error]] = collections.defaultdict(list)


def _get(name: str) -> _Plugin:
    if name in _registry:
        return _registry[name]

    if name in _errors:
        raise Error(f'Registration for "{name}" failed: ' +
                    ', '.join(str(e) for e in _errors[name]))

    raise Error(f'The plugin "{name}" has not been registered')


def errors() -> Dict[str, List[Error]]:
    return _errors


def run(name: str, args: List[str]) -> int:
    """Runs a plugin by name. Raises Error if the plugin is not registered."""
    return _get(name).run(args)


def command_help() -> str:
    """Returns a help string for the registered plugins."""
    width = max(len(name) for name in _registry) + 1 if _registry else 1
    help_items = '\n'.join(f'  {name:{width}} {plugin.help()}'
                           for name, plugin in sorted(_registry.items()))
    return f'supported commands:\n{help_items}'


_BUILTIN_PLUGIN = '<built-in>'


def _valid_registration(name: str, module: str, function: str,
                        source: Union[Path, str]) -> bool:
    """Determines if a plugin should be registered or not."""
    existing = _registry.get(name)

    if existing is None:
        return True

    if source == _BUILTIN_PLUGIN:
        raise Error(
            f'Attempted to register built-in plugin "{name}", but that '
            f'plugin was previously registered ({existing.source})!')

    if existing.source == _BUILTIN_PLUGIN:
        _LOG.debug('%s: Overriding built-in plugin "%s" with %s.%s', source,
                   name, module, function)
        return True

    if source == _registry[name].source:
        _LOG.warning(
            '%s: "%s" is registered multiple times in this file! '
            'Only the first registration takes effect', source, name)
    else:
        _LOG.debug(
            '%s: The plugin "%s" was previously registered in %s; '
            'ignoring registration as %s.%s', source, name,
            _registry[name].source, module, function)

    return False


def _register(name: str,
              module: str,
              function: str,
              source: Union[Path, str] = _BUILTIN_PLUGIN) -> None:
    """Registers a plugin from the specified source."""

    if not _valid_registration(name, module, function, source):
        return

    try:
        _registry[name] = _Plugin(name, module, function, source)
        _LOG.debug('%s: Registered plugin "%s" for %s.%s', source, name,
                   module, function)
    except Error as err:
        _errors[name].append(err)
        _LOG.error('%s: Failed to register plugin "%s": %s', source, name, err)


def find_in_parents(name: str, path: Path) -> Optional[Path]:
    """Searches parent directories of the path for a file or directory."""
    path = path.resolve()

    while not path.joinpath(name).exists():
        path = path.parent

        if path.samefile(path.parent):
            return None

    return path.joinpath(name)


def find_all_in_parents(name: str, path: Path) -> Iterable[Path]:
    """Searches all parent directories of the path for files or directories."""

    while True:
        result = find_in_parents(name, path)
        if result is None:
            return

        yield result
        path = result.parent.parent


def _register_builtin_plugins():
    """Registers the commands that are included with pw by default."""

    _register('doctor', 'pw_doctor.doctor', 'main')
    _register('format', 'pw_presubmit.format_code', 'main')
    _register('help', 'pw_cli.plugins', '_help_command')
    _register('logdemo', 'pw_cli.log', 'main')
    _register('module-check', 'pw_module.check', 'main')
    _register('test', 'pw_unit_test.test_runner', 'main')
    _register('watch', 'pw_watch.watch', 'main')


def register(directory: Path):
    """Finds and registers command line plugins."""
    _register_builtin_plugins()

    # Find pw plugins files starting in the current and parent directories.
    for path in find_all_in_parents(REGISTRY_FILE, directory):
        if not path.is_file():
            continue

        _LOG.debug('Found plugins file %s', path)
        _sources.append(path)

        with path.open() as contents:
            for lineno, line in enumerate(contents, 1):
                line = line.strip()
                if line and not line.startswith('#'):
                    try:
                        name, module, function = line.split()
                        _register(name, module, function, path)
                    except ValueError as err:
                        _errors[line.strip()].append(Error(err))
                        _LOG.error(
                            '%s:%d: Failed to parse plugin entry "%s": '
                            'Expected 3 items (name, module, function), got %d',
                            path, lineno, line, len(line.split()))


def _help_text(plugins: Iterable[str] = ()) -> Iterable[str]:
    """Yields detailed information about commands."""
    yield arguments.format_help()

    if not plugins:
        plugins = list(_registry)

    yield '\ndetailed command information:'

    wrapper = TextWrapper(width=80,
                          initial_indent='   ',
                          subsequent_indent=' ' * 13)

    for plugin in sorted(plugins):
        yield f'  [{plugin}]'

        try:
            for line in _get(plugin).details():
                yield wrapper.fill(line)
        except Error as err:
            yield wrapper.fill(f'error     {err}')

        yield ''

    yield 'PW_PLUGINS files:'

    if _sources:
        yield from (f'  [{i}] {file}' for i, file in enumerate(_sources, 1))
    else:
        yield '  (none found)'


def _help_command():
    """Display detailed information about pw commands."""
    parser = argparse.ArgumentParser(description=_help_command.__doc__)
    parser.add_argument('plugins',
                        metavar='plugin',
                        nargs='*',
                        help='command for which to display detailed info')

    for line in _help_text(**vars(parser.parse_args())):
        print(line, file=sys.stderr)
