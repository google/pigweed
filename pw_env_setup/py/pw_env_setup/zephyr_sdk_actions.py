# Copyright 2024 The Pigweed Authors
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
"""Zephyr SDK pw_env_setup action plugin.

This action triggers `cmake -P <cache_path>/cmake/zephyr_sdk_export.cmake` after
CIPD setup.
"""
import os
import pathlib
import shutil
import subprocess


def run_action(env=None):
    """Project action to install the Zephyr SDK."""
    if not env:
        raise ValueError(f"Missing 'env', got %{env}")

    with env():
        cmake = shutil.which('cmake')

        zephyr_sdk = list(
            pathlib.Path(os.environ['PW_CIPD_CIPD_INSTALL_DIR']).glob(
                '**/cmake/zephyr_sdk_export.cmake'
            )
        )[0]
        subprocess.check_call(
            [cmake, '-P', str(zephyr_sdk)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
