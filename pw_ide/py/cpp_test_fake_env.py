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
"""Tests for pw_ide.cpp which require fake envs"""

import os
from pathlib import Path
import sys
import tempfile
from typing import Optional
import unittest

import pw_cli.env
from pw_cli.env import pigweed_environment
import pw_ide.cpp
from pw_ide.cpp import find_cipd_installed_exe_path


class PwFindCipdInstalledExeTests(unittest.TestCase):
    """A test case for testing `find_cipd_installed_exe_path`.

    Test case hacks environment value `PW_PIGWEED_CIPD_INSTALL_DIR` and
    `PW_FAKE_PROJ_CIPD_INSTALL_DIR` to point to temporary directories. It checks
    that executable file search through these CIPD defined environment variables
    are working properly.
    """

    def setUp(self) -> None:
        self.temp_dir = tempfile.TemporaryDirectory()
        self.temp_dir_path = Path(self.temp_dir.name)

        self.envs_saves: dict[str, Optional[str]] = {
            "PW_FAKE_PROJ_CIPD_INSTALL_DIR": None,
            "PW_PIGWEED_CIPD_INSTALL_DIR": None,
        }

        for env_name in self.envs_saves:
            self.envs_saves[env_name] = os.environ.get(env_name, None)

        self.setup_test_files_and_fake_envs()

        # Environment variables are cached in each modules as global variables.
        # In order to override them, caches need to be reset.
        # pylint: disable=protected-access
        pw_cli.env._memoized_environment = None
        # pylint: enable=protected-access
        pw_ide.cpp.env = pigweed_environment()

        return super().setUp()

    def tearDown(self) -> None:
        for env_var_name, env_var in self.envs_saves.items():
            if env_var is not None:
                os.environ[env_var_name] = env_var
            else:
                os.environ.pop(env_var_name)

        self.temp_dir.cleanup()

        return super().tearDown()

    def setup_test_files_and_fake_envs(self) -> None:
        """Create necessary temporaries file and sets corresponding envs."""
        os.environ["PW_FAKE_PROJ_CIPD_INSTALL_DIR"] = str(
            self.touch_temp_exe_file(
                Path("proj_install") / "bin" / "exe_in_proj_install"
            ).parent.parent
        )

        os.environ["PW_PIGWEED_CIPD_INSTALL_DIR"] = str(
            self.touch_temp_exe_file(
                Path("pigweed_install") / "bin" / "exe_in_pigweed_install"
            ).parent.parent
        )

    def touch_temp_exe_file(self, exe_path: Path) -> Path:
        """Create a temporary executable file given a path."""
        if sys.platform.lower() in ("win32", "cygwin"):
            exe_path = Path(str(exe_path) + ".exe")

        file_path = self.temp_dir_path / exe_path

        os.makedirs(os.path.dirname(file_path), exist_ok=True)
        with open(file_path, "w") as f:
            f.write("aaa")

        return file_path

    def test_find_from_pigweed_cipd_install(self):
        env_vars = vars(pigweed_environment())

        self.assertTrue(
            str(
                find_cipd_installed_exe_path("exe_in_pigweed_install")
            ).startswith(env_vars.get("PW_PIGWEED_CIPD_INSTALL_DIR"))
        )

    def test_find_from_project_defined_cipd_install(self):
        env_vars = vars(pigweed_environment())

        self.assertTrue(
            str(find_cipd_installed_exe_path("exe_in_proj_install")).startswith(
                env_vars.get("PW_FAKE_PROJ_CIPD_INSTALL_DIR")
            )
        )


if __name__ == '__main__':
    unittest.main()
