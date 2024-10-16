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
"""Tests for pw_cli_analytics.analytics."""

import getpass
import os.path
import unittest

from pw_cli_analytics import analytics

# pylint: disable=no-member


class TestRemoveUsername(unittest.TestCase):
    """Tests for analytics.remove_username."""

    def setUp(self):
        self.home = '/home/username'
        self.username = 'username'

    def remove_username(self, arg: str) -> str:
        return analytics.remove_username(
            arg,
            username=self.username,
            home=self.home,
        )

    def test_remove_home_complete(self):
        self.assertEqual(self.remove_username(self.home), '$HOME')

    def test_remove_home(self):
        self.assertEqual(
            self.remove_username(f'{self.home}/foo/bar'),
            '$HOME/foo/bar',
        )

    def test_remove_username_complete(self):
        self.assertEqual(self.remove_username(self.username), '$USERNAME')

    def test_remove_username(self):
        self.assertEqual(
            self.remove_username(f'foo-{self.username}-bar'),
            'foo-$USERNAME-bar',
        )

    def test_noop_foobar(self):
        self.assertEqual(
            self.remove_username('foo-bar'),
            'foo-bar',
        )

    def test_noop_usernamenospaceafter(self):
        self.assertEqual(
            self.remove_username(f'{self.username}nospaceafter'),
            f'{self.username}nospaceafter',
        )

    def test_windows_remove_home_complete(self):
        self.home = r'C:\users\username'
        self.assertEqual(
            self.remove_username(self.home),
            '$HOME',
        )

    def test_windows_remove_home(self):
        self.home = r'C:\users\username'
        self.assertEqual(
            self.remove_username(rf'{self.home}\foo\bar'),
            r'$HOME\foo\bar',
        )


class TestRemoveUsernameEnviron(unittest.TestCase):
    """Tests for analytics.remove_username."""

    def setUp(self):
        self.username = getpass.getuser()
        self.home = os.path.expanduser('~')

    def remove_username(self, arg: str) -> str:
        return analytics.remove_username(
            arg,
            username=self.username,
            home=self.home,
        )

    def test_remove_home_complete(self):
        self.assertEqual(self.remove_username(self.home), '$HOME')

    def test_remove_home(self):
        self.assertEqual(
            self.remove_username(os.path.join(self.home, 'foo', 'bar')),
            os.path.join('$HOME', 'foo', 'bar'),
        )

    def test_remove_username_complete(self):
        self.assertEqual(self.remove_username(self.username), '$USERNAME')

    def test_remove_username(self):
        self.assertEqual(
            self.remove_username(f'foo-{self.username}-bar'),
            'foo-$USERNAME-bar',
        )


if __name__ == '__main__':
    unittest.main()
