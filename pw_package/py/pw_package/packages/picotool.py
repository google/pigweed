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

import logging
import os
from pathlib import Path
import shutil
import subprocess
from typing import Sequence

import pw_package.git_repo
import pw_package.package_manager

_LOG = logging.getLogger(__package__)


def force_copy(source: Path, destination: Path):
    _LOG.info('Copy %s -> %s', source, destination)
    # ensure the destination directory exists
    # otherwise the copy will fail on mac.
    dirname = os.path.dirname(destination)
    if not os.path.isdir(dirname):
        os.makedirs(dirname)
    destination.unlink(missing_ok=True)
    shutil.copy(source, destination)


class Picotool(pw_package.package_manager.Package):
    """Install and check status of picotool."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, name='picotool', **kwargs)

    def install(self, path: Path) -> None:
        env = os.environ.copy()

        def log_and_run(command: Sequence[str], **kwargs):
            _LOG.info('==> %s', ' '.join(command))
            return subprocess.run(
                command,
                env=env,
                check=True,
                **kwargs,
            )

        log_and_run(('bazel', 'build', '@picotool'))
        build_path = log_and_run(
            ('bazel', 'cquery', '@picotool', '--output=files'),
            capture_output=True,
            text=True,
        ).stdout.strip()

        picotool_bin = path / 'out' / 'picotool'
        force_copy(build_path, picotool_bin)

        _LOG.info('Done! picotool binary located at:')
        _LOG.info(picotool_bin)

        bootstrap_env_path = Path(env.get('_PW_ACTUAL_ENVIRONMENT_ROOT', ''))
        if bootstrap_env_path.is_dir() and picotool_bin.is_file():
            bin_path = (
                bootstrap_env_path / 'cipd' / 'packages' / 'pigweed' / 'bin'
            )
            destination_path = bin_path / picotool_bin.name
            force_copy(build_path, destination_path)

    def info(self, path: Path) -> Sequence[str]:
        return (f'{self.name} installed in: {path}',)


pw_package.package_manager.register(Picotool)
