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
"""Tests for pw_console.console_app"""

import unittest
from parameterized import parameterized  # type: ignore

from pw_console.helpers import get_line_height


class TestHelperFunctions(unittest.TestCase):
    """Tests for helper functions."""
    @parameterized.expand([
        (
            'with short prefix height 2',
            len('LINE that should be wrapped'),
            len('|                |'),
            len('--->'),
            ('LINE that should b\n'
             '--->e wrapped     \n').count('\n'),
        ),
        (
            'with short prefix height 3',
            len('LINE that should be wrapped three times.'),
            len('|                |'),
            len('--->'),
            ('LINE that should b\n'
             '--->e wrapped thre\n'
             '--->e times.      \n').count('\n'),
        ),
        (
            'no wrapping needed',
            len('LINE wrapped'),
            len('|                |'),
            len('--->'),
            'LINE wrapped\n'.count('\n'),
        ),
        (
            'prefix is > screen width',
            len('LINE that should be wrapped'),
            len('|                |'),
            len('------------------>'),
            ('LINE that should b\n'
             'e wrapped         \n').count('\n'),
        ),
        (
            'prefix is == screen width',
            len('LINE that should be wrapped'),
            len('|                |'),
            len('----------------->'),
            ('LINE that should b\n'
             'e wrapped         \n').count('\n'),
        ),
    ])
    def test_get_line_height(self, _name, text_width, screen_width,
                             prefix_width, expected_height) -> None:
        """Test line height calculations."""
        self.assertEqual(
            get_line_height(text_width, screen_width, prefix_width),
            expected_height)


if __name__ == '__main__':
    unittest.main()
