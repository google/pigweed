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
"""Tests the hdlc_lite encoder"""

import unittest

from pw_hdlc_lite import encoder


class TestEncoderFunctions(unittest.TestCase):
    """Tests Encoding bytes with different arguments using a custom serial."""
    def test_encode_1byte_payload(self):
        data = bytearray()
        encoder.encode_and_write_payload(b'A', data.extend)

        expected_bytes = b'\x7EA\x15\xB9\x7E'
        self.assertEqual(data, expected_bytes)

    def test_encode_empty_payload(self):
        data = bytearray()
        encoder.encode_and_write_payload(b'', data.extend)

        expected_bytes = b'\x7E\xFF\xFF\x7E'
        self.assertEqual(data, expected_bytes)

    def test_encode_9byte_payload(self):
        data = bytearray()
        encoder.encode_and_write_payload(b'123456789', data.extend)

        expected_bytes = b'\x7E123456789\xB1\x29\x7E'
        self.assertEqual(data, expected_bytes)

    def test_encode_unescaping_payload_escapeflag(self):
        data = bytearray()
        encoder.encode_and_write_payload(b'\x7D', data.extend)

        expected_bytes = b'\x7E\x7D\x5D\xCA\x4E\x7E'
        self.assertEqual(data, expected_bytes)

    def test_encode_unescaping_payload_framedelimiter(self):
        data = bytearray()
        encoder.encode_and_write_payload(b'\x7E', data.extend)

        expected_bytes = b'\x7E\x7D\x5E\xA9\x7D\x5E\x7E'
        self.assertEqual(data, expected_bytes)

    def test_encode_unescaping_payload_mix(self):
        data = bytearray()
        encoder.encode_and_write_payload(b'~{abc}~', data.extend)

        expected_bytes = b'\x7E\x7D\x5E\x7Babc\x7D\x5D\x7D\x5E\x49\xE5\x7E'
        self.assertEqual(data, expected_bytes)

    def test_encode_multiple_payloads(self):
        data = bytearray()
        encoder.encode_and_write_payload(b'A', data.extend)
        encoder.encode_and_write_payload(b'A', data.extend)

        expected_bytes = b'\x7EA\x15\xB9\x7E\x7EA\x15\xB9\x7E'
        self.assertEqual(data, expected_bytes)


if __name__ == '__main__':
    unittest.main()
