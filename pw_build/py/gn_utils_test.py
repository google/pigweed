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
"""Tests for the pw_build.gn_utils module."""

import unittest

from pw_build.gn_utils import GnLabel, GnPath, GnVisibility


class TestGnPath(unittest.TestCase):
    """Tests for gn_utils.GnPath."""

    def test_from_bazel(self):
        """Tests creating a GN path from a Bazel string."""
        path = GnPath('$dir_3p_test', bazel='//foo:bar/baz.txt')
        self.assertEqual(path.file(), 'baz.txt')
        self.assertEqual(path.name(), 'baz')
        self.assertEqual(path.extension(), 'txt')
        self.assertEqual(path.dir(), '$dir_3p_test/foo/bar')

    def test_from_gn(self):
        """Tests creating a GN path from a GN string."""
        path = GnPath('$dir_3p_test', gn='foo/bar/baz.txt')
        self.assertEqual(path.file(), 'baz.txt')
        self.assertEqual(path.name(), 'baz')
        self.assertEqual(path.extension(), 'txt')
        self.assertEqual(path.dir(), '$dir_3p_test/foo/bar')

    def test_from_str(self):
        """Tests creating a GN path from a raw string."""
        path = GnPath('$dir_3p_test/foo/bar/baz.txt')
        self.assertEqual(path.file(), 'baz.txt')
        self.assertEqual(path.name(), 'baz')
        self.assertEqual(path.extension(), 'txt')
        self.assertEqual(path.dir(), '$dir_3p_test/foo/bar')


class TestGnLabel(unittest.TestCase):
    """Tests for gn_utils.GnLabel."""

    def test_from_bazel_with_name(self):
        """Tests creating a GN label from a Bazel string including a name."""
        label = GnLabel('$dir_3p/test', bazel='//foo/bar:baz')
        self.assertEqual(label.name(), 'baz')
        self.assertEqual(label.dir(), '$dir_3p/test/foo/bar')
        self.assertEqual(label.no_toolchain(), '$dir_3p/test/foo/bar:baz')
        self.assertEqual(
            label.with_toolchain(),
            '$dir_3p/test/foo/bar:baz(default_toolchain)',
        )
        self.assertFalse(label.repo())

    def test_from_bazel_without_name(self):
        """Tests creating a GN label from a Bazel string without a name."""
        label = GnLabel('$dir_3p/test', bazel='//foo/bar')
        self.assertEqual(label.name(), 'bar')
        self.assertEqual(label.dir(), '$dir_3p/test/foo/bar')
        self.assertEqual(label.no_toolchain(), '$dir_3p/test/foo/bar')
        self.assertEqual(
            label.with_toolchain(), '$dir_3p/test/foo/bar(default_toolchain)'
        )
        self.assertFalse(label.repo())

    def test_from_bazel_with_external_repo(self):
        """Tests creating a GN label from a Bazel string with a repo."""
        label = GnLabel('$dir_3p/test', bazel='@com_corp_project//foo/bar:baz')
        self.assertEqual(label.name(), 'baz')
        self.assertEqual(label.dir(), '$repo/foo/bar')
        self.assertEqual(label.no_toolchain(), '$repo/foo/bar:baz')
        self.assertEqual(
            label.with_toolchain(),
            '$repo/foo/bar:baz(default_toolchain)',
        )
        self.assertEqual(label.repo(), 'com_corp_project')

    def test_from_gn_absolute(self):
        """Tests creating a GN label from an absolute GN label string."""
        label = GnLabel('$dir_3p/test', gn='//foo/bar:baz')
        self.assertEqual(label.name(), 'baz')
        self.assertEqual(label.dir(), '//foo/bar')
        self.assertEqual(label.no_toolchain(), '//foo/bar:baz')
        self.assertEqual(
            label.with_toolchain(), '//foo/bar:baz(default_toolchain)'
        )

    def test_from_gn_with_variable(self):
        """Tests creating a GN label from a GN string with a variable."""
        label = GnLabel('$dir_3p/test', gn='$dir_pw_build/foo/bar')
        self.assertEqual(label.name(), 'bar')
        self.assertEqual(label.dir(), '$dir_pw_build/foo/bar')
        self.assertEqual(label.no_toolchain(), '$dir_pw_build/foo/bar')
        self.assertEqual(
            label.with_toolchain(), '$dir_pw_build/foo/bar(default_toolchain)'
        )

    def test_from_gn_relative(self):
        """Tests creating a GN label from a relative GN label string."""
        label = GnLabel('$dir_3p/test', gn='foo/bar')
        self.assertEqual(label.name(), 'bar')
        self.assertEqual(label.dir(), '$dir_3p/test/foo/bar')
        self.assertEqual(label.no_toolchain(), '$dir_3p/test/foo/bar')
        self.assertEqual(
            label.with_toolchain(), '$dir_3p/test/foo/bar(default_toolchain)'
        )

    def test_from_gn_with_dotdot(self):
        """Tests creating a GN label from a GN string that ascends the tree."""
        label = GnLabel('$dir_3p/test', gn='../../../foo/../bar')
        self.assertEqual(label.name(), 'bar')
        self.assertEqual(label.dir(), '../bar')
        self.assertEqual(label.no_toolchain(), '../bar')
        self.assertEqual(label.with_toolchain(), '../bar(default_toolchain)')

    def test_from_str_with_name(self):
        """Tests creating a GN label from a raw string with a target name."""
        label = GnLabel('$dir_3p/test/foo/bar:baz')
        self.assertEqual(label.name(), 'baz')
        self.assertEqual(label.dir(), '$dir_3p/test/foo/bar')
        self.assertEqual(label.no_toolchain(), '$dir_3p/test/foo/bar:baz')
        self.assertEqual(
            label.with_toolchain(),
            '$dir_3p/test/foo/bar:baz(default_toolchain)',
        )

    def test_from_str_without_name(self):
        """Tests creating a GN label from a raw string without a name."""
        label = GnLabel('$dir_3p/test/foo/bar')
        self.assertEqual(label.name(), 'bar')
        self.assertEqual(label.dir(), '$dir_3p/test/foo/bar')
        self.assertEqual(label.no_toolchain(), '$dir_3p/test/foo/bar')
        self.assertEqual(
            label.with_toolchain(), '$dir_3p/test/foo/bar(default_toolchain)'
        )

    def test_relative_to_with_name(self):
        """Tests creating a relative label from a GN label with a name."""
        label = GnLabel('$dir_3p/foo/bar:baz')
        self.assertEqual(label.relative_to('$dir_3p'), 'foo/bar:baz')
        self.assertEqual(label.relative_to('$dir_3p/foo'), 'bar:baz')
        self.assertEqual(label.relative_to('$dir_3p/foo/bar'), ':baz')
        self.assertEqual(label.relative_to('$dir_3p/bar'), '../foo/bar:baz')
        self.assertEqual(label.relative_to('//'), '$dir_3p/foo/bar:baz')
        self.assertEqual(label.relative_to('$other'), '$dir_3p/foo/bar:baz')

    def test_relative_to_without_name(self):
        """Tests creating a relative label from a GN label without a name."""
        label = GnLabel('$dir_3p/foo/bar')
        self.assertEqual(label.relative_to('$dir_3p/foo/bar'), ':bar')

    def test_relative_to_absolute_with_name(self):
        """Tests creating a relative label from a named absolute GN label."""
        label = GnLabel('//foo/bar:baz')
        self.assertEqual(label.relative_to('//'), 'foo/bar:baz')
        self.assertEqual(label.relative_to('//bar/baz'), '../../foo/bar:baz')

    def test_relative_to_absolute_without_name(self):
        """Tests creating a relative label from an unnamed absolute GN label."""
        label = GnLabel('//foo/bar')
        self.assertEqual(label.relative_to('//foo/bar'), ':bar')

    def test_resolve_repo_without_repo(self):
        """Tests trying to set the repo placeholder for a local label."""
        label = GnLabel('$dir_3p/test', bazel='//foo/bar:baz')
        self.assertEqual(str(label), '$dir_3p/test/foo/bar:baz')
        self.assertFalse(label.repo())
        label.resolve_repo('my-external_repo')
        self.assertEqual(str(label), '$dir_3p/test/foo/bar:baz')
        self.assertFalse(label.repo())

    def test_resolve_repowith_repo(self):
        """Tests setting the repo placeholder."""
        label = GnLabel('$dir_3p/test', bazel='@com_corp_project//foo/bar:baz')
        self.assertEqual(str(label), '$repo/foo/bar:baz')
        self.assertEqual(label.repo(), 'com_corp_project')
        label.resolve_repo('my-external_repo')
        self.assertEqual(
            str(label), '$dir_pw_third_party/my-external_repo/foo/bar:baz'
        )
        self.assertEqual(label.repo(), 'com_corp_project')


class TestGnVisibility(unittest.TestCase):
    """Tests for gn_utils.GnVisibility."""

    def test_from_bazel_public(self):
        """Tests creating a public visibility scope from a Bazel string."""
        scope = GnVisibility(
            '$dir_3p/test', '$dir_3p/test/foo', bazel='//visibility:public'
        )
        self.assertEqual(str(scope), '//*')

    def test_from_bazel_private(self):
        """Tests creating a private visibility scope from a Bazel string."""
        scope = GnVisibility(
            '$dir_3p/test', '$dir_3p/test/foo', bazel='//visibility:private'
        )
        self.assertEqual(str(scope), '$dir_3p/test/foo:*')

    def test_from_bazel_same_subpackage(self):
        """Tests creating a visibility for the same subpackage."""
        scope = GnVisibility(
            '$dir_3p/test', '$dir_3p/test/foo', bazel='//foo:__subpackages__'
        )
        self.assertEqual(str(scope), '$dir_3p/test/foo/*')

    def test_from_bazel_same_package(self):
        """Tests creating a visibility for the same package."""
        scope = GnVisibility(
            '$dir_3p/test', '$dir_3p/test/foo', bazel='//foo:__pkg__'
        )
        self.assertEqual(str(scope), '$dir_3p/test/foo:*')

    def test_from_bazel_other_subpackages(self):
        """Tests creating a visibility for a different subpackage."""
        scope = GnVisibility(
            '$dir_3p/test', '$dir_3p/test/foo', bazel='//bar:__subpackages__'
        )
        self.assertEqual(str(scope), '$dir_3p/test/bar/*')

    def test_from_bazel_other_package(self):
        """Tests creating a visibility for a different package."""
        scope = GnVisibility(
            '$dir_3p/test', '$dir_3p/test/foo', bazel='//bar:__pkg__'
        )
        self.assertEqual(str(scope), '$dir_3p/test/bar:*')

    def test_from_gn_relative(self):
        """Tests creating a visibility from a relative GN string."""
        scope = GnVisibility('$dir_3p/test', '$dir_3p/test/foo', gn=':*')
        self.assertEqual(str(scope), '$dir_3p/test/foo:*')

    def test_from_gn_with_dotdot(self):
        """Tests creating a visibility from a string that ascends the tree."""
        scope = GnVisibility('$dir_3p/test', '$dir_3p/test/foo', gn='../*')
        self.assertEqual(str(scope), '$dir_3p/test/*')

    def test_from_gn_absolute(self):
        """Tests creating a visibility from an absolute GN string."""
        scope = GnVisibility('$dir_3p/test', '$dir_3p/test/foo', gn='$dir_3p/*')
        self.assertEqual(str(scope), '$dir_3p/*')

    def test_from_str(self):
        """Tests creating a visibility from a raw string."""
        scope = GnVisibility('$dir_3p/test', '$dir_3p/test/foo/bar:*')
        self.assertEqual(str(scope), '$dir_3p/test/foo/bar:*')


if __name__ == '__main__':
    unittest.main()
