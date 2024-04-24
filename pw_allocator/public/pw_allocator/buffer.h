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

#include <array>
#include <cstddef>
#include <cstdint>

#include "pw_bytes/span.h"
#include "pw_result/result.h"

namespace pw::allocator {

/// Associates a default-constructed type with a memory buffer.
///
/// Although the type is arbitrary, the intended purpose of of this class is to
/// provide allocators with memory to use, e.g. when testing.
///
/// This class uses composition instead of inheritance in order to allow the
/// wrapped type's destructor to reference the memory without risk of a
/// use-after-free. As a result, the specific methods of the wrapped type
/// are not directly accesible. Instead, they can be accessed using the `*` and
/// `->` operators, e.g.
///
/// @code{.cpp}
/// WithBuffer<MyAllocator, 256> allocator;
/// allocator->MethodSpecificToMyAllocator();
/// @endcode
///
/// Note that this class does NOT initialize the allocator, since initialization
/// is not specified as part of the `Allocator` interface and may vary from
/// allocator to allocator. As a result, typical usage includes deriving a class
/// that initializes the wrapped allocator with the buffer in a constructor. See
/// `AllocatorForTest` for an example.
///
/// @tparam   T             The wrapped object.
/// @tparam   kBufferSize   The size of the backing memory, in bytes.
/// @tparam   kAlignment    Buffer memory will be aligned to this alignment
///                         boundary.
template <typename T, size_t kBufferSize, size_t kAlignment = 1>
class WithBuffer {
 public:
  static constexpr size_t kCapacity = kBufferSize;

  ByteSpan as_bytes() { return buffer_; }
  std::byte* data() { return buffer_.data(); }
  size_t size() const { return buffer_.size(); }

  T& operator*() { return obj_; }
  const T& operator*() const { return obj_; }

  T* operator->() { return &obj_; }
  const T* operator->() const { return &obj_; }

 private:
  alignas(kAlignment) std::array<std::byte, kBufferSize> buffer_;
  T obj_;
};

/// Returns the largest aligned subspan of a given byte span.
///
/// The subspan will start and end on alignment boundaries.
///
/// @returns @rst
///
/// .. pw-status-codes::
///
///    OK: Returns the aligned subspan.
///
///    RESOURCE_EXHAUSTED: The given span does not contain an alignment
///    boundary.
///
/// @endrst
Result<ByteSpan> GetAlignedSubspan(ByteSpan bytes, size_t alignment);

/// Returns whether one region is completely contained within another.
///
/// @param[in]  ptr     Points to the start of the subregion.
/// @param[in]  layout  Describes the subregion.
/// @param[in]  outer   The memory that may contain the subregion.
bool IsWithin(const void* ptr, size_t size, ConstByteSpan outer);

}  // namespace pw::allocator
