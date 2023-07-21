#!/usr/bin/env python3
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
"""Tests that pw_test_group outputs expected metadata."""

import argparse
import json
import pathlib
import sys
import unittest

TEST_TARGET_NAME = 'metadata_only_test'
EXTRA_METADATA_ENTRY = {'extra_key': 'extra_value'}


class TestGroupMetadataTest(unittest.TestCase):
    """Tests that pw_test_group outputs expected metadata."""

    metadata_path: pathlib.Path

    def test_metadata_exists(self) -> None:
        """Asserts that the metadata file has been created."""
        self.assertTrue(self.metadata_path.exists())

    def test_metadata_has_test_entry(self) -> None:
        """Asserts that the expected test entry is present."""
        meta_text = self.metadata_path.read_text()
        try:
            meta = json.loads(meta_text)
        except json.decoder.JSONDecodeError as jde:
            raise ValueError(
                f'Failed to decode file {self.metadata_path} '
                f'as JSON: {meta_text}'
            ) from jde
        self.assertIsInstance(meta, list)
        found = False
        for test_entry in meta:
            self.assertIn('test_type', test_entry)
            if test_entry['test_type'] != 'unit_test':
                continue
            self.assertIn('test_name', test_entry)
            self.assertIn('test_directory', test_entry)
            if test_entry['test_name'] == TEST_TARGET_NAME:
                found = True
                self.assertIn('extra_metadata', test_entry)
                self.assertEqual(
                    test_entry['extra_metadata'], EXTRA_METADATA_ENTRY
                )
        self.assertTrue(found)


def main() -> None:
    """Tests that pw_test_group outputs expected metadata."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        '--stamp-path',
        type=pathlib.Path,
        required=True,
        help='Path to the stamp file output of pw_test_group',
    )
    parser.add_argument(
        'unittest_args',
        nargs=argparse.REMAINDER,
        help='Arguments after "--" are passed to unittest.',
    )
    args = parser.parse_args()
    # Use the stamp file location to find the location of the metadata file.
    # Unfortunately, there's no reliable toolchain-agnostic way to grab the path
    # to the metadata file itself within GN.
    TestGroupMetadataTest.metadata_path = (
        args.stamp_path.parent / 'metadata_only_group.testinfo.json'
    )
    unittest_args = sys.argv[:1] + args.unittest_args[1:]
    unittest.main(argv=unittest_args)


if __name__ == '__main__':
    main()
