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
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

#define PW_CHECKSUM_CRC32_INITIAL_VALUE 0xFFFFFFFFu

// C API for calculating the CRC32 of an array of data.

// Updates an existing CRC value. The previous_result must have been returned
// from a previous CRC32 call; it is not used as the initial value.
uint32_t pw_ChecksumCrc32Append(const void* data,
                                size_t size_bytes,
                                uint32_t previous_result);

// Starts calculating a CRC32 for the provided data.
static inline uint32_t pw_ChecksumCrc32(const void* data, size_t size_bytes) {
  return pw_ChecksumCrc32Append(
      data, size_bytes, ~PW_CHECKSUM_CRC32_INITIAL_VALUE);
}

#ifdef __cplusplus
}  // extern "C"

#include <span>

namespace pw::checksum {

inline constexpr uint32_t kCrc32InitialValue = PW_CHECKSUM_CRC32_INITIAL_VALUE;

// Starts calculating a CRC32 with the provided data. Uses kCrc32InitialValue as
// the initial value.
inline uint32_t Crc32(std::span<const std::byte> data) {
  return pw_ChecksumCrc32(data.data(), data.size_bytes());
}

// Updates an existing CRC value. The previous_result must have been returned
// from a previous CRC32 call; it is not used as the initial value.
inline uint32_t Crc32(std::span<const std::byte> data,
                      uint32_t previous_result) {
  return pw_ChecksumCrc32Append(
      data.data(), data.size_bytes(), previous_result);
}

// Starts calculating a CRC32 with the provided byte. Uses kCrc32InitialValue as
// the initial value.
inline uint32_t Crc32(std::byte value) {
  return pw_ChecksumCrc32(&value, sizeof(value));
}

// Updates an existing CRC value. The previous_result must have been returned
// from a previous CRC32 call; it is not used as the initial value.
inline uint32_t Crc32(std::byte value, uint32_t previous_result) {
  return pw_ChecksumCrc32Append(&value, sizeof(value), previous_result);
}

}  // namespace pw::checksum
#endif  // __cplusplus
