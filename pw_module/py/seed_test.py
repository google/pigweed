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
"""Tests for pw_module.seed."""

from pathlib import Path
import tempfile
import unittest

from pw_module import seed


_SAMPLE_REGISTRY_FILE = '''
import("//build_overrides/pigweed.gni")

import("seed.gni")

pw_seed("0001") {
  sources = [ "0001-the-seed-process.rst" ]
  inputs = [ "0001-the-seed-process/seed-index-gerrit.png" ]
  title = "The SEED Process"
  status = "Meta"
  author = "The Pigweed Authors"
  facilitator = "N/A"
}

pw_seed("0002") {
  sources = [ "0002-template.rst" ]
  title = "SEED Template"
  status = "Meta"
  author = "The Pigweed Authors"
  facilitator = "N/A"
}

pw_seed_index("seeds") {
  index_file = "0000-index.rst"
  seeds = [
    ":0001",
    ":0002",
  ]
}
'''


_SAMPLE_REGISTRY_FILE_WITH_ADDED_SEED = '''
import("//build_overrides/pigweed.gni")

import("seed.gni")

pw_seed("0001") {
  sources = [ "0001-the-seed-process.rst" ]
  inputs = [ "0001-the-seed-process/seed-index-gerrit.png" ]
  title = "The SEED Process"
  status = "Meta"
  author = "The Pigweed Authors"
  facilitator = "N/A"
}

pw_seed("0002") {
  sources = [ "0002-template.rst" ]
  title = "SEED Template"
  status = "Meta"
  author = "The Pigweed Authors"
  facilitator = "N/A"
}

pw_seed("0200") {
  title = "a title"
  author = "an author"
  status = "Draft"
  changelist = 111111
}

pw_seed_index("seeds") {
  index_file = "0000-index.rst"
  seeds = [
    ":0001",
    ":0002",
    ":0200",
  ]
}
'''


class TestSeedMetadata(unittest.TestCase):
    """Tests for SeedMetadata."""

    def test_default_filename_basic(self):
        meta = seed.SeedMetadata(
            number=789,
            title='Simple Title 2',
            authors='',
            status=seed.SeedStatus.DRAFT,
        )
        self.assertEqual(meta.default_filename(), '0789-simple-title-2.rst')

    def test_default_filename_special_characters(self):
        meta = seed.SeedMetadata(
            number=9876,
            title="pw_some_module: Pigweed's newest module",
            authors='',
            status=seed.SeedStatus.DRAFT,
        )
        self.assertEqual(
            meta.default_filename(),
            '9876-pw_some_module-pigweeds-newest-module.rst',
        )


class TestSeedRegistry(unittest.TestCase):
    """Tests for SEED registry modifications."""

    def setUp(self):
        self._dir = tempfile.TemporaryDirectory()
        root = Path(self._dir.name)
        self._build_file = root / 'seed' / 'BUILD.gn'
        self._build_file.parent.mkdir()
        self._build_file.write_text(_SAMPLE_REGISTRY_FILE)
        self._registry = seed.SeedRegistry.parse(self._build_file)

    def tearDown(self):
        self._dir.cleanup()

    def test_basic_parsing(self):
        self.assertEqual(self._registry.seed_count(), 2)

    def test_insert_seed(self):
        meta = seed.SeedMetadata(
            number=200,
            title='a title',
            authors='an author',
            status=seed.SeedStatus.DRAFT,
            changelist=111111,
        )
        self._registry.insert(meta)
        self._registry.write()

        self.assertEqual(
            self._build_file.read_text(),
            _SAMPLE_REGISTRY_FILE_WITH_ADDED_SEED,
        )


if __name__ == '__main__':
    unittest.main()
