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
"""Tests the hdlc_lite decoder module."""

import unittest

from pw_hdlc_lite import decoder


class TestDecoder(unittest.TestCase):
    """Tests decoding bytes with different arguments."""
    def test_decode_1byte_payload(self):
        decode = decoder.Decoder()
        test_array = b'\x7EA\x15\xB9\x7E'
        decoded_packets = list(decode.add_bytes(test_array))
        self.assertEqual(decoded_packets, [b'A'])

    def test_decode_empty_payload(self):
        decode = decoder.Decoder()
        test_array = b'\x7E\xFF\xFF\x7E'
        decoded_packets = list(decode.add_bytes(test_array))
        self.assertEqual(decoded_packets, [b''])

    def test_decode_9byte_payload(self):
        decode = decoder.Decoder()
        test_array = b'\x7E123456789\xB1\x29\x7E'
        decoded_packets = list(decode.add_bytes(test_array))
        self.assertEqual(decoded_packets, [b'123456789'])

    def test_decode_unescaping_payload_escapeflag(self):
        decode = decoder.Decoder()
        test_array = b'\x7E\x7D\x5D\xCA\x4E\x7E'
        decoded_packets = list(decode.add_bytes(test_array))
        self.assertEqual(decoded_packets, [b'\x7D'])

    def test_decode_unescaping_payload_framedelimiter(self):
        decode = decoder.Decoder()
        test_array = b'\x7E\x7D\x5E\xA9\x7D\x5E\x7E'
        decoded_packets = list(decode.add_bytes(test_array))
        self.assertEqual(decoded_packets, [b'\x7E'])

    def test_decode_unescaping_payload_mix(self):
        decode = decoder.Decoder()
        test_array = b'\x7E\x7D\x5E\x7Babc\x7D\x5D\x7D\x5E\x49\xE5\x7E'
        decoded_packets = list(decode.add_bytes(test_array))
        self.assertEqual(decoded_packets, [b'~{abc}~'])

    def test_decode_in_parts(self):
        decode = decoder.Decoder()
        test_array = b'\x7EA\x15\xB9\x7E\x7EA\x15\xB9\x7E'
        decoded_packets = list(decode.add_bytes(test_array[:3]))
        self.assertEqual(decoded_packets, [])
        decoded_packets = list(decode.add_bytes(test_array[3:8]))
        self.assertEqual(decoded_packets, [b'A'])
        decoded_packets = list(decode.add_bytes(test_array[8:]))
        self.assertEqual(decoded_packets, [b'A'])

        decoded_packets = list(decode.add_bytes(test_array))
        self.assertEqual(len(decoded_packets), 2)
        self.assertEqual(decoded_packets, [b'A', b'A'])

    def test_decode_incorrectcrc(self):
        decode = decoder.Decoder()
        test_array = b'\x7EA\x15\xB8\x7E'
        with self.assertRaises(decoder.CrcMismatchError):
            next(decode.add_bytes(test_array))

    def test_decode_incorrectcrc_mix(self):
        decode = decoder.Decoder()
        test_array = b'\x7EA\x15\xB9\x7E\x7EA\x15\xB8\x7E'
        decoded_packets = decode.add_bytes(test_array)
        self.assertEqual(next(decoded_packets), b'A')
        with self.assertRaises(decoder.CrcMismatchError):
            next(decoded_packets)


if __name__ == '__main__':
    unittest.main()
