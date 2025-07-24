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
"""Tests for the runfiles_manager."""

import unittest

from pw_build import runfiles_manager

try:
    # pylint: disable=unused-import
    from python.runfiles import runfiles  # type: ignore

    _IS_GN = False
except ImportError:
    _IS_GN = True


class TestRunfilesManager(unittest.TestCase):
    """Tests for RunfilesManager.

    These tests should always be run in BOTH the GN and Bazel builds.
    There are different code paths tested in each. If one of the two breaks,
    this test should NEVER have "if gn" or "if bazel" magic, since the purpose
    of the RunfilesManager is to make handling both builds ergonomic.
    """

    def setUp(self):
        self.r = runfiles_manager.RunfilesManager()

    def test_runfiles_from_path(self):
        self.r.add_bazel_tool('a', 'pw_build_external_runfile_resource.black')
        self.r.add_bootstrapped_tool('a', 'black', from_shell_path=True)
        black_tool = self.r.get('a')
        self.assertTrue(black_tool.is_file())

    def test_runfiles_at_root(self):
        self.r.add_bazel_file('b', 'pw_build.test_runfile')
        self.r.add_bazel_file('c', 'pw_build.another_test_runfile')
        self.r.add_bootstrapped_file(
            'b', '${PW_ROOT}/pw_build/test_data/test_runfile.txt'
        )
        self.r.add_bootstrapped_file(
            'c', '${PW_ROOT}/pw_build/test_data/test_runfile.txt'
        )

        b = self.r['b']
        self.assertTrue(b.is_file())
        self.assertEqual(b.read_text(), "OK\n")

        c = self.r.get('c')
        self.assertTrue(c.is_file())
        self.assertEqual(c.read_text(), "OK\n")

    def test_bazel_missing(self):
        self.r.add_bootstrapped_file(
            'b', '${PW_ROOT}/pw_build/test_data/test_runfile.txt'
        )

        with self.assertRaisesRegex(AssertionError, 'Either register in'):
            self.r.get('b')

    def test_bootstrap_missing(self):
        self.r.add_bazel_file('b', 'pw_build.test_runfile')

        with self.assertRaisesRegex(AssertionError, 'Either register in'):
            self.r.get('b')

    def test_both_missing(self):
        with self.assertRaisesRegex(
            FileNotFoundError,
            'is not a registered tool or runfile resource',
        ):
            self.r.get('a')


if _IS_GN:

    class TestBootstrapRunfilesManager(unittest.TestCase):
        """GN-only tests for RunfilesManager."""

        def setUp(self):
            self.r = runfiles_manager.RunfilesManager()

        def test_forgot_bazel(self):
            with self.assertRaisesRegex(
                AssertionError,
                '`a` was registered for bootstrap environments, but not for '
                r'Bazel environments\. Either register in Bazel',
            ):
                self.r.add_bootstrapped_tool(
                    'a',
                    '${PW_ROOT}/pw_build/test_data/test_runfile.txt',
                )
                self.r.get('a')

        def test_not_a_file(self):
            with self.assertRaisesRegex(
                FileNotFoundError,
                r'Runfile `a=[^`]+` does not exist',
            ):
                self.r.add_bootstrapped_tool(
                    'a',
                    '${PW_ROOT}/pw_build/test_data/not_a_test_runfile.txt',
                )
                self.r.get('a')

        def test_unspecified_env_var(self):
            with self.assertRaisesRegex(
                ValueError,
                'no environment variable name',
            ):
                self.r.add_bootstrapped_tool('a', '${}/nah')

        def test_unknown_env_var(self):
            with self.assertRaisesRegex(
                AssertionError,
                'Failed to expand the following environment variables for '
                r'runfile entry `a=\${PW_ROOT}/\${_PW_VAR_D9EC8687538}/'
                r'\${_PW_VAR_DEF6F3B0CA7}`: _PW_VAR_D9EC8687538, '
                '_PW_VAR_DEF6F3B0CA7',
            ):
                self.r.add_bootstrapped_tool(
                    'a',
                    '${PW_ROOT}/${_PW_VAR_D9EC8687538}/${_PW_VAR_DEF6F3B0CA7}',
                )

        def test_misused_resource(self):
            self.r.add_bazel_file('a', 'pw_build.test_runfile', exclusive=True)

            with self.assertRaisesRegex(
                AssertionError,
                'was marked as `exclusive=True` to Bazel environments, '
                'but was used in a bootstrap environment',
            ):
                self.r.get('a')

else:

    class TestBazelRunfilesManager(unittest.TestCase):
        """Bazel-only tests for RunfilesManager."""

        def setUp(self):
            self.r = runfiles_manager.RunfilesManager()

        def test_forgot_bootstrap(self):
            with self.assertRaisesRegex(
                AssertionError,
                '`a` was registered for Bazel environments, but not for '
                r'bootstrap environments\. Either register in bootstrap',
            ):
                self.r.add_bazel_tool('a', 'pw_build.test_runfile')
                self.r.get('a')

        def test_bad_import_path(self):
            with self.assertRaisesRegex(
                ImportError,
                'Did you forget to add a dependency',
            ):
                self.r.add_bazel_tool('a', 'no.ahha.lol')

        def test_misused_resource(self):
            self.r.add_bootstrapped_file(
                'a',
                '${PW_ROOT}/pw_build/test_data/test_runfile.txt',
                exclusive=True,
            )

            with self.assertRaisesRegex(
                AssertionError,
                'was marked as `exclusive=True` to bootstrap environments, '
                'but was used in a Bazel environment',
            ):
                self.r.get('a')


if __name__ == '__main__':
    unittest.main()
