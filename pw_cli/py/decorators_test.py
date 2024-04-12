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
"""Tests for pw_cli.decorators."""

# Redefine __package__ so we can test against a real package name.
__package__ = 'pw_cli.tests'  # pylint: disable=redefined-builtin

import os
import re
import unittest
import warnings

from pw_cli.decorators import deprecated


_VAL = 5


def _not_deprecated() -> int:
    return _VAL


@deprecated('Use `_not_deprecated()`')
def _deprecated_func() -> int:
    return _not_deprecated()


class TestDeprecationAnnotation(unittest.TestCase):
    """Tests for @deprecated annotation."""

    def test_deprecated(self):
        expected_file_path = os.path.sep.join(
            ('pw_cli', 'py', 'decorators_test.py')
        )
        expected_warning_re = ' '.join(
            (
                re.escape(expected_file_path) + r':[0-9]*:',
                r'pw_cli\.tests\.decorators_test\._deprecated_func\(\)',
                r'is deprecated\.',
                r'Use `_not_deprecated\(\)`',
            )
        )

        with warnings.catch_warnings(record=True) as caught_warnings:
            warnings.simplefilter("always")
            n = _deprecated_func()
            self.assertEqual(n, _VAL)
            self.assertEqual(len(caught_warnings), 1)

            self.assertRegex(
                str(caught_warnings[0].message), re.compile(expected_warning_re)
            )


if __name__ == '__main__':
    unittest.main()
