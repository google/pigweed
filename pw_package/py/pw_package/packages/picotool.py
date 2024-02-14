# Copyright 2023 The Pigweed Authors
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
"""Install and check status of picotool."""

from contextlib import contextmanager
import logging
import os
import pathlib
from pathlib import Path
import shutil
import subprocess
from typing import Sequence

import pw_package.git_repo
import pw_package.package_manager

_LOG = logging.getLogger(__package__)


@contextmanager
def change_working_dir(directory: Path):
    original_dir = Path.cwd()
    try:
        os.chdir(directory)
        yield directory
    finally:
        os.chdir(original_dir)


class Picotool(pw_package.package_manager.Package):
    """Install and check status of picotool."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, name='picotool', **kwargs)

        self._pico_tool_repo = pw_package.git_repo.GitRepo(
            name='picotool',
            url=(
                'https://pigweed.googlesource.com/third_party/'
                'github/raspberrypi/picotool.git'
            ),
            commit='f6fe6b7c321a2def8950d2a440335dfba19e2eab',
        )

    def install(self, path: Path) -> None:
        self._pico_tool_repo.install(path)

        env = os.environ.copy()
        env['PICO_SDK_PATH'] = str(path.parent.absolute() / 'pico_sdk')
        bootstrap_env_path = Path(env.get('_PW_ACTUAL_ENVIRONMENT_ROOT', ''))

        commands = (
            ('cmake', '-S', './', '-B', 'out/', '-G', 'Ninja'),
            ('ninja', '-C', 'out'),
        )

        with change_working_dir(path) as _picotool_repo:
            for command in commands:
                _LOG.info('==> %s', ' '.join(command))
                subprocess.run(
                    command,
                    env=env,
                    check=True,
                )

        picotool_bin = path / 'out' / 'picotool'
        _LOG.info('Done! picotool binary located at:')
        _LOG.info(picotool_bin)

        if bootstrap_env_path.is_dir() and picotool_bin.is_file():
            bin_path = (
                bootstrap_env_path / 'cipd' / 'packages' / 'pigweed' / 'bin'
            )
            destination_path = bin_path / picotool_bin.name
            _LOG.info('Copy %s -> %s', picotool_bin, destination_path)
            shutil.copy(picotool_bin, destination_path)

    def info(self, path: pathlib.Path) -> Sequence[str]:
        return (f'{self.name} installed in: {path}',)


pw_package.package_manager.register(Picotool)
