#!/usr/bin/env python
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
"""Tests for pw_zephyr's manifest subcommand"""
import pathlib

import unittest
import os
import shutil
import subprocess
import tempfile
from pw_env_setup_zephyr import zephyr


class ZephyrManifestTest(unittest.TestCase):
    """Tests the manifest subcommand for Zephyr"""

    def setUp(self) -> None:
        super().setUp()
        self._pw_dir = tempfile.mkdtemp()
        self._pw_path = pathlib.Path(self._pw_dir)
        self._zephyr_path = (
            self._pw_path / 'environment' / 'packages' / 'zephyr'
        )
        self.init_git_dir(self._pw_path, 'pigweed')

        os.environ['PW_ROOT'] = str(self._pw_dir)

    def tearDown(self) -> None:
        super().tearDown()
        shutil.rmtree(self._pw_dir)

    def _init_zephyr(self) -> None:
        self._zephyr_path.mkdir(parents=True)
        self.init_git_dir(self._zephyr_path, 'zephyr')

    @staticmethod
    def git_get_revision(path: pathlib.Path) -> str:
        result = subprocess.run(
            ['git', 'log', '--pretty=format:"%H"', '-n', '1'],
            stdout=subprocess.PIPE,
            cwd=path,
        )
        assert result.returncode == 0
        return result.stdout.decode('utf-8').strip().strip('"')

    @staticmethod
    def init_git_dir(path: pathlib.Path, name: str) -> None:
        subprocess.check_call(['git', 'init'], cwd=path)
        subprocess.check_call(
            ['git', 'remote', 'add', 'origin', f'http://fake.path/{name}'],
            cwd=path,
        )
        subprocess.check_call(
            ['git', 'commit', '--allow-empty', '-m', '"Test commit"'], cwd=path
        )

    def test_pigweed_only_manifest(self) -> None:
        yaml = zephyr.generate_manifest()
        assert yaml.get('manifest') is not None
        assert yaml['manifest'].get('remotes') is not None
        remotes: list = yaml['manifest']['remotes']
        assert len(remotes) == 1
        assert remotes[0].get('name') == 'pigweed'
        assert remotes[0].get('url-base') == 'http://fake.path/pigweed'
        assert yaml['manifest'].get('projects') is not None
        projects: list = yaml['manifest']['projects']
        assert len(projects) == 1
        assert projects[0].get('name') == 'pigweed'
        assert projects[0].get('remote') == 'pigweed'
        assert projects[0].get('revision') == self.git_get_revision(
            self._pw_path
        )
        assert projects[0].get('path') == str(
            self._pw_path.relative_to(
                os.path.commonpath([self._pw_path, os.getcwd()])
            )
        )
        assert projects[0].get('import') is False

    def test_pigweed_and_zephyr_manifest(self) -> None:
        yaml = zephyr.generate_manifest()
        assert yaml.get('manifest') is not None
        assert yaml['manifest'].get('remotes') is not None
        remotes: list = yaml['manifest']['remotes']
        assert len(remotes) == 2
        zephyr_remote = [it for it in remotes if it.get('name') == 'zephyr'][0]
        assert zephyr_remote.get('url-base') == 'http://fake.path/zephyr'
        assert yaml['manifest'].get('projects') is not None
        projects: list = yaml['manifest']['projects']
        zephyr_project = [it for it in projects if it.get('name') == 'zephyr'][
            0
        ]
        assert zephyr_project.get('remote') == 'zephyr'
        assert zephyr_project.get('revision') == self.git_get_revision(
            self._zephyr_path
        )
        assert zephyr_project.get('path') == str(
            self._zephyr_path.relative_to(
                os.path.commonpath([self._zephyr_path, os.getcwd()])
            )
        )
        assert zephyr_project.get('import') is True
