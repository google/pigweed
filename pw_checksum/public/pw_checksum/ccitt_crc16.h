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

// Provides an implementation of the CCITT CRC16 for the polynomial
//
//   x^16 + x^12 + x^5 + 1
//
// Polynomial 0x1021, initial value 0xFFFF. See https://www.zlib.net/crc_v3.txt.
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// C API for calculating the CCITT CRC16 of an array of data.
uint16_t pw_ChecksumCcittCrc16(const void* data,
                               size_t size_bytes,
                               uint16_t initial_value);

#ifdef __cplusplus
}  // extern "C"

#include <span>

namespace pw::checksum {

inline constexpr uint16_t kCcittCrc16DefaultInitialValue = 0xFFFF;

// Calculates the CCITT CRC16 for the provided data. To update an existing CRC,
// pass the previous value as the initial_value argument.
inline uint16_t CcittCrc16(
    std::span<const std::byte> data,
    uint16_t initial_value = kCcittCrc16DefaultInitialValue) {
  return pw_ChecksumCcittCrc16(data.data(), data.size_bytes(), initial_value);
}

// Calculates the CCITT CRC16 for a single byte. This is useful for updating a
// CRC byte-by-byte.
inline uint16_t CcittCrc16(
    std::byte value, uint16_t initial_value = kCcittCrc16DefaultInitialValue) {
  return pw_ChecksumCcittCrc16(&value, sizeof(value), initial_value);
}

}  // namespace pw::checksum

#endif  // __cplusplus
