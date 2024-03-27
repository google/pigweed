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
import os
import shutil
import subprocess


def run_action(env=None):
    """Project action to run 'npm install'."""
    if not env:
        raise ValueError(f"Missing 'env', got %{env}")

    with env():
        npm = shutil.which('npm.cmd' if os.name == 'nt' else 'npm')

        repo_root = os.environ.get('PW_PROJECT_ROOT') or os.environ.get(
            'PW_ROOT'
        )

        # TODO: b/323378974 - Better integrate NPM actions with pw_env_setup so
        # we don't have to manually set `npm_config_cache` every time we run
        # npm.
        # Force npm cache to live inside the environment directory.
        npm_env = os.environ.copy()
        npm_env['npm_config_cache'] = os.path.join(
            npm_env['_PW_ACTUAL_ENVIRONMENT_ROOT'], 'npm-cache'
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
            stderr=subprocess.STDOUT,
            stdin=subprocess.DEVNULL,
            cwd=repo_root,
            env=npm_env,
        )

        subprocess.run(
            [
                npm,
                "run",
                "log-viewer-setup",
                "--quiet",
                "--no-progress",
                "--loglevel=error",
            ],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            stdin=subprocess.DEVNULL,
            cwd=repo_root,
            env=npm_env,
        )
