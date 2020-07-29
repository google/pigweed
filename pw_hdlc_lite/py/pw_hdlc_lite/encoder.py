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
"""Encoder functions for encoding bytes using HDLC-Lite protocol"""

import binascii

from pw_hdlc_lite import constants

_HDLC_ESCAPE = bytes([constants.HDLC_ESCAPE])
_HDLC_FRAME_DELIMITER = bytes([constants.HDLC_FRAME_DELIMITER])


def encode_and_write_payload(payload, write):
    """Escapes the payload and writes the data-frame to the serial device."""

    write(_HDLC_FRAME_DELIMITER)

    # The crc_hqx function computes the 2-byte CCITT-CRC16 value
    crc = binascii.crc_hqx(payload, 0xFFFF).to_bytes(2, byteorder='little')
    payload += crc
    payload = payload.replace(_HDLC_ESCAPE, b'\x7D\x5D')
    payload = payload.replace(_HDLC_FRAME_DELIMITER, b'\x7D\x5E')
    write(payload)

    write(_HDLC_FRAME_DELIMITER)
