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
"""Tests for the pw_build.gn_config module."""

import unittest

from typing import Iterable

from pw_build.gn_config import (
    consolidate_configs,
    GnConfig,
    GN_CONFIG_FLAGS,
)
from pw_build.gn_utils import GnLabel, MalformedGnError


class TestGnConfig(unittest.TestCase):
    """Tests for gn_config.GnConfig.

    Attributes:
        config: A common config for testing.
    """

    def compare(self, actual: Iterable[str], *expected: str):
        """Sorts lists and compares them."""
        self.assertEqual(sorted(expected), sorted(list(actual)))

    def setUp(self):
        self.config = GnConfig()

    def test_bool_empty(self):
        """Identifies an empty config correctly."""
        self.assertFalse(self.config)

    def test_bool_nonempty(self):
        """Identifies a non-empty config correctly."""
        self.config.add('defines', 'KEY=VAL')
        self.assertTrue(self.config)

    def test_has_none(self):
        """Indicates a config does not have a flag when it has no values."""
        self.assertFalse(self.config.has('cflags'))

    def test_has_single(self):
        """Indicates a config has a flag when it has one value."""
        self.config.add('cflags', '-frobinator')
        self.assertTrue(self.config.has('cflags'))

    def test_has_multiple(self):
        """Indicates a config has a flag when it has multiple values."""
        self.config.add('cflags', '-frobinator')
        self.config.add('cflags', '-fizzbuzzer')
        self.assertTrue(self.config.has('cflags'))

    def test_has_two_flags(self):
        """Indicates a config has a flag when it has multiple flags."""
        self.config.add('cflags', '-frobinator')
        self.config.add('cflags_c', '-foobarbaz')
        self.assertTrue(self.config.has('cflags'))

    def test_add_and_get(self):
        """Tests ability to add valid flags to a config."""
        for flag in GN_CONFIG_FLAGS:
            self.config.add(flag, f'{flag}_value1')
            self.config.add(flag, f'{flag}_value2', f'{flag}_value3')
        for flag in GN_CONFIG_FLAGS:
            self.compare(
                self.config.get(flag),
                f'{flag}_value1',
                f'{flag}_value2',
                f'{flag}_value3',
            )

    def test_add_and_get_invalid(self):
        """Tests ability to add only valid flags to a config."""
        with self.assertRaises(MalformedGnError):
            self.config.add('invalid', 'value')

    def test_take_missing(self):
        """Tests ability to return an empty list when taking a missing flag."""
        self.compare(self.config.take('cflags'))
        self.assertFalse(self.config.has('defines'))
        self.assertFalse(self.config.has('cflags'))

    def test_take(self):
        """Tests ability to remove and return values from a config."""
        self.config.add('defines', 'KEY1=VAL1', 'KEY2=VAL2')
        self.config.add('cflags', '-frobinator', '-fizzbuzzer')
        self.assertTrue(self.config.has('defines'))
        self.assertTrue(self.config.has('cflags'))

        self.compare(self.config.take('cflags'), '-frobinator', '-fizzbuzzer')
        self.assertTrue(self.config.has('defines'))
        self.assertFalse(self.config.has('cflags'))

    def test_deduplicate_no_label(self):
        """Tests raising an error from deduplicating a labelless config."""
        cfg1 = GnConfig()
        cfg1.add('defines', 'KEY1=VAL1')
        with self.assertRaises(ValueError):
            list(self.config.deduplicate(cfg1))

    def test_deduplicate_empty(self):
        """Tests deduplicating an empty config."""
        cfg2 = GnConfig()
        cfg2.label = ':cfg2'
        self.assertFalse(list(self.config.deduplicate(cfg2)))

    def test_deduplicate_not_subset(self):
        """Tests deduplicating a config whose values are not a subset."""
        self.config.add('defines', 'KEY1=VAL1', 'KEY2=VAL2')
        self.config.add('cflags', '-frobinator', '-fizzbuzzer', '-foobarbaz')

        cfg3 = GnConfig()
        cfg3.label = ':cfg3'
        cfg3.add('defines', 'KEY1=VAL1', 'KEY3=VAL3')
        self.assertFalse(list(self.config.deduplicate(cfg3)))

    def test_deduplicate(self):
        """Tests deduplicating multiple overlapping configs."""
        self.config.add('defines', 'KEY1=VAL1', 'KEY2=VAL2')
        self.config.add('cflags', '-frobinator', '-fizzbuzzer', '-foobarbaz')

        cfg4 = GnConfig()
        cfg4.label = ':cfg4'
        cfg4.add('defines', 'KEY1=VAL1')
        cfg4.add('cflags', '-frobinator')

        cfg5 = GnConfig()
        cfg5.label = ':cfg5'
        cfg5.add('defines', 'KEY1=VAL1')
        cfg5.add('cflags', '-foobarbaz')

        cfg6 = GnConfig()
        cfg6.label = ':skipped'
        cfg6.add('cflags', '-foobarbaz')

        cfgs = [
            str(cfg.label) for cfg in self.config.deduplicate(cfg4, cfg5, cfg6)
        ]
        self.assertEqual(cfgs, [':cfg4', ':cfg5'])
        self.compare(self.config.get('defines'), 'KEY2=VAL2')
        self.compare(self.config.get('cflags'), '-fizzbuzzer')

    def test_extract_public(self):
        """Tests ability to removes and return `public_config` values."""
        self.config.add('public_defines', 'KEY1=VAL1', 'KEY2=VAL2')
        self.config.add('defines', 'KEY3=VAL3', 'KEY4=VAL4')
        self.config.add('include_dirs', 'foo', 'bar', 'baz')
        self.config.add('cflags', '-frobinator', '-fizzbuzzer')

        config = self.config.extract_public()

        self.assertFalse(self.config.has('public_defines'))
        self.assertFalse(self.config.has('include_dirs'), 'KEY2=VAL2')
        self.compare(self.config.get('defines'), 'KEY3=VAL3', 'KEY4=VAL4')
        self.compare(self.config.get('cflags'), '-frobinator', '-fizzbuzzer')

        self.assertFalse(config.has('public_defines'))
        self.compare(config.get('defines'), 'KEY1=VAL1', 'KEY2=VAL2')
        self.compare(config.get('include_dirs'), 'foo', 'bar', 'baz')
        self.assertFalse(config.has('cflags'))

    def test_consolidate_configs(self):
        """Tests ability to merges configs into the smallest exact cover."""
        cfg1 = GnConfig()
        cfg1.add('cflags', 'one', 'a', 'b')
        cfg1.add('defines', 'k=1')
        cfg1.add('include_dirs', 'foo', 'bar')

        cfg2 = GnConfig()
        cfg2.add('cflags', 'one', 'a', 'b', 'c')
        cfg2.add('defines', 'k=1')
        cfg2.add('include_dirs', 'foo', 'baz')

        cfg3 = GnConfig()
        cfg3.add('cflags', 'one', 'two', 'a', 'b')
        cfg3.add('defines', 'k=1')
        cfg3.add('include_dirs', 'foo', 'bar')

        cfg4 = GnConfig()
        cfg4.add('cflags', 'one', 'b', 'c')
        cfg4.add('defines', 'k=0')
        cfg4.add('include_dirs', 'foo', 'baz')

        label = GnLabel('//foo/bar')
        common = list(consolidate_configs(label, cfg1, cfg2, cfg3, cfg4))
        self.assertEqual(len(common), 3)
        self.assertEqual(str(common[0].label), '//foo/bar:bar_public_config1')
        self.assertEqual(str(common[1].label), '//foo/bar:bar_config1')
        self.assertEqual(str(common[2].label), '//foo/bar:bar_config2')
        self.compare(common[0].get('include_dirs'), 'foo')
        self.compare(common[1].get('cflags'), 'one', 'b')
        self.compare(common[2].get('cflags'), 'a')
        self.compare(common[2].get('defines'), 'k=1')


if __name__ == '__main__':
    unittest.main()
