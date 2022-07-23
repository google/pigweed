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
"""Provides functionality for encoding tokenized messages."""

import base64
import struct
from typing import Union

_INT32_MAX = 2**31 - 1
_UINT32_MAX = 2**32 - 1
BASE64_PREFIX = '$'


def _zig_zag_encode(value: int) -> int:
    """Encodes signed integers to give a compact varint encoding."""
    return value << 1 if value >= 0 else (value << 1) ^ (~0)


def _little_endian_base128_encode(integer: int) -> bytearray:
    data = bytearray()

    while True:
        # Grab 7 bits; the eighth bit is set to 1 to indicate more data coming.
        data.append((integer & 0x7f) | 0x80)
        integer >>= 7

        if not integer:
            break

    data[-1] &= 0x7f  # clear the top bit of the last byte
    return data


def _encode_int32(arg: int) -> bytearray:
    # Convert large unsigned numbers into their corresponding signed values.
    if arg > _INT32_MAX:
        arg -= 2**32

    return _little_endian_base128_encode(_zig_zag_encode(arg))


def _encode_string(arg: bytes) -> bytes:
    size_byte = len(arg) if len(arg) < 128 else 0xff
    return struct.pack('B', size_byte) + arg[:127]


def encode_token_and_args(token: int, *args: Union[int, float, bytes,
                                                   str]) -> bytes:
    """Encodes a tokenized message given its token and arguments.

    This function assumes that the token represents a format string with
    conversion specifiers that correspond with the provided argument types.
    Currently, only 32-bit integers are supported.
    """

    if token < 0 or token > _UINT32_MAX:
        raise ValueError(
            f'The token ({token}) must be an unsigned 32-bit integer')

    data = bytearray(struct.pack('<I', token))

    for arg in args:
        if isinstance(arg, int):
            if arg.bit_length() > 32:
                raise ValueError(
                    f'Cannot encode {arg}: only 32-bit integers may be encoded'
                )
            data += _encode_int32(arg)
        elif isinstance(arg, float):
            data += struct.pack('<f', arg)
        elif isinstance(arg, str):
            data += _encode_string(arg.encode())
        elif isinstance(arg, bytes):
            data += _encode_string(arg)
        else:
            raise ValueError(
                f'{arg} has type {type(arg)}, which is not supported')

    return bytes(data)


def prefixed_base64(data: bytes, prefix: str = '$') -> str:
    """Encodes a tokenized message as prefixed Base64."""
    return prefix + base64.b64encode(data).decode()
