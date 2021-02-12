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
"""Tests for pw_watch.minimal_watch_directories."""

import unittest
import os
import tempfile
from pathlib import Path

import pw_watch.watch as watch


def make_tree(root, directories):
    for directory in directories:
        os.mkdir(Path(root, directory))


class TestMinimalWatchDirectories(unittest.TestCase):
    """Tests for pw_watch.watch.minimal_watch_directories."""
    def test_empty_directory(self):
        subdirectories_to_watch = []
        with tempfile.TemporaryDirectory() as tmpdir:
            ans_subdirectories_to_watch = [(Path(tmpdir), False)]
            subdirectories_to_watch = \
                watch.minimal_watch_directories(tmpdir, 'f1')

        self.assertEqual(set(subdirectories_to_watch),
                         set(ans_subdirectories_to_watch))

    def test_non_exist_directories_to_exclude(self):
        subdirectories_to_watch = []
        exclude_list = ['f3']
        with tempfile.TemporaryDirectory() as tmpdir:
            make_tree(tmpdir, ['f1', 'f2'])
            ans_subdirectories_to_watch = [
                (Path(tmpdir, 'f1'), True),
                (Path(tmpdir, 'f2'), True),
                (Path(tmpdir), False),
            ]
            subdirectories_to_watch = \
                watch.minimal_watch_directories(tmpdir, exclude_list)

        self.assertEqual(set(subdirectories_to_watch),
                         set(ans_subdirectories_to_watch))

    def test_one_layer_directories(self):
        subdirectories_to_watch = []
        exclude_list = ['f1']
        with tempfile.TemporaryDirectory() as tmpdir:
            make_tree(tmpdir, [
                'f1',
                'f1/f1',
                'f1/f2',
                'f2',
                'f2/f1',
            ])
            ans_subdirectories_to_watch = [
                (Path(tmpdir, 'f2'), True),
                (Path(tmpdir), False),
            ]
            subdirectories_to_watch = \
                watch.minimal_watch_directories(tmpdir, exclude_list)

        self.assertEqual(set(subdirectories_to_watch),
                         set(ans_subdirectories_to_watch))

    def test_two_layers_direcories(self):
        subdirectories_to_watch = []
        exclude_list = ['f1/f2']
        with tempfile.TemporaryDirectory() as tmpdir:
            make_tree(tmpdir, [
                'f1',
                'f1/f1',
                'f1/f2',
                'f2',
                'f2/f1',
            ])
            ans_subdirectories_to_watch = [
                (Path(tmpdir, 'f2'), True),
                (Path(tmpdir, 'f1/f1'), True),
                (Path(tmpdir), False),
                (Path(tmpdir, 'f1'), False),
            ]
            subdirectories_to_watch = \
                watch.minimal_watch_directories(tmpdir, exclude_list)

        self.assertEqual(set(subdirectories_to_watch),
                         set(ans_subdirectories_to_watch))

    def test_empty_exclude_list(self):
        subdirectories_to_watch = []
        exclude_list = []
        with tempfile.TemporaryDirectory() as tmpdir:
            make_tree(tmpdir, [
                'f1',
                'f1/f1',
                'f1/f2',
                'f2',
                'f2/f1',
            ])
            ans_subdirectories_to_watch = [
                (Path(tmpdir, 'f2'), True),
                (Path(tmpdir, 'f1'), True),
                (Path(tmpdir), False),
            ]
            subdirectories_to_watch = \
                watch.minimal_watch_directories(tmpdir, exclude_list)

        self.assertEqual(set(subdirectories_to_watch),
                         set(ans_subdirectories_to_watch))

    def test_multiple_directories_in_exclude_list(self):
        """test case for multiple directories to exclude"""
        subdirectories_to_watch = []
        exclude_list = [
            'f1/f2',
            'f3/f1',
            'f3/f3',
        ]
        with tempfile.TemporaryDirectory() as tmpdir:
            make_tree(tmpdir, [
                'f1', 'f1/f1', 'f1/f2', 'f2', 'f2/f1', 'f3', 'f3/f1', 'f3/f2',
                'f3/f3'
            ])
            ans_subdirectories_to_watch = [
                (Path(tmpdir, 'f2'), True),
                (Path(tmpdir, 'f1/f1'), True),
                (Path(tmpdir, 'f3/f2'), True),
                (Path(tmpdir), False),
                (Path(tmpdir, 'f1'), False),
                (Path(tmpdir, 'f3'), False),
            ]
            subdirectories_to_watch = \
                watch.minimal_watch_directories(tmpdir, exclude_list)

        self.assertEqual(set(subdirectories_to_watch),
                         set(ans_subdirectories_to_watch))

    def test_nested_sibling_exclusion(self):
        subdirectories_to_watch = []
        exclude_list = [
            'f1/f1/f1/f1/f1',
            'f1/f1/f1/f2',
        ]
        with tempfile.TemporaryDirectory() as tmpdir:
            make_tree(tmpdir, [
                'f1',
                'f1/f1',
                'f1/f1/f1',
                'f1/f1/f1/f1',
                'f1/f1/f1/f1/f1',
                'f1/f1/f1/f1/f2',
                'f1/f1/f1/f1/f3',
                'f1/f1/f1/f2',
            ])
            ans_subdirectories_to_watch = [
                (Path(tmpdir, 'f1/f1/f1/f1/f2'), True),
                (Path(tmpdir, 'f1/f1/f1/f1/f3'), True),
                (Path(tmpdir), False),
                (Path(tmpdir, 'f1'), False),
                (Path(tmpdir, 'f1/f1'), False),
                (Path(tmpdir, 'f1/f1/f1'), False),
                (Path(tmpdir, 'f1/f1/f1/f1'), False),
            ]
            subdirectories_to_watch = \
                watch.minimal_watch_directories(tmpdir, exclude_list)

        self.assertEqual(set(subdirectories_to_watch),
                         set(ans_subdirectories_to_watch))


if __name__ == '__main__':
    unittest.main()
