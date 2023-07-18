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
import json
import pathlib
import subprocess
import sys
import tempfile

from typing import Sequence

import pw_env_setup.virtualenv_setup

import pw_package.git_repo
import pw_package.package_manager


class Zephyr(pw_package.git_repo.GitRepo):
    """Install and check status of Zephyr."""

    def __init__(self, *args, **kwargs):
        super().__init__(
            *args,
            name="zephyr",
            url="https://github.com/zephyrproject-rtos/zephyr",
            commit="356c8cbe63ae01b3ab438382639d25bb418a0213",  # v3.4 release
            **kwargs,
        )

    def info(self, path: pathlib.Path) -> Sequence[str]:
        return (
            f"{self.name} installed in: {path}",
            "Enable by running 'gn args out' and adding this line:",
            f'  dir_pw_third_party_zephyr = "{path}"',
        )

    @staticmethod
    def __populate_download_cache_from_cipd(path: pathlib.Path) -> None:
        """Check for Zephyr SDK in cipd"""
        package_path = path.parent.resolve()
        core_cache_path = package_path / "zephyr_sdk"
        core_cache_path.mkdir(parents=True, exist_ok=True)

        cipd_package_subpath = "infra/3pp/tools/zephyr_sdk/${platform}"

        # Check if a teensy cipd package is readable
        with tempfile.NamedTemporaryFile(
            prefix="cipd", delete=True
        ) as temp_json:
            cipd_acl_check_command = [
                "cipd",
                "acl-check",
                cipd_package_subpath,
                "-reader",
                "-json-output",
                temp_json.name,
            ]
            subprocess.run(cipd_acl_check_command, capture_output=True)

            # Return if no packages are readable.
            if not json.load(temp_json)["result"]:
                raise RuntimeError("Failed to verify cipd is readable")

        # Initialize cipd
        subprocess.check_call(
            [
                "cipd",
                "init",
                "-force",
                core_cache_path.as_posix(),
            ]
        )
        # Install the Zephyr SDK
        subprocess.check_call(
            [
                "cipd",
                "install",
                cipd_package_subpath,
                "-root",
                core_cache_path.as_posix(),
                "-force",
            ]
        )
        # Setup Zephyr SDK
        subprocess.check_call(
            [
                core_cache_path.as_posix() + "/setup.sh",
                "-t all",
                "-c",
                "-h",
            ]
        )

    def install(self, path: pathlib.Path) -> None:
        super().install(path)

        self.__populate_download_cache_from_cipd(path)
        with importlib.resources.path(
            pw_env_setup.virtualenv_setup, "constraint.list"
        ) as constraint:
            subprocess.check_call(
                [
                    sys.executable,
                    "-m",
                    "pip",
                    "install",
                    "-r",
                    f"{path}/scripts/requirements.txt",
                    "-c",
                    str(constraint),
                ]
            )


pw_package.package_manager.register(Zephyr)
