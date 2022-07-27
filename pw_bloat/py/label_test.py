#!/usr/bin/env python3
# Copyright 2022 The Pigweed Authors
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
"""Tests for bloaty configuration tooling."""

import unittest
import os

from pw_bloat.label import from_bloaty_csv, DataSourceMap


def get_test_map():
    pw_root = os.environ.get("PW ROOT")
    filename = f"{pw_root}/pigweed/pw_bloat/test_label.csv"
    with open(filename, 'r') as csvfile:
        ds_map = from_bloaty_csv(csvfile)
        capacity_patterns = [("^__TEXT$", 459), ("^_", 920834)]
        for cap_pattern, cap_size in capacity_patterns:
            ds_map.add_capacity(cap_pattern, cap_size)
    return ds_map


class LabelStructTest(unittest.TestCase):
    """Testing class for the label structs."""
    def test_data_source_total_size(self):
        ds_map = DataSourceMap(["a", "b", "c"])
        self.assertEqual(ds_map.get_total_size(), 0)

    def test_data_source_single_insert_total_size(self):
        ds_map = DataSourceMap(["a", "b", "c"])
        ds_map.insert_label_hierachy(["FLASH", ".code", "main()"], 30)
        self.assertEqual(ds_map.get_total_size(), 30)

    def test_data_source_multiple_insert_total_size(self):
        ds_map = DataSourceMap(["a", "b", "c"])
        ds_map.insert_label_hierachy(["FLASH", ".code", "main()"], 30)
        ds_map.insert_label_hierachy(["RAM", ".code", "foo()"], 100)
        self.assertEqual(ds_map.get_total_size(), 130)


if __name__ == '__main__':
    unittest.main()
