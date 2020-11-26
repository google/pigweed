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
"""Install and check status of teensy-core."""

import logging
import re
from pathlib import Path
from typing import Sequence

from pw_arduino_build import core_installer

import pw_package.package_manager

_LOG: logging.Logger = logging.getLogger(__name__)


class ArduinoCore(pw_package.package_manager.Package):
    """Install and check status of arduino cores."""
    def __init__(self, core_name, *args, **kwargs):
        super().__init__(*args, name=core_name, **kwargs)

    def status(self, path: Path) -> bool:
        return (path / 'hardware').is_dir()

    def install(self, path: Path) -> None:
        if self.status(path):
            return
        # Otherwise delete current version and reinstall
        core_installer.install_core(path.parent.resolve().as_posix(),
                                    path.name)

    def info(self, path: Path) -> Sequence[str]:
        packages_root = path.parent.resolve()
        arduino_package_path = path
        arduino_package_name = None

        message = [
            f'{self.name} currently installed in: {path}',
        ]
        # Make gn args sample copy/paste-able by omitting the starting timestamp
        # and INF log on each line.
        message_gn_args = [
            'Enable by running "gn args out" and adding these lines:',
            f'  pw_arduino_build_CORE_PATH = "{packages_root}"',
            f'  pw_arduino_build_CORE_NAME = "{self.name}"'
        ]

        # Search for first valid 'package/version' directory
        for hardware_dir in [
                path for path in (path / 'hardware').iterdir()
                if path.is_dir()
        ]:
            if path.name in ["arduino", "tools"]:
                continue
            for subdir in [
                    path for path in hardware_dir.iterdir() if path.is_dir()
            ]:
                if subdir.name == 'avr' or re.match(r'[0-9.]+', subdir.name):
                    arduino_package_name = f'{hardware_dir.name}/{subdir.name}'
                    break

        if arduino_package_name:
            message_gn_args += [
                f'  pw_arduino_build_PACKAGE_NAME = "{arduino_package_name}"',
                '  pw_arduino_build_BOARD = "BOARD_NAME"'
            ]
            message += ["\n".join(message_gn_args)]
            message += [
                'Where BOARD_NAME is any supported board.',
                # Have arduino_builder command appear on it's own line.
                'List available boards by running:\n'
                '  arduino_builder '
                f'--arduino-package-path {arduino_package_path} '
                f'--arduino-package-name {arduino_package_name} list-boards'
            ]
        return message


for arduino_core_name in core_installer.supported_cores():
    pw_package.package_manager.register(ArduinoCore, name=arduino_core_name)
