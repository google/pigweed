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

#include "pw_allocator/hardening.h"
#include "pw_result/result.h"

namespace pw::allocator {
namespace internal {

// Helper variables to determine when a template parameter is an array type.
// Based on the sample implementation found at
// https://en.cppreference.com/w/cpp/memory/unique_ptr/make_unique.
template <typename>
constexpr bool is_unbounded_array_v = false;

template <typename T>
constexpr bool is_unbounded_array_v<T[]> = true;

template <typename>
constexpr bool is_bounded_array_v = false;

template <typename T, size_t kN>
constexpr bool is_bounded_array_v<T[kN]> = true;

}  // namespace internal

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
  constexpr Layout() : Layout(0) {}
  constexpr explicit Layout(size_t size)
      : Layout(size, alignof(std::max_align_t)) {}
  constexpr Layout(size_t size, size_t alignment)
      : size_(size), alignment_(alignment) {}

  /// Creates a Layout for the given type.
  template <typename T>
  static constexpr std::enable_if_t<!std::is_array_v<T>, Layout> Of() {
    return Layout(sizeof(T), alignof(T));
  }

  /// Creates a Layout for the given bounded array type, e.g. Foo[kN].
  template <typename T>
  static constexpr std::enable_if_t<internal::is_bounded_array_v<T>, Layout>
  Of() {
    return Layout(sizeof(T), alignof(std::remove_extent_t<T>));
  }

  /// Creates a Layout for the given array type, e.g. Foo[].
  template <typename T>
  static constexpr std::enable_if_t<internal::is_unbounded_array_v<T>, Layout>
  Of(size_t count) {
    using U = std::remove_extent_t<T>;
    size_t size = sizeof(U);
    Hardening::Multiply(size, count);
    return Layout(size, alignof(U));
  }

  /// If the result is okay, returns its contained layout; otherwise, returns a
  /// default layout.
  static constexpr Layout Unwrap(const Result<Layout>& result) {
    return result.ok() ? (*result) : Layout();
  }

  constexpr Layout Extend(size_t size) const {
    Hardening::Increment(size, size_);
    return Layout(size, alignment_);
  }

  constexpr Layout Align(size_t alignment) const {
    return Layout(size_, std::max(alignment, alignment_));
  }

  constexpr size_t size() const { return size_; }
  constexpr size_t alignment() const { return alignment_; }

 private:
  size_t size_;
  size_t alignment_;
};

inline bool operator==(const Layout& lhs, const Layout& rhs) {
  return lhs.size() == rhs.size() && lhs.alignment() == rhs.alignment();
}

inline bool operator!=(const Layout& lhs, const Layout& rhs) {
  return !(lhs == rhs);
}

}  // namespace pw::allocator
