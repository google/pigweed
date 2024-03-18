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
"""Pigweed Watch config file preferences loader."""

import argparse
import copy
import shlex
from pathlib import Path
from typing import Any, Callable, Dict, Tuple, Union

from pw_config_loader import yaml_config_loader_mixin

_DEFAULT_CONFIG: Dict[Any, Any] = {
    # Config settings not available as a command line options go here.
    'build_system_commands': {
        'default': {
            'commands': [
                {
                    'command': 'ninja',
                    'extra_args': [],
                },
            ],
        },
    },
}

_DEFAULT_PROJECT_FILE = Path('$PW_PROJECT_ROOT/.pw_build.yaml')
_DEFAULT_PROJECT_USER_FILE = Path('$PW_PROJECT_ROOT/.pw_build.user.yaml')
_DEFAULT_USER_FILE = Path('$HOME/.pw_build.yaml')


def load_defaults_from_argparse(
    add_parser_arguments: Callable[
        [argparse.ArgumentParser], argparse.ArgumentParser
    ]
) -> Dict[Any, Any]:
    parser = argparse.ArgumentParser(
        description='', formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser = add_parser_arguments(parser)
    default_namespace, _unknown_args = parser.parse_known_args(
        [],  # Pass in blank arguments to avoid catching args from sys.argv.
    )
    defaults_flags = vars(default_namespace)
    return defaults_flags


class ProjectBuilderPrefs(yaml_config_loader_mixin.YamlConfigLoaderMixin):
    """Pigweed Watch preferences storage class."""

    def __init__(
        self,
        load_argparse_arguments: Callable[
            [argparse.ArgumentParser], argparse.ArgumentParser
        ],
        project_file: Union[Path, bool] = _DEFAULT_PROJECT_FILE,
        project_user_file: Union[Path, bool] = _DEFAULT_PROJECT_USER_FILE,
        user_file: Union[Path, bool] = _DEFAULT_USER_FILE,
    ) -> None:
        self.load_argparse_arguments = load_argparse_arguments

        self.config_init(
            config_section_title='pw_build',
            project_file=project_file,
            project_user_file=project_user_file,
            user_file=user_file,
            default_config=_DEFAULT_CONFIG,
            environment_var='PW_BUILD_CONFIG_FILE',
        )

    def reset_config(self) -> None:
        super().reset_config()
        self._update_config(
            load_defaults_from_argparse(self.load_argparse_arguments),
            yaml_config_loader_mixin.Stage.DEFAULT,
        )

    def _argparse_build_system_commands_to_prefs(  # pylint: disable=no-self-use
        self, argparse_input: list[list[str]]
    ) -> Dict[str, Any]:
        result = copy.copy(_DEFAULT_CONFIG['build_system_commands'])
        for out_dir, command in argparse_input:
            new_dir_spec = result.get(out_dir, {})
            # Get existing commands list
            new_commands = new_dir_spec.get('commands', [])

            # Convert 'ninja -k 1' to 'ninja' and ['-k', '1']
            extra_args = []
            command_tokens = shlex.split(command)
            if len(command_tokens) > 1:
                extra_args = command_tokens[1:]
                command = command_tokens[0]

            # Append the command step
            new_commands.append({'command': command, 'extra_args': extra_args})
            new_dir_spec['commands'] = new_commands
            result[out_dir] = new_dir_spec
        return result

    def apply_command_line_args(self, new_args: argparse.Namespace) -> None:
        default_args = load_defaults_from_argparse(self.load_argparse_arguments)

        # Only apply settings that differ from the defaults.
        changed_settings: Dict[Any, Any] = {}
        for key, value in vars(new_args).items():
            if key in default_args and value != default_args[key]:
                if key == 'build_system_commands':
                    value = self._argparse_build_system_commands_to_prefs(value)
                changed_settings[key] = value

        self._update_config(
            changed_settings,
            yaml_config_loader_mixin.Stage.DEFAULT,
        )

    @property
    def run_commands(self) -> list[str]:
        return self._config.get('run_command', [])

    @property
    def build_directories(self) -> Dict[str, list[str]]:
        """Returns build directories and the targets to build in each."""
        build_directories: Union[
            list[str], Dict[str, list[str]]
        ] = self._config.get('build_directories', {})
        final_build_dirs: Dict[str, list[str]] = {}

        if isinstance(build_directories, dict):
            final_build_dirs = build_directories
        else:
            # Convert list style command line arg to dict
            for build_dir in build_directories:
                # build_dir should be a list of strings from argparse
                assert isinstance(build_dir, list)
                assert isinstance(build_dir[0], str)
                build_dir_name = build_dir[0]
                new_targets = build_dir[1:]
                # Append new targets in case out dirs are repeated on the
                # command line. For example:
                #   -C out python.tests -C out python.lint
                existing_targets = final_build_dirs.get(build_dir_name, [])
                existing_targets.extend(new_targets)
                final_build_dirs[build_dir_name] = existing_targets

        # If no build directory was specified fall back to 'out' with
        # default_build_targets or empty targets. If run_commands were supplied,
        # only run those by returning an empty final_build_dirs list.
        if not final_build_dirs and not self.run_commands:
            final_build_dirs['out'] = self._config.get(
                'default_build_targets', []
            )

        return final_build_dirs

    def _get_build_system_commands_for(self, build_dir: str) -> Dict[str, Any]:
        config_dict = self._config.get('build_system_commands', {})
        if not config_dict:
            config_dict = _DEFAULT_CONFIG['build_system_commands']
        default_system_commands: Dict[str, Any] = config_dict.get('default', {})
        if default_system_commands is None:
            default_system_commands = {}
        build_system_commands = config_dict.get(build_dir)

        # In case 'out:' is in the config but has no contents.
        if not build_system_commands:
            return default_system_commands

        return build_system_commands

    def build_system_commands(
        self, build_dir: str
    ) -> list[Tuple[str, list[str]]]:
        build_system_commands = self._get_build_system_commands_for(build_dir)

        command_steps: list[Tuple[str, list[str]]] = []
        commands: list[Dict[str, Any]] = build_system_commands.get(
            'commands', []
        )
        for command_step in commands:
            command_steps.append(
                (command_step['command'], command_step['extra_args'])
            )
        return command_steps
