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
"""pw_ide configuration."""

import os
from pathlib import Path
from typing import Any, Dict, List, Optional, Union

from pw_console.yaml_config_loader_mixin import YamlConfigLoaderMixin

PW_IDE_DIR_NAME = '.pw_ide'

_PW_IDE_DEFAULT_DIR = Path(
    os.path.expandvars('$PW_PROJECT_ROOT')) / PW_IDE_DIR_NAME

_DEFAULT_CONFIG = {
    'setup': [],
    'targets': [],
    'working_dir': _PW_IDE_DEFAULT_DIR,
}

_DEFAULT_PROJECT_FILE = Path('$PW_PROJECT_ROOT/.pw_ide.yaml')
_DEFAULT_PROJECT_USER_FILE = Path('$PW_PROJECT_ROOT/.pw_ide.user.yaml')
_DEFAULT_USER_FILE = Path('$HOME/.pw_ide.yaml')


class IdeSettings(YamlConfigLoaderMixin):
    """Pigweed IDE features settings storage class."""
    def __init__(
        self,
        project_file: Union[Path, bool] = _DEFAULT_PROJECT_FILE,
        project_user_file: Union[Path, bool] = _DEFAULT_PROJECT_USER_FILE,
        user_file: Union[Path, bool] = _DEFAULT_USER_FILE,
        default_config: Optional[Dict[str, Any]] = None,
    ) -> None:
        self.config_init(
            config_section_title='pw_ide',
            project_file=project_file,
            project_user_file=project_user_file,
            user_file=user_file,
            default_config=_DEFAULT_CONFIG
            if default_config is None else default_config,
            environment_var='PW_IDE_CONFIG_FILE',
        )

    @property
    def working_dir(self) -> Path:
        """Path to the pw_ide working directory.

        This should not be a directory that's regularly deleted or manipulated
        by other processes (e.g. the GN `out` directory) nor should it be
        committed to the code repo.
        """
        return Path(self._config.get('working_dir', ''))

    @property
    def targets(self) -> List[str]:
        """The list of targets that should be made available for code analysis.

        In this case, "target" is analogous to a GN target, i.e., a particular
        build configuration. Targets defined here will be used when processing
        a compilation database.
        """
        return self._config.get('targets', list())

    @property
    def setup(self) -> List[str]:
        """`pw ide setup` should do everything necessary to get the project from
        a fresh checkout to a working default IDE experience. This defines the
        list of commands that makes that happen.

        Commands need to be formatted as lists in the way that Python's
        subprocess.run expects, since that's exactly where they're going.

        Note that this command must be idempotent, so that the user can run it
        whenever they want without a care in the world.
        """
        return self._config.get('setup', list())
