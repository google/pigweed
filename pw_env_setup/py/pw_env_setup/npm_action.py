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
"""NPM pw_env_setup action plugin.

This action triggers `npm install` after CIPD setup.
"""
import subprocess
import os
import shutil


def run_action(env=None):
    """Project action to run 'npm install'."""
    if not env:
        raise ValueError(f"Missing 'env', got %{env}")

    with env():
        npm = shutil.which('npm')
        repo_root = os.environ.get('PW_PROJECT_ROOT') or os.environ.get(
            'PW_ROOT'
        )
        subprocess.run(
            [
                npm,
                "install",
                "--quiet",
                "--no-progress",
                "--loglevel=error",
            ],
            stdout=subprocess.PIPE,
            cwd=repo_root,
        )
