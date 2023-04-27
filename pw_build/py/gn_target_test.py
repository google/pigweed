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
"""Tests for the pw_build.gn_target module."""

import unittest

from pw_build.bazel_query import BazelRule
from pw_build.gn_target import GnTarget
from pw_build.gn_utils import GnLabel, MalformedGnError


class TestGnTarget(unittest.TestCase):
    """Tests for gn_target.GnTarget."""

    def setUp(self):
        self.rule = BazelRule('//my-package:my-target', 'cc_library')

    def test_from_bazel_rule_label(self):
        """Tests the GN target name and package derived from a Bazel rule."""
        target = GnTarget('$build', '$src', bazel=self.rule)
        self.assertEqual(target.name(), 'my-target')
        self.assertEqual(target.package(), 'my-package')

    def test_from_bazel_rule_type_source_set(self):
        """Tests creating a `pw_source_set` from a Bazel rule."""
        target = GnTarget('$build', '$src', bazel=self.rule)
        self.assertEqual(target.type(), 'pw_source_set')

    def test_from_bazel_rule_type_static_lib(self):
        """Tests creating a `pw_static_library` from a Bazel rule."""
        self.rule.set_attr('linkstatic', True)
        target = GnTarget('$build', '$src', bazel=self.rule)
        self.assertEqual(target.type(), 'pw_static_library')

    def test_from_bazel_rule_type_invalid(self):
        """Tests creating an invalid type from a Bazel rule."""
        with self.assertRaises(MalformedGnError):
            rule = BazelRule('//my-package:my-target', 'custom_type')
            GnTarget('$build', '$src', bazel=rule)

    def test_from_bazel_rule_visibility(self):
        """Tests getting the GN visibility from a Bazel rule."""
        self.rule.add_visibility('//visibility:private')
        self.rule.add_visibility('//foo:__subpackages__')
        self.rule.add_visibility('//foo/bar:__pkg__')
        target = GnTarget('$build', '$src', bazel=self.rule)
        self.assertEqual(
            {str(scope) for scope in target.visibility},
            {
                '$build/my-package:*',
                '$build/foo/*',
                '$build/foo/bar:*',
            },
        )

    def test_from_bazel_rule_visibility_public(self):
        """Tests that 'public' overrides any other visibility"""
        self.rule.add_visibility('//visibility:private')
        self.rule.add_visibility('//visibility:public')
        target = GnTarget('$build', '$src', bazel=self.rule)
        self.assertEqual(target.visibility, [])

    def test_from_bazel_rule_visibility_invalid(self):
        """Tests that and invalid visibility raises an error."""
        self.rule.add_visibility('invalid')
        with self.assertRaises(MalformedGnError):
            GnTarget('$build', '$src', bazel=self.rule)

    def test_from_bazel_rule_testonly_unset(self):
        """Tests that omitting `testonly` defaults it to False."""
        target = GnTarget('$build', '$src', bazel=self.rule)
        self.assertFalse(target.testonly)

    def test_from_bazel_rule_testonly_false(self):
        """Tests setting `testonly` to False."""
        self.rule.set_attr('testonly', False)
        target = GnTarget('$build', '$src', bazel=self.rule)
        self.assertFalse(target.testonly)

    def test_from_bazel_rule_testonly_true(self):
        """Tests setting `testonly` to True."""
        self.rule.set_attr('testonly', True)
        target = GnTarget('$build', '$src', bazel=self.rule)
        self.assertTrue(target.testonly)

    def test_from_bazel_rule_public(self):
        """Tests getting the GN public headers from a Bazel rule."""
        self.rule.set_attr('hdrs', ['//foo:bar.h', '//:baz.h'])
        target = GnTarget('$build', '$src', bazel=self.rule)
        self.assertEqual(
            {str(path) for path in target.public},
            {
                '$src/foo/bar.h',
                '$src/baz.h',
            },
        )

    def test_from_bazel_rule_sources(self):
        """Tests getting the GN source files from a Bazel rule."""
        self.rule.set_attr('srcs', ['//foo:bar.cc', '//:baz.cc'])
        target = GnTarget('$build', '$src', bazel=self.rule)
        self.assertEqual(
            {str(path) for path in target.sources},
            {
                '$src/foo/bar.cc',
                '$src/baz.cc',
            },
        )

    def test_from_bazel_rule_inputs(self):
        """Tests getting the GN input files from a Bazel rule."""
        self.rule.set_attr(
            'additional_linker_inputs', ['//foo:bar.data', '//:baz.data']
        )
        target = GnTarget('$build', '$src', bazel=self.rule)
        self.assertEqual(
            {str(path) for path in target.inputs},
            {
                '$src/foo/bar.data',
                '$src/baz.data',
            },
        )

    def test_from_bazel_rule_include_dirs(self):
        """Tests getting the GN include directories from a Bazel rule."""
        self.rule.set_attr('includes', ['//foo'])
        target = GnTarget('$build', '$src', bazel=self.rule)
        self.assertEqual(
            set(target.config.get('include_dirs')),
            {
                '$src',
                '$src/foo',
            },
        )

    def test_from_bazel_rule_configs(self):
        """Tests getting the GN config flags from a Bazel rule."""
        self.rule.set_attr('defines', ['KEY1=VAL1'])
        self.rule.set_attr('copts', ['-frobinator'])
        self.rule.set_attr('linkopts', ['-fizzbuzzer'])
        self.rule.set_attr('local_defines', ['KEY2=VAL2', 'KEY3=VAL3'])
        target = GnTarget('$build', '$src', bazel=self.rule)
        self.assertEqual(
            set(target.config.get('public_defines')), {'KEY1=VAL1'}
        )
        self.assertEqual(set(target.config.get('cflags')), {'-frobinator'})
        self.assertEqual(set(target.config.get('ldflags')), {'-fizzbuzzer'})
        self.assertEqual(
            set(target.config.get('defines')), {'KEY2=VAL2', 'KEY3=VAL3'}
        )

    def test_from_bazel_rule_deps(self):
        """Tests getting the GN dependencies from a Bazel rule."""
        self.rule.set_attr(
            'deps', ['//my-package:foo', '@com_corp_project//bar']
        )
        self.rule.set_attr(
            'implementation_deps',
            [
                '//other-pkg/subdir',
                '@com_corp_project//:top-level',
            ],
        )
        target = GnTarget('$build', '$src', bazel=self.rule)
        self.assertEqual(
            {str(label) for label in target.public_deps},
            {
                '$build/my-package:foo',
                '$repo/bar',
            },
        )
        self.assertEqual(
            {str(label) for label in target.deps},
            {
                '$build/other-pkg/subdir',
                '$repo:top-level',
            },
        )

    def test_make_relative_adjacent(self):
        """Tests rebasing labels for a target."""
        target = GnTarget('$build/pkg', '$src')
        label = target.make_relative(GnLabel('$build/pkg:adjacent-config'))
        self.assertEqual(label, ":adjacent-config")

    def test_make_relative_absolute(self):
        """Tests rebasing labels for a target."""
        target = GnTarget('$build/pkg', '$src')
        label = target.make_relative(GnLabel('//absolute:config'))
        self.assertEqual(label, "//absolute:config")

    def test_make_relative_descend(self):
        """Tests rebasing labels for a target."""
        target = GnTarget('$build/pkg', '$src')
        label = target.make_relative(GnLabel('$build/pkg/relative:dep'))
        self.assertEqual(label, "relative:dep")

    def test_make_relative_ascend(self):
        """Tests rebasing labels for a target."""
        target = GnTarget('$build/pkg', '$src')
        label = target.make_relative(GnLabel('$build/dotdot/relative'))
        self.assertEqual(label, "../dotdot/relative")


if __name__ == '__main__':
    unittest.main()
