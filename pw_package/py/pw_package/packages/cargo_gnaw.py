# Copyright 2025 The Pigweed Authors
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
"""Install and check status of cargo_gnaw."""

import logging
import os
from pathlib import Path
import subprocess
from typing import Sequence

import pw_package.git_repo
import pw_package.package_manager

_LOG = logging.getLogger(__package__)


class CargoGnaw(pw_package.git_repo.GitRepo):
    """Install and check status of cargo_gnaw."""

    def __init__(self, *args, **kwargs):
        # pylint: disable=line-too-long
        super().__init__(
            *args,
            name='cargo_gnaw',
            url='https://pigweed.googlesource.com/third_party/fuchsia/cargo-gnaw',
            commit='8ef41dbfb1855d7d4faa0af59f253d9d6a6efada',
            **kwargs,
        )
        # pylint: enable=line-too-long
        self.install_path = None

    def install(self, path: Path) -> None:
        super().install(path)

        env = os.environ.copy()

        def log_and_run(command: Sequence[str], **kwargs):
            _LOG.info('==> %s', ' '.join(command))
            return subprocess.run(
                command,
                env=env,
                check=True,
                **kwargs,
            )

        bootstrap_env_path = Path(env.get('_PW_ACTUAL_ENVIRONMENT_ROOT', ''))
        if not bootstrap_env_path.is_dir():
            raise EnvironmentError('Environment is not properly bootstrapped!')

        install_path = bootstrap_env_path / 'cipd' / 'packages' / 'pigweed'
        log_and_run(
            (
                'cargo',
                'install',
                '--root',
                str(install_path),
                '--path',
                str(path),
                '--locked',
            )
        )
        self.install_path = install_path
        _LOG.info('Done! gnaw installs successfully!')

    def info(self, path: Path) -> Sequence[str]:
        if self.install_path is None:
            raise ValueError(f'{self.name} install failed!')
        return (
            f'{self.name} source cloned to {path}, '
            f'installed to {self.install_path}.',
        )


pw_package.package_manager.register(CargoGnaw)
