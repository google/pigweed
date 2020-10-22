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
"""Install and check status of Git repository-based packages."""

import os
import pathlib
import shutil
import subprocess
from typing import Union

import pw_package.package_manager

PathOrStr = Union[pathlib.Path, str]


def git_stdout(*args: PathOrStr,
               show_stderr=False,
               repo: PathOrStr = '.') -> str:
    return subprocess.run(['git', '-C', repo, *args],
                          stdout=subprocess.PIPE,
                          stderr=None if show_stderr else subprocess.DEVNULL,
                          check=True).stdout.decode().strip()


def git(*args: PathOrStr,
        repo: PathOrStr = '.') -> subprocess.CompletedProcess:
    return subprocess.run(['git', '-C', repo, *args], check=True)


class GitRepo(pw_package.package_manager.Package):
    """Install and check status of Git repository-based packages."""
    def __init__(self, url, commit, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self._url = url
        self._commit = commit

    def status(self, path: pathlib.Path) -> bool:
        if not os.path.isdir(path / '.git'):
            return False

        remote = git_stdout('remote', 'get-url', 'origin', repo=path)
        commit = git_stdout('rev-parse', 'HEAD', repo=path)
        status = git_stdout('status', '--porcelain=v1', repo=path)
        return remote == self._url and commit == self._commit and not status

    def install(self, path: pathlib.Path) -> None:
        # If already installed and at correct version exit now.
        if self.status(path):
            return

        # Otherwise delete current version and clone again.
        if os.path.isdir(path):
            shutil.rmtree(path)

        # --filter=blob:none means we don't get history, just the current
        # revision. If we later run commands that need history it will be
        # retrieved on-demand. For small repositories the effect is negligible
        # but for large repositories this should be a significant improvement.
        git('clone', '--filter=blob:none', self._url, path)
        git('reset', '--hard', self._commit, repo=path)
