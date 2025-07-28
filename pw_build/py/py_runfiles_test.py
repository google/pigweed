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
"""Tests for pw_py_importable_runfile."""

from pathlib import Path
import unittest

import pw_build.test_runfile
import pw_build.another_test_runfile
import pw_build_external_runfile_resource.black
from python.runfiles import runfiles  # type: ignore


class TestRunfiles(unittest.TestCase):
    """Tests for pw_py_importable_runfile."""

    def setUp(self):
        self.r = runfiles.Create()

    def test_expected_runfiles(self):
        runfile = Path(self.r.Rlocation(*pw_build.test_runfile.RLOCATION))
        self.assertTrue(runfile.is_file())
        self.assertEqual(runfile.read_text(), "OK\n")

    def test_compare_runfiles(self):
        runfile = Path(self.r.Rlocation(*pw_build.test_runfile.RLOCATION))
        rules_python_runfile = Path(
            self.r.Rlocation(
                "pigweed/pw_build/test_data/test_runfile.txt",
                self.r.CurrentRepository(),
            )
        )
        self.assertTrue(rules_python_runfile.is_file())
        self.assertEqual(
            runfile.read_text(),
            rules_python_runfile.read_text(),
        )

    def test_expected_remapped_runfiles(self):
        runfile = Path(
            self.r.Rlocation(*pw_build.another_test_runfile.RLOCATION)
        )
        self.assertTrue(runfile.is_file())
        self.assertEqual(runfile.read_text(), "OK\n")

    def test_compare_remapped_runfiles(self):
        runfile = Path(
            self.r.Rlocation(*pw_build.another_test_runfile.RLOCATION)
        )
        rules_python_runfile = Path(
            self.r.Rlocation(
                "pigweed/pw_build/test_data/test_runfile.txt",
                self.r.CurrentRepository(),
            )
        )
        self.assertTrue(rules_python_runfile.is_file())
        self.assertEqual(
            runfile.read_text(),
            rules_python_runfile.read_text(),
        )

    def test_external_py_resource(self):
        runfile = Path(
            self.r.Rlocation(
                *pw_build_external_runfile_resource.black.RLOCATION
            )
        )
        self.assertTrue(runfile.is_file())


if __name__ == '__main__':
    unittest.main()
