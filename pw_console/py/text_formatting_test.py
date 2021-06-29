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
"""Tests for pw_console.text_formatting"""

import unittest
from parameterized import parameterized  # type: ignore

from pw_console.text_formatting import get_line_height


class TestTextFormatting(unittest.TestCase):
    """Tests for text_formatting functions."""
    @parameterized.expand([
        (
            'with short prefix height 2',
            len('LINE that should be wrapped'),  # text_width
            len('|                |'),  # screen_width
            len('--->'),  # prefix_width
            (   'LINE that should b\n'
                '--->e wrapped     \n').count('\n'),  # expected_height
            len(             '_____'),  # expected_trailing_characters
        ),
        (
            'with short prefix height 3',
            len('LINE that should be wrapped three times.'),  # text_width
            len('|                |'),  # screen_width
            len('--->'),  # prefix_width
            (   'LINE that should b\n'
                '--->e wrapped thre\n'
                '--->e times.      \n').count('\n'),  # expected_height
            len(            '______'),  # expected_trailing_characters
        ),
        (
            'with short prefix height 4',
            len('LINE that should be wrapped even more times, say four.'),
            len('|                |'),  # screen_width
            len('--->'),  # prefix_width
            (   'LINE that should b\n'
                '--->e wrapped even\n'
                '---> more times, s\n'
                '--->ay four.      \n').count('\n'),  # expected_height
            len(            '______'),  # expected_trailing_characters
        ),
        (
            'no wrapping needed',
            len('LINE wrapped'),  # text_width
            len('|                |'),  # screen_width
            len('--->'),  # prefix_width
            (   'LINE wrapped      \n').count('\n'),  # expected_height
            len(            '______'),  # expected_trailing_characters
        ),
        (
            'prefix is > screen width',
            len('LINE that should be wrapped'),  # text_width
            len('|                |'),  # screen_width
            len('------------------>'),  # prefix_width
            (   'LINE that should b\n'
                'e wrapped         \n').count('\n'),  # expected_height
            len(         '_________'),  # expected_trailing_characters
        ),
        (
            'prefix is == screen width',
            len('LINE that should be wrapped'),  # text_width
            len('|                |'),  # screen_width
            len('----------------->'),  # prefix_width
            (   'LINE that should b\n'
                'e wrapped         \n').count('\n'),  # expected_height
            len(         '_________'),  # expected_trailing_characters
        ),
    ]) # yapf: disable

    def test_get_line_height(self, _name, text_width, screen_width,
                             prefix_width, expected_height,
                             expected_trailing_characters) -> None:
        """Test line height calculations."""
        height, remaining_width = get_line_height(text_width, screen_width,
                                                  prefix_width)
        self.assertEqual(height, expected_height)
        self.assertEqual(remaining_width, expected_trailing_characters)


if __name__ == '__main__':
    unittest.main()
