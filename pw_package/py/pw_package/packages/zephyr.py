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
"""Install and check status of Zephyr."""
import importlib.resources
import pathlib
import subprocess
import sys
from typing import Sequence

import pw_env_setup.virtualenv_setup

import pw_package.git_repo
import pw_package.package_manager


class Zephyr(pw_package.git_repo.GitRepo):
    """Install and check status of Zephyr."""

    def __init__(self, *args, **kwargs):
        super().__init__(
            *args,
            name='zephyr',
            url='https://github.com/zephyrproject-rtos/zephyr',
            commit='356c8cbe63ae01b3ab438382639d25bb418a0213',  # v3.4 release
            **kwargs,
        )

    def info(self, path: pathlib.Path) -> Sequence[str]:
        return (
            f'{self.name} installed in: {path}',
            "Enable by running 'gn args out' and adding this line:",
            f'  dir_pw_third_party_zephyr = "{path}"',
        )

    def install(self, path: pathlib.Path) -> None:
        super().install(path)

        with importlib.resources.path(
            pw_env_setup.virtualenv_setup, 'constraint.list'
        ) as constraint:
            subprocess.check_call(
                [
                    sys.executable,
                    '-m',
                    'pip',
                    'install',
                    '-r',
                    f'{path}/scripts/requirements.txt',
                    '-c',
                    str(constraint),
                ]
            )


pw_package.package_manager.register(Zephyr)
