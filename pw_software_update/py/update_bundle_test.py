# Copyright 2021 The Pigweed Authors
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
"""Unit tests for pw_software_update/update_bundle.py."""

from pathlib import Path
import tempfile
import unittest

from pw_software_update import update_bundle


class GenUnsignedUpdateBundleTest(unittest.TestCase):
    """Test the generation of unsigned update bundles."""
    def test_bundle_generation(self):
        """Tests basic creation of an UpdateBundle from temp dir."""
        foo_bytes = b'\xf0\x0b\xa4'
        bar_bytes = b'\x0b\xa4\x99'
        baz_bytes = b'\xba\x59\x06'
        qux_bytes = b'\x8a\xf3\x12'
        with tempfile.TemporaryDirectory() as tempdir_name:
            temp_root = Path(tempdir_name)
            (temp_root / 'foo.bin').write_bytes(foo_bytes)
            (temp_root / 'bar.bin').write_bytes(bar_bytes)
            (temp_root / 'baz.bin').write_bytes(baz_bytes)
            (temp_root / 'qux.exe').write_bytes(qux_bytes)
            bundle = update_bundle.gen_unsigned_update_bundle(temp_root)

        self.assertEqual(foo_bytes, bundle.target_payloads['foo.bin'])
        self.assertEqual(bar_bytes, bundle.target_payloads['bar.bin'])
        self.assertEqual(baz_bytes, bundle.target_payloads['baz.bin'])
        self.assertEqual(qux_bytes, bundle.target_payloads['qux.exe'])

    def test_excludes(self):
        """Checks that excludes are excluded from update bundles."""
        foo_bytes = b'\xf0\x0b\xa4'
        bar_bytes = b'\x0b\xa4\x99'
        baz_bytes = b'\xba\x59\x06'
        qux_bytes = b'\x8a\xf3\x12'
        with tempfile.TemporaryDirectory() as tempdir_name:
            temp_root = Path(tempdir_name)
            (temp_root / 'foo.bin').write_bytes(foo_bytes)
            (temp_root / 'bar.bin').write_bytes(bar_bytes)
            (temp_root / 'baz.bin').write_bytes(baz_bytes)
            (temp_root / 'qux.exe').write_bytes(qux_bytes)
            bundle = update_bundle.gen_unsigned_update_bundle(
                temp_root, exclude=(Path('foo.bin'), Path('baz.bin')))

        self.assertNotIn('foo.bin', bundle.target_payloads)
        self.assertEqual(bar_bytes, bundle.target_payloads['bar.bin'])
        self.assertNotIn(
            'baz.bin',
            bundle.target_payloads,
        )
        self.assertEqual(qux_bytes, bundle.target_payloads['qux.exe'])

    def test_excludes_and_remapping(self):
        """Checks that remapping works, even in combination with excludes."""
        foo_bytes = b'\x12\xab\x34'
        bar_bytes = b'\xcd\x56\xef'
        baz_bytes = b'\xa1\xb2\xc3'
        qux_bytes = b'\x1f\x2e\x3d'
        remap_paths = {
            Path('foo.bin'): 'main',
            Path('bar.bin'): 'backup',
            Path('baz.bin'): 'tertiary',
        }
        with tempfile.TemporaryDirectory() as tempdir_name:
            temp_root = Path(tempdir_name)
            (temp_root / 'foo.bin').write_bytes(foo_bytes)
            (temp_root / 'bar.bin').write_bytes(bar_bytes)
            (temp_root / 'baz.bin').write_bytes(baz_bytes)
            (temp_root / 'qux.exe').write_bytes(qux_bytes)
            bundle = update_bundle.gen_unsigned_update_bundle(
                temp_root,
                exclude=(Path('qux.exe'), ),
                remap_paths=remap_paths)

        self.assertEqual(foo_bytes, bundle.target_payloads['main'])
        self.assertEqual(bar_bytes, bundle.target_payloads['backup'])
        self.assertEqual(baz_bytes, bundle.target_payloads['tertiary'])
        self.assertNotIn('qux.exe', bundle.target_payloads)

    def test_incomplete_remapping_logs(self):
        """Checks that incomplete remappings log warnings."""
        foo_bytes = b'\x12\xab\x34'
        bar_bytes = b'\xcd\x56\xef'
        remap_paths = {Path('foo.bin'): 'main'}
        with tempfile.TemporaryDirectory() as tempdir_name:
            temp_root = Path(tempdir_name)
            (temp_root / 'foo.bin').write_bytes(foo_bytes)
            (temp_root / 'bar.bin').write_bytes(bar_bytes)
            with self.assertLogs(level='WARNING') as log:
                update_bundle.gen_unsigned_update_bundle(
                    temp_root,
                    exclude=(Path('qux.exe'), ),
                    remap_paths=remap_paths)
                self.assertIn('Some remaps defined, but not "bar.bin"',
                              log.output[0])

    def test_remap_of_missing_file(self):
        """Checks that remapping a missing file raises an error."""
        foo_bytes = b'\x12\xab\x34'
        remap_paths = {
            Path('foo.bin'): 'main',
            Path('bar.bin'): 'backup',
        }
        with tempfile.TemporaryDirectory() as tempdir_name:
            temp_root = Path(tempdir_name)
            (temp_root / 'foo.bin').write_bytes(foo_bytes)
            with self.assertRaises(FileNotFoundError):
                update_bundle.gen_unsigned_update_bundle(
                    temp_root, remap_paths=remap_paths)


class ParseRemapArgTest(unittest.TestCase):
    """Test the parsing of remap argument strings."""
    def test_valid_arg(self):
        """Checks that valid remap strings are parsed correctly."""
        original_path, new_target_file_name = update_bundle.parse_remap_arg(
            'foo.bin > main')
        self.assertEqual(Path('foo.bin'), original_path)
        self.assertEqual('main', new_target_file_name)

    def test_invalid_arg_raises(self):
        """Checks that invalid remap string raise an error."""
        with self.assertRaises(ValueError):
            update_bundle.parse_remap_arg('foo.bin main')


if __name__ == '__main__':
    unittest.main()
