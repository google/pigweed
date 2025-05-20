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

from pw_build.gn_target import GnTarget


class TestGnTarget(unittest.TestCase):
    """Tests for gn_target.GnTarget."""

    def test_target_name_and_type(self):
        """Tests setting and getting the GN target name and type."""
        target = GnTarget('my_target_type', 'some_target_name')
        self.assertEqual(target.name(), 'some_target_name')
        self.assertEqual(target.type(), 'my_target_type')

    def test_target_no_origin(self):
        target = GnTarget('type', 'name')
        self.assertEqual(target.origin, None)

    def test_target_no_build_args(self):
        target = GnTarget('type', 'name')
        self.assertEqual(list(target.build_args()), [])

    def test_target_with_build_args(self):
        target = GnTarget('type', 'name')
        target.attrs = {
            # build args in paths in these attributes should be included.
            'public': ['$dir_pw_third_party_foo/include/one-header.h'],
            'sources': [
                '$dir_pw_third_party_foo/src/source1.cc',
                '$dir_pw_third_party_bar/src/source2.cc',
            ],
            'inputs': [],
            'include_dirs': [
                '$dir_pw_third_party_bar/include/',
                '$dir_pw_third_party_baz/include/',
            ],
            # build args in labels should not be included.
            'configs': ['$pw_external_qux:config'],
            'deps': ['$pw_external_quux:dep'],
        }
        build_args = set(target.build_args())
        self.assertIn('$dir_pw_third_party_foo', build_args)
        self.assertIn('$dir_pw_third_party_bar', build_args)
        self.assertIn('$dir_pw_third_party_baz', build_args)


if __name__ == '__main__':
    unittest.main()
