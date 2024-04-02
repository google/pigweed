// Copyright 2024 The Pigweed Authors
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

#include "pw_assert/assert.h"
#include "pw_preprocessor/compiler.h"

namespace pw::allocator {

/// Describes the layout of a block of memory.
///
/// Layouts are passed to allocators, and consist of a (possibly padded) size
/// and a power-of-two alignment no larger than the size. Layouts can be
/// constructed for a type `T` using `Layout::Of`.
///
/// Example:
///
/// @code{.cpp}
///    struct MyStruct {
///      uint8_t field1[3];
///      uint32_t field2[3];
///    };
///    constexpr Layout layout_for_struct = Layout::Of<MyStruct>();
/// @endcode
class Layout {
 public:
  constexpr Layout() = default;
  constexpr Layout(size_t size, size_t alignment = alignof(std::max_align_t))
      : size_(size), alignment_(alignment) {}

  /// Creates a Layout for the given type.
  template <typename T>
  static constexpr Layout Of() {
    return Layout(sizeof(T), alignof(T));
  }

  constexpr Layout Extend(size_t size) const {
    PW_ASSERT(!PW_ADD_OVERFLOW(size, size_, &size));
    return Layout(size, alignment_);
  }

  size_t size() const { return size_; }
  size_t alignment() const { return alignment_; }

 private:
  size_t size_ = 0;
  size_t alignment_ = 1;
};

inline bool operator==(const Layout& lhs, const Layout& rhs) {
  return lhs.size() == rhs.size() && lhs.alignment() == rhs.alignment();
}

inline bool operator!=(const Layout& lhs, const Layout& rhs) {
  return !(lhs == rhs);
}

}  // namespace pw::allocator
