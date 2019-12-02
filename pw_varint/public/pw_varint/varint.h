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
#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "pw_span/span.h"

namespace pw::varint {

// The maximum number of bytes occupied by an encoded varint. The maximum
// uint64_t occupies 10 bytes when encoded.
inline constexpr size_t kMaxVarintSizeBytes = 10;

// ZigZag encodes a signed integer. This maps small negative numbers to small,
// unsigned positive numbers, which improves their density for LEB128 encoding.
//
// ZigZag encoding works by moving the sign bit from the most-significant bit to
// the least-significant bit. For the signed k-bit integer n, the formula is
//
//   (n << 1) ^ (n >> (k - 1))
//
// See the following for a description of ZigZag encoding:
//   https://developers.google.com/protocol-buffers/docs/encoding#types
template <typename T>
constexpr std::make_unsigned_t<T> ZigZagEncode(T n) {
  static_assert(std::is_signed<T>(), "Zig-zag encoding is for signed integers");
  using U = std::make_unsigned_t<T>;
  return (static_cast<U>(n) << 1) ^ static_cast<U>(n >> (sizeof(T) * 8 - 1));
}

// Encodes a uint64_t with Little-Endian Base 128 (LEB128) encoding.
size_t EncodeLittleEndianBase128(uint64_t integer, const span<uint8_t>& output);

// Encodes the provided integer using a variable-length encoding and returns the
// number of bytes written.
//
// The encoding is the same as used in protocol buffers. Signed integers are
// ZigZag encoded to remove leading 1s from small negative numbers, then the
// resulting number is encoded as Little Endian Base 128 (LEB128). Unsigned
// integers are encoded directly as LEB128.
//
// Returns the number of bytes written or 0 if the result didn't fit in the
// encoding buffer.
template <typename T>
size_t EncodeVarint(T integer, const span<uint8_t>& output) {
  if constexpr (std::is_signed<T>()) {
    return EncodeLittleEndianBase128(ZigZagEncode(integer), output);
  } else {
    return EncodeLittleEndianBase128(integer, output);
  }
}

// Decodes a varint-encoded value. If reading into a signed integer, the value
// is ZigZag decoded.
//
// Returns the number of bytes read from the input if successful. Returns zero
// if the result does not fit in a int64_t / uint64_t or if the input is
// exhausted before the number terminates. Reads a maximum of 10 bytes.
//
// The following example decodes multiple varints from a buffer:
//
//   while (!data.empty()) {
//     int64_t value;
//     size_t bytes = DecodeVarint(data, &value);
//
//     if (bytes == 0u) {
//       return Status::DATA_LOSS;
//     }
//     results.push_back(value);
//     data = data.subspan(bytes)
//   }
//
size_t DecodeVarint(const span<const uint8_t>& input, int64_t* value);
size_t DecodeVarint(const span<const uint8_t>& input, uint64_t* value);

}  // namespace pw::varint
