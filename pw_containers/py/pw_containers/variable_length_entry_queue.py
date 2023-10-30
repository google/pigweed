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
"""Decodes the in-memory representation of a sized-entry ring buffer."""

import struct
from typing import Iterable, Tuple

_HEADER = struct.Struct('III')  # data_size_bytes, head, tail


def _decode_leb128(
    data: bytes, offset: int = 0, max_bits: int = 32
) -> Tuple[int, int]:
    count = value = shift = 0

    while offset < len(data):
        byte = data[offset]

        count += 1
        value |= (byte & 0x7F) << shift

        if not byte & 0x80:
            return offset + count, value

        shift += 7
        if shift >= max_bits:
            raise ValueError(f'Varint exceeded {max_bits}-bit limit')

    raise ValueError(f'Unterminated varint {data[offset:]!r}')


def parse(queue: bytes) -> Iterable[bytes]:
    """Decodes the in-memory representation of a variable-length entry queue.

    Args:
      queue: The bytes representation of a variable-length entry queue.

    Yields:
      Each entry in the buffer as bytes.
    """
    array_size_bytes, head, tail = _HEADER.unpack_from(queue)

    total_encoded_size = _HEADER.size + array_size_bytes
    if len(queue) < total_encoded_size:
        raise ValueError(
            f'Ring buffer data ({len(queue)} B) is smaller than the encoded '
            f'size ({total_encoded_size} B)'
        )

    data = queue[_HEADER.size : total_encoded_size]

    if tail < head:
        data = data[head:] + data[:tail]
    else:
        data = data[head:tail]

    index = 0
    while index < len(data):
        index, size = _decode_leb128(data, index)

        if index + size > len(data):
            raise ValueError(
                f'Corruption detected; '
                f'encoded size {size} B is too large for a {len(data)} B array'
            )
        yield data[index : index + size]

        index += size
