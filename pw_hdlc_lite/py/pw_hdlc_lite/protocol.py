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
"""Module for low-level HDLC protocol features."""

import zlib

# Special flag character for delimiting HDLC frames.
FLAG = 0x7E

# Special character for escaping other special characters in a frame.
ESCAPE = 0x7D


def escape(byte: int) -> int:
    """Escapes or unescapes a byte, which should have been preceeded by 0x7d."""
    return byte ^ 0x20


def frame_check_sequence(data: bytes) -> bytes:
    return zlib.crc32(data).to_bytes(4, 'little')
