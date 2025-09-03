// Copyright 2023 The Pigweed Authors
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

#include "lib/stdcompat/bit.h"
#include "pw_assert/assert.h"
#include "pw_bytes/span.h"
#include "pw_numeric/checked_arithmetic.h"

namespace pw {

/// @submodule{pw_bytes,align}

/// Returns whether the given pointer meets the given alignment requirement.
inline bool IsAlignedAs(const void* ptr, size_t alignment) {
  return (cpp20::bit_cast<uintptr_t>(ptr) % alignment) == 0;
}

/// Returns whether the given pointer meets the alignment requirement for the
/// given type.
template <typename T>
bool IsAlignedAs(const void* ptr) {
  return IsAlignedAs(ptr, alignof(T));
}

/// Returns the value rounded down to the nearest multiple of alignment.
constexpr size_t AlignDown(uintptr_t value, size_t alignment) {
  PW_ASSERT(CheckedMul((value / alignment), alignment, value));
  return value;
}

/// Returns the value rounded down to the nearest multiple of alignment.
template <typename T>
constexpr T* AlignDown(T* value, size_t alignment) {
  return reinterpret_cast<T*>(
      AlignDown(reinterpret_cast<uintptr_t>(value), alignment));
}

/// Returns the value rounded up to the nearest multiple of alignment.
constexpr size_t AlignUp(uintptr_t value, size_t alignment) {
  PW_ASSERT(CheckedIncrement(value, alignment - 1));
  return AlignDown(value, alignment);
}

/// Returns the value rounded up to the nearest multiple of alignment.
template <typename T>
constexpr T* AlignUp(T* value, size_t alignment) {
  return reinterpret_cast<T*>(
      AlignUp(reinterpret_cast<uintptr_t>(value), alignment));
}

/// Returns the number of padding bytes required to align the provided length.
constexpr size_t Padding(size_t length, size_t alignment) {
  return AlignUp(length, alignment) - length;
}

/// Returns the largest aligned subspan of a given byte span.
///
/// The subspan will start and end on alignment boundaries.
///
/// @returns A `ByteSpan` within `bytes` aligned to `alignment`, or an empty
///   `ByteSpan` if alignment was not possible.
ByteSpan GetAlignedSubspan(ByteSpan bytes, size_t alignment);

/// @}

}  // namespace pw
