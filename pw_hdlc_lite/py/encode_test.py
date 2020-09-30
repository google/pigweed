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
"""Tests encoding HDLC frames."""

import unittest

from pw_hdlc_lite import encode
from pw_hdlc_lite import protocol
from pw_hdlc_lite.protocol import frame_check_sequence as _fcs

FLAG = bytes([protocol.FLAG])


def _with_fcs(data: bytes) -> bytes:
    return data + _fcs(data)


class TestEncodeInformationFrame(unittest.TestCase):
    """Tests Encoding bytes with different arguments using a custom serial."""
    def test_empty(self):
        self.assertEqual(encode.information_frame(0, b''),
                         FLAG + _with_fcs(b'\0\0') + FLAG)
        self.assertEqual(encode.information_frame(0x1a, b''),
                         FLAG + _with_fcs(b'\x1a\0') + FLAG)

    def test_1byte(self):
        self.assertEqual(encode.information_frame(0, b'A'),
                         FLAG + _with_fcs(b'\0\0A') + FLAG)

    def test_multibyte(self):
        self.assertEqual(encode.information_frame(0, b'123456789'),
                         FLAG + _with_fcs(b'\x00\x00123456789') + FLAG)

    def test_escape(self):
        self.assertEqual(
            encode.information_frame(0x7e, b'\x7d'),
            FLAG + b'\x7d\x5e\x00\x7d\x5d' + _fcs(b'\x7e\x00\x7d') + FLAG)
        self.assertEqual(
            encode.information_frame(0x7d, b'A\x7e\x7dBC'),
            FLAG + b'\x7d\x5d\x00A\x7d\x5e\x7d\x5dBC' +
            _fcs(b'\x7d\x00A\x7e\x7dBC') + FLAG)


if __name__ == '__main__':
    unittest.main()
