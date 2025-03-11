# Copyright 2025 The Pigweed Authors
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
"""Tests for finding config files."""

import os
import unittest
from pathlib import Path

from pw_config_loader import find_config
from pyfakefs import fake_filesystem_unittest


class TestFindConfig(fake_filesystem_unittest.TestCase):
    """Tests for config file loading helpers."""

    def setUp(self):
        self.setUpPyfakefs()

        fake_fs_root = Path('/home/gregory')
        if os.name == 'nt':
            fake_fs_root = fake_fs_root.resolve()
        self.fs.create_dir(fake_fs_root)
        os.chdir(fake_fs_root)

    def test_configs_in_parents_basic(self):
        """Tests the basic functionality of configs_in_parents."""
        self.fs.create_file('foo.conf')
        self.fs.create_file('pigweed/foo.conf')
        self.fs.create_file('pigweed/pw_cli/baz.txt')

        configs = find_config.configs_in_parents(
            'foo.conf', Path('pigweed/pw_cli/baz.txt')
        )

        self.assertEqual(
            list(configs),
            [
                Path('pigweed/foo.conf').resolve(),
                Path('foo.conf').resolve(),
            ],
        )

    def test_configs_in_parents_no_configs(self):
        """Tests configs_in_parents when no config files exist."""
        self.fs.create_file('pigweed/pw_cli/baz.txt')

        configs = find_config.configs_in_parents(
            'foo.conf', Path('pigweed/pw_cli/baz.txt')
        )

        self.assertEqual(list(configs), [])

    def test_configs_in_parents_path_is_root(self):
        """Tests configs_in_parents when the path is the root directory."""
        self.fs.create_file('/foo.conf')

        configs = find_config.configs_in_parents('foo.conf', Path('/'))

        self.assertEqual(list(configs), [Path('/foo.conf').resolve()])

    def test_paths_by_nearest_config_basic(self):
        """Tests the basic functionality of paths_by_nearest_config."""
        self.fs.create_file('foo.conf')
        self.fs.create_file('pigweed/foo.conf')
        self.fs.create_file('pigweed/pw_cli/baz.txt')

        paths_by_config = find_config.paths_by_nearest_config(
            'foo.conf',
            [
                Path('pigweed/pw_cli/baz.txt'),
                Path('pigweed/foo.conf'),
                Path('foo.conf'),
            ],
        )

        self.assertEqual(
            dict(paths_by_config),
            {
                Path('pigweed/foo.conf').resolve(): [
                    Path('pigweed/pw_cli/baz.txt'),
                    Path('pigweed/foo.conf'),
                ],
                Path('foo.conf').resolve(): [
                    Path('foo.conf'),
                ],
            },
        )

    def test_paths_by_nearest_config_no_configs(self):
        """Tests paths_by_nearest_config when no config files exist."""
        self.fs.create_file('pigweed/pw_cli/baz.txt')

        paths_by_config = find_config.paths_by_nearest_config(
            'foo.conf', [Path('pigweed/pw_cli/baz.txt')]
        )

        self.assertEqual(
            dict(paths_by_config),
            {
                None: [Path('pigweed/pw_cli/baz.txt')],
            },
        )

    def test_paths_by_nearest_config_empty_paths(self):
        """Tests paths_by_nearest_config when the paths list is empty."""
        paths_by_config = find_config.paths_by_nearest_config(
            'foo.conf',
            [],
        )

        self.assertEqual(dict(paths_by_config), {})


if __name__ == '__main__':
    unittest.main()
