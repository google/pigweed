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
"""Install and check status of stm32cube."""

import pathlib
from typing import Sequence

import pw_stm32cube_build.gen_file_list
import pw_package.git_repo
import pw_package.package_manager

# Compatible versions are listed on the README.md of each hal_driver repo
_STM32CUBE_VERSIONS = {
    "f0": {
        "hal_driver_tag": "v1.7.5",
        "cmsis_device_tag": "v2.3.5",
        "cmsis_core_tag": "v5.4.0_cm0",
    },
    "f1": {
        "hal_driver_tag": "v1.1.7",
        "cmsis_device_tag": "v4.3.2",
        "cmsis_core_tag": "v5.4.0_cm3",
    },
    "f2": {
        "hal_driver_tag": "v1.2.6",
        "cmsis_device_tag": "v2.2.4",
        "cmsis_core_tag": "v5.4.0_cm3",
    },
    "f3": {
        "hal_driver_tag": "v1.5.5",
        "cmsis_device_tag": "v2.3.5",
        "cmsis_core_tag": "v5.4.0_cm4",
    },
    "f4": {
        "hal_driver_tag": "v1.7.12",
        "cmsis_device_tag": "v2.6.6",
        "cmsis_core_tag": "v5.4.0_cm4",
    },
    "f7": {
        "hal_driver_tag": "v1.2.9",
        "cmsis_device_tag": "v1.2.6",
        "cmsis_core_tag": "v5.4.0_cm7",
    },
    "g0": {
        "hal_driver_tag": "v1.4.1",
        "cmsis_device_tag": "v1.4.0",
        "cmsis_core_tag": "v5.6.0_cm0",
    },
    "g4": {
        "hal_driver_tag": "v1.2.1",
        "cmsis_device_tag": "v1.2.1",
        "cmsis_core_tag": "v5.6.0_cm4",
    },
    "h7": {
        "hal_driver_tag": "v1.10.0",
        "cmsis_device_tag": "v1.10.0",
        "cmsis_core_tag": "v5.6.0",
    },
    "l0": {
        "hal_driver_tag": "v1.10.4",
        "cmsis_device_tag": "v1.9.1",
        "cmsis_core_tag": "v4.5_cm0",
    },
    "l1": {
        "hal_driver_tag": "v1.4.3",
        "cmsis_device_tag": "v2.3.1",
        "cmsis_core_tag": "v5.4.0_cm3",
    },
    "l4": {
        "hal_driver_tag": "v1.13.0",
        "cmsis_device_tag": "v1.7.1",
        "cmsis_core_tag": "v5.6.0_cm4",
    },
    "l5": {
        "hal_driver_tag": "v1.0.4",
        "cmsis_device_tag": "v1.0.4",
        "cmsis_core_tag": "v5.6.0_cm33",
    },
    "wb": {
        "hal_driver_tag": "v1.8.0",
        "cmsis_device_tag": "v1.8.0",
        "cmsis_core_tag": "v5.4.0_cm4",
    },
    "wl": {
        "hal_driver_tag": "v1.0.0",
        "cmsis_device_tag": "v1.0.0",
        "cmsis_core_tag": "v5.6.0_cm4",
    },
}


class Stm32Cube(pw_package.package_manager.Package):
    """Install and check status of stm32cube."""
    def __init__(self, family, tags, *args, **kwargs):
        super().__init__(*args, name=f'stm32cube_{family}', **kwargs)

        st_github_url = 'https://github.com/STMicroelectronics'

        self._hal_driver = pw_package.git_repo.GitRepo(
            name='hal_driver',
            url=f'{st_github_url}/stm32{family}xx_hal_driver.git',
            tag=tags['hal_driver_tag'],
        )

        self._cmsis_core = pw_package.git_repo.GitRepo(
            name='cmsis_core',
            url=f'{st_github_url}/cmsis_core.git',
            tag=tags['cmsis_core_tag'],
        )

        self._cmsis_device = pw_package.git_repo.GitRepo(
            name='cmsis_device',
            url=f'{st_github_url}/cmsis_device_{family}.git',
            tag=tags['cmsis_device_tag'],
        )

    def install(self, path: pathlib.Path) -> None:
        self._hal_driver.install(path / self._hal_driver.name)
        self._cmsis_core.install(path / self._cmsis_core.name)
        self._cmsis_device.install(path / self._cmsis_device.name)

        pw_stm32cube_build.gen_file_list.gen_file_list(path)

    def status(self, path: pathlib.Path) -> bool:
        return all([
            self._hal_driver.status(path / self._hal_driver.name),
            self._cmsis_core.status(path / self._cmsis_core.name),
            self._cmsis_device.status(path / self._cmsis_device.name),
            (path / "files.txt").is_file(),
        ])

    def info(self, path: pathlib.Path) -> Sequence[str]:
        return (
            f'{self.name} installed in: {path}',
            "Enable by running 'gn args out' and adding this line:",
            f'  dir_pw_third_party_{self.name} = "{path}"',
        )


for st_family, st_tags in _STM32CUBE_VERSIONS.items():
    pw_package.package_manager.register(Stm32Cube, st_family, st_tags)
