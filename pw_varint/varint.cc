// Copyright 2020 The Pigweed Authors
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

namespace pw {
namespace varint {

extern "C" size_t pw_VarintEncode(uint64_t integer,
                                  void* output,
                                  size_t output_size) {
  size_t written = 0;
  std::byte* buffer = static_cast<std::byte*>(output);

  do {
    if (written >= output_size) {
      return 0;
    }

    // Grab 7 bits; the eighth bit is set to 1 to indicate more data coming.
    buffer[written++] = static_cast<std::byte>(integer) | std::byte(0x80);
    integer >>= 7;
  } while (integer != 0u);

  buffer[written - 1] &= std::byte(0x7f);  // clear the top bit of the last byte
  return written;
}

extern "C" size_t pw_VarintZigZagEncode(int64_t integer,
                                        void* output,
                                        size_t output_size) {
  return pw_VarintEncode(ZigZagEncode(integer), output, output_size);
}

extern "C" size_t pw_VarintDecode(const void* input,
                                  size_t input_size,
                                  uint64_t* output) {
  uint64_t decoded_value = 0;
  uint_fast8_t count = 0;
  const std::byte* buffer = static_cast<const std::byte*>(input);

  // The largest 64-bit ints require 10 B.
  const size_t max_count = std::min(kMaxVarint64SizeBytes, input_size);

  while (true) {
    if (count >= max_count) {
      return 0;
    }

    // Add the bottom seven bits of the next byte to the result.
    decoded_value |= static_cast<uint64_t>(buffer[count] & std::byte(0x7f))
                     << (7 * count);

    // Stop decoding if the top bit is not set.
    if ((buffer[count++] & std::byte(0x80)) == std::byte(0)) {
      break;
    }
  }

  *output = decoded_value;
  return count;
}

extern "C" size_t pw_VarintZigZagDecode(const void* input,
                                        size_t input_size,
                                        int64_t* output) {
  uint64_t value = 0;
  size_t bytes = pw_VarintDecode(input, input_size, &value);
  *output = ZigZagDecode(value);
  return bytes;
}

extern "C" size_t pw_VarintEncodedSize(uint64_t integer) {
  return EncodedSize(integer);
}

extern "C" size_t pw_VarintZigZagEncodedSize(int64_t integer) {
  return ZigZagEncodedSize(integer);
}

}  // namespace varint
}  // namespace pw
