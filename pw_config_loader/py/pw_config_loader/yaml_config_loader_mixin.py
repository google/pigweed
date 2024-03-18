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
"""Yaml config file loader mixin."""

import enum
import os
import logging
from pathlib import Path
from typing import Any, Optional, Sequence

import yaml

_LOG = logging.getLogger(__package__)


class MissingConfigTitle(Exception):
    """Exception for when an existing YAML file is missing config_title."""


class Stage(enum.Enum):
    DEFAULT = 1
    PROJECT_FILE = 1
    USER_PROJECT_FILE = 2
    USER_FILE = 3
    ENVIRONMENT_VAR_FILE = 4
    OUT_OF_BAND = 5


class YamlConfigLoaderMixin:
    """Yaml Config file loader mixin.

    Use this mixin to load yaml file settings and save them into
    ``self._config``. For example:

    ::

       class ConsolePrefs(YamlConfigLoaderMixin):
           def __init__(self) -> None:
               self.config_init(
                   config_section_title='pw_console',
                   project_file=Path('project_file.yaml'),
                   project_user_file=Path('project_user_file.yaml'),
                   user_file=Path('~/user_file.yaml'),
                   default_config={},
                   environment_var='PW_CONSOLE_CONFIG_FILE',
               )

    """

    def config_init(
        self,
        config_section_title: str | Sequence[str],
        project_file: Optional[Path | bool] = None,
        project_user_file: Optional[Path | bool] = None,
        user_file: Optional[Path | bool] = None,
        default_config: Optional[dict[Any, Any]] = None,
        environment_var: Optional[str] = None,
        skip_files_without_sections: bool = False,
    ) -> None:
        """Call this to load YAML config files in order of precedence.

        The following files are loaded in this order:
        1. project_file
        2. project_user_file
        3. user_file

        Lastly, if a valid file path is specified at
        ``os.environ[environment_var]`` then load that file overriding all
        config options.

        Args:
            config_section_title: String name of this config section. For
                example: ``pw_console`` or ``pw_watch``. In the YAML file this
                is represented by a ``config_title`` key.

                ::

                   ---
                   config_title: pw_console

            project_file: Project level config file. This is intended to be a
                file living somewhere under a project folder and is checked into
                the repo. It serves as a base config all developers can inherit
                from.
            project_user_file: User's personal config file for a specific
                project. This can be a file that lives in a project folder that
                is git-ignored and not checked into the repo.
            user_file: A global user based config file. This is typically a file
                in the users home directory and settings here apply to all
                projects.
            default_config: A Python dict representing the base default
                config. This dict will be applied as a starting point before
                loading any yaml files.
            environment_var: The name of an environment variable to check for a
                config file. If a config file exists there it will be loaded on
                top of the default_config ignoring project and user files.
            skip_files_without_sections: Don't produce an exception if a
                config file doesn't include the relevant section. Instead, just
                move on to the next file.
        """

        self._config_section_title: tuple[str, ...]
        if isinstance(config_section_title, (list, tuple)):
            self._config_section_title = tuple(config_section_title)
        elif isinstance(config_section_title, str):
            self._config_section_title = (config_section_title,)
        else:
            raise TypeError(
                f'unexpected config section title {config_section_title!r}'
            )
        self.default_config = default_config if default_config else {}
        self.reset_config()

        if project_file and isinstance(project_file, Path):
            self.project_file = Path(
                os.path.expandvars(str(project_file.expanduser()))
            )
            self.load_config_file(
                self.project_file,
                skip_files_without_sections=skip_files_without_sections,
                stage=Stage.PROJECT_FILE,
            )

        if project_user_file and isinstance(project_user_file, Path):
            self.project_user_file = Path(
                os.path.expandvars(str(project_user_file.expanduser()))
            )
            self.load_config_file(
                self.project_user_file,
                skip_files_without_sections=skip_files_without_sections,
                stage=Stage.USER_PROJECT_FILE,
            )

        if user_file and isinstance(user_file, Path):
            self.user_file = Path(
                os.path.expandvars(str(user_file.expanduser()))
            )
            self.load_config_file(
                self.user_file,
                skip_files_without_sections=skip_files_without_sections,
                stage=Stage.USER_FILE,
            )

        # Check for a config file specified by an environment variable.
        if environment_var is None:
            return
        environment_config = os.environ.get(environment_var, None)
        if environment_config:
            env_file_path = Path(environment_config)
            if not env_file_path.is_file():
                raise FileNotFoundError(
                    f'Cannot load config file: {env_file_path}'
                )
            self.reset_config()
            self.load_config_file(
                env_file_path,
                skip_files_without_sections=skip_files_without_sections,
                stage=Stage.ENVIRONMENT_VAR_FILE,
            )

    def _update_config(self, cfg: dict[Any, Any], stage: Stage) -> None:
        if cfg is None:
            cfg = {}
        for key, value in cfg.items():
            if stage != Stage.DEFAULT:
                self._config[key] = self.handle_overloaded_value(
                    key=key,
                    stage=stage,
                    original_value=self._config.get(key),
                    overriding_value=value,
                )
            else:
                self._config[key] = value

    def handle_overloaded_value(  # pylint: disable=no-self-use
        self,
        key: str,  # pylint: disable=unused-argument
        stage: Stage,  # pylint: disable=unused-argument
        original_value: Any,  # pylint: disable=unused-argument
        overriding_value: Any,
    ) -> Any:
        """Overload this in subclasses to handle of overloaded values."""
        return overriding_value

    def reset_config(self) -> None:
        self._config: dict[Any, Any] = {}
        self._update_config(self.default_config, Stage.DEFAULT)

    def _load_config_from_string(  # pylint: disable=no-self-use
        self, file_contents: str
    ) -> list[dict[Any, Any]]:
        return list(yaml.safe_load_all(file_contents))

    def load_config_file(
        self,
        file_path: Path,
        skip_files_without_sections: bool = False,
        stage: Stage = Stage.OUT_OF_BAND,
    ) -> None:
        """Load a config file and extract the appropriate section."""
        if not file_path.is_file():
            return

        cfgs = self._load_config_from_string(file_path.read_text())

        for cfg in cfgs:
            cfg_copy = cfg
            for config_section_title in self._config_section_title:
                if config_section_title in cfg_copy:
                    cfg_copy = cfg_copy[config_section_title]
                else:
                    break
            else:
                self._update_config(cfg_copy, stage)
                continue

            config_title_value = '.'.join(self._config_section_title)
            if cfg.get('config_title', False) == config_title_value:
                self._update_config(cfg, stage)
                continue

            if skip_files_without_sections:
                pass
            else:
                raise MissingConfigTitle(
                    f'\n\nThe config file "{file_path}" is missing the '
                    f'expected "config_title: {config_title_value}" '
                    'setting.'
                )
