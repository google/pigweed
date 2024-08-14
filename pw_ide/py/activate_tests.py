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
"""Tests for pw_ide.activate"""

import unittest
from pw_ide.activate import (
    find_pigweed_json_above,
    find_pigweed_json_below,
    pigweed_root,
)

from test_cases import TempDirTestCase


class TestFindPigweedJson(TempDirTestCase):
    """Test functions that find pigweed.json"""

    def test_find_pigweed_json_above_1_level(self):
        self.touch_temp_file("pigweed.json")
        nested_dir = self.temp_dir_path / "nested"
        nested_dir.mkdir()
        presumed_root = find_pigweed_json_above(nested_dir)
        self.assertEqual(presumed_root, self.temp_dir_path)

    def test_find_pigweed_json_above_2_levels(self):
        self.touch_temp_file("pigweed.json")
        nested_dir = self.temp_dir_path / "nested" / "again"
        nested_dir.mkdir(parents=True)
        presumed_root = find_pigweed_json_above(nested_dir)
        self.assertEqual(presumed_root, self.temp_dir_path)

    def test_find_pigweed_json_below_1_level(self):
        nested_dir = self.temp_dir_path / "nested"
        nested_dir.mkdir()
        self.touch_temp_file(nested_dir / "pigweed.json")
        presumed_root = find_pigweed_json_below(self.temp_dir_path)
        self.assertEqual(presumed_root, nested_dir)

    def test_find_pigweed_json_below_2_level(self):
        nested_dir = self.temp_dir_path / "nested" / "again"
        nested_dir.mkdir(parents=True)
        self.touch_temp_file(nested_dir / "pigweed.json")
        presumed_root = find_pigweed_json_below(self.temp_dir_path)
        self.assertEqual(presumed_root, nested_dir)

    def test_pigweed_json_above_is_preferred(self):
        self.touch_temp_file("pigweed.json")

        workspace_dir = self.temp_dir_path / "workspace"
        workspace_dir.mkdir()

        pigweed_dir = workspace_dir / "pigweed"
        pigweed_dir.mkdir()
        self.touch_temp_file(pigweed_dir / "pigweed.json")

        presumed_root, _ = pigweed_root(self.temp_dir_path, False)
        self.assertEqual(presumed_root, self.temp_dir_path)


if __name__ == '__main__':
    unittest.main()
