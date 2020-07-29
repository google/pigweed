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
"""Decoder class for decoding bytes using HDLC-Lite protocol"""

import binascii

from pw_hdlc_lite import constants


class CrcMismatchError(Exception):
    pass


class Decoder:
    """Decodes the received _data-frames using the HDLC-Lite protocol."""
    def __init__(self):
        self._data = bytearray()
        self._unescape_next_byte_flag = False

    def add_bytes(self, byte_array: bytes):
        """Unescapes the bytes and yields the CRC-verified packets.

        If the CRC-verification fails, the function will raise a
        CrcMismatchError exception.
        """
        for byte in byte_array:
            if byte == constants.HDLC_FRAME_DELIMITER:
                if self._data:
                    if self._check_crc():
                        yield self._data[:-2]
                    else:
                        raise CrcMismatchError()
                    self._data.clear()
            elif byte == constants.HDLC_ESCAPE:
                self._unescape_next_byte_flag = True
            else:
                self._add_unescaped_byte(byte)

    def _add_unescaped_byte(self, byte):
        """Unescapes the bytes based on the _unescape_next_byte_flag flag."""
        if self._unescape_next_byte_flag:
            self._data.append(byte ^ constants.HDLC_UNESCAPING_CONSTANT)
            self._unescape_next_byte_flag = False
        else:
            self._data.append(byte)

    def _check_crc(self):
        return binascii.crc_hqx(self._data[:-2], 0xFFFF).to_bytes(
            2, byteorder='little') == self._data[-2:]
