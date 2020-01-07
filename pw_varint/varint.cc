// Copyright 2019 The Pigweed Authors
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#include "pw_varint/varint.h"

#include <algorithm>

namespace pw::varint {

size_t EncodeLittleEndianBase128(uint64_t integer,
                                 const span<std::byte>& output) {
  size_t written = 0;
  do {
    if (written >= output.size()) {
      return 0;
    }

    // Grab 7 bits; the eighth bit is set to 1 to indicate more data coming.
    output[written++] = static_cast<std::byte>(integer) | std::byte{0x80};
    integer >>= 7;
  } while (integer != 0u);

  output[written - 1] &= std::byte{0x7f};  // clear the top bit of the last byte
  return written;
}

size_t Decode(const span<const std::byte>& input, int64_t* value) {
  const size_t bytes = Decode(input, reinterpret_cast<uint64_t*>(value));
  *value = ZigZagDecode(static_cast<uint64_t>(*value));
  return bytes;
}

size_t Decode(const span<const std::byte>& input, uint64_t* value) {
  uint64_t decoded_value = 0;
  uint_fast8_t count = 0;

  // The largest 64-bit ints require 10 B.
  const size_t max_count = std::min(kMaxVarintSizeBytes, input.size());

  while (true) {
    if (count >= max_count) {
      return 0;
    }

    // Add the bottom seven bits of the next byte to the result.
    decoded_value |= static_cast<uint64_t>(input[count] & std::byte{0x7f})
                     << (7 * count);

    // Stop decoding if the top bit is not set.
    if ((input[count++] & std::byte{0x80}) == std::byte{0}) {
      break;
    }
  }

  *value = decoded_value;
  return count;
}

}  // namespace pw::varint
