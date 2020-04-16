#!/usr/bin/env python3
# Copyright 2020 The Pigweed Authors
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
"""Tests the tokenized string decode module."""

import unittest

import tokenized_string_decoding_test_data as tokenized_string
import varint_test_data
from pw_tokenizer import decode


def error(msg, value=None):
    """Formats msg as the message for an argument that failed to parse."""
    if value is None:
        return '<[{}]>'.format(msg)
    return '<[{} ({})]>'.format(msg, value)


class TestDecodeTokenized(unittest.TestCase):
    """Tests decoding tokenized strings with various arguments."""
    def test_decode_generated_data(self):
        self.assertGreater(len(tokenized_string.TEST_DATA), 100)

        for fmt, decoded, encoded in tokenized_string.TEST_DATA:
            self.assertEqual(decode.decode(fmt, encoded, True), decoded)

    def test_unicode_decode_errors(self):
        """Tests unicode errors, which do not occur in the C++ decoding code."""
        self.assertEqual(decode.decode('Why, %c', b'\x01', True),
                         'Why, ' + error('%c ERROR', -1))

        self.assertEqual(
            decode.decode('%sXY%+ldxy%u', b'\x83N\x80!\x01\x02', True),
            '{}XY{}xy{}'.format(error('%s ERROR', "'N\\x80!'"),
                                error('%+ld SKIPPED', -1),
                                error('%u SKIPPED', 1)))

        self.assertEqual(
            decode.decode('%s%lld%9u', b'\x82$\x80\x80', True),
            '{0}{1}{2}'.format(error("%s ERROR ('$\\x80')"),
                               error('%lld SKIPPED'), error('%9u SKIPPED')))

        self.assertEqual(decode.decode('%c', b'\xff\xff\xff\xff\x0f', True),
                         error('%c ERROR', -2147483648))

    def test_ignore_errors(self):
        self.assertEqual(decode.decode('Why, %c', b'\x01'), 'Why, %c')

        self.assertEqual(decode.decode('%s %d', b'\x01!'), '! %d')

    def test_pointer(self):
        """Tests pointer args, which are not natively supported in Python."""
        self.assertEqual(decode.decode('Hello: %p', b'\x00', True),
                         'Hello: 0x00000000')
        self.assertEqual(decode.decode('%p%d%d', b'\x02\x80', True),
                         '0x00000001<[%d ERROR]><[%d SKIPPED]>')


class TestIntegerDecoding(unittest.TestCase):
    """Test decoding variable-length integers."""
    def test_decode_generated_data(self):
        test_data = varint_test_data.TEST_DATA
        self.assertGreater(len(test_data), 100)

        for signed_spec, signed, unsigned_spec, unsigned, encoded in test_data:
            self.assertEqual(
                int(signed),
                decode.FormatSpec.from_string(signed_spec).decode(
                    bytearray(encoded)).value)

            self.assertEqual(
                int(unsigned),
                decode.FormatSpec.from_string(unsigned_spec).decode(
                    bytearray(encoded)).value)


if __name__ == '__main__':
    unittest.main()
