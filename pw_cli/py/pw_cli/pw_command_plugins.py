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
"""This module manages the global plugin registry for pw_cli."""

import argparse
from pathlib import Path
import sys
from typing import Iterable

from pw_cli import arguments, env, plugins
import pw_env_setup.config_file

plugin_registry = plugins.Registry(validator=plugins.callable_with_no_args)
REGISTRY_FILE = 'PW_PLUGINS'


def _register_builtin_plugins(registry: plugins.Registry) -> None:
    """Registers the commands that are included with pw by default."""

    # Register these by name to avoid circular dependencies.
    registry.register_by_name('bloat', 'pw_bloat.__main__', 'main')
    registry.register_by_name('doctor', 'pw_doctor.doctor', 'main')
    registry.register_by_name('format', 'pw_presubmit.format_code', 'main')
    registry.register_by_name('keep-sorted', 'pw_presubmit.keep_sorted', 'main')
    registry.register_by_name('logdemo', 'pw_cli.log', 'main')
    registry.register_by_name('module', 'pw_module.__main__', 'main')
    registry.register_by_name(
        'python-packages', 'pw_env_setup.python_packages', 'main'
    )
    registry.register_by_name('test', 'pw_unit_test.test_runner', 'main')
    registry.register_by_name('watch', 'pw_watch.watch', 'main')

    registry.register('help', _help_command)


def _help_command():
    """Display detailed information about pw commands."""
    parser = argparse.ArgumentParser(description=_help_command.__doc__)
    parser.add_argument(
        'plugins',
        metavar='plugin',
        nargs='*',
        help='command for which to display detailed info',
    )

    print(arguments.format_help(plugin_registry), file=sys.stderr)

    for line in plugin_registry.detailed_help(**vars(parser.parse_args())):
        print(line, file=sys.stderr)


def register() -> None:
    _register_builtin_plugins(plugin_registry)
    parsed_env = env.pigweed_environment()
    pw_plugins_file: Path = parsed_env.PW_PROJECT_ROOT / REGISTRY_FILE

    if pw_plugins_file.is_file():
        plugin_registry.register_file(pw_plugins_file)
    else:
        plugin_registry.register_config(
            config=pw_env_setup.config_file.load(),
            path=pw_env_setup.config_file.path(),
        )


def errors() -> dict:
    return plugin_registry.errors()


def format_help() -> str:
    return arguments.format_help(plugin_registry)


def run(name: str, args: Iterable[str]) -> int:
    return plugin_registry.run_with_argv(name, args)
