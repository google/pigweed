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
#include <cstdint>

#include "pw_allocator/capability.h"
#include "pw_allocator/deallocator.h"
#include "pw_allocator/layout.h"
#include "pw_assert/assert.h"
#include "pw_bytes/span.h"
#include "pw_result/result.h"

namespace pw::allocator {

/// Abstract interface for fixed-layout memory allocation.
///
/// The interface makes no guarantees about its implementation. Consumers of the
/// generic interface must not make any assumptions around allocator behavior,
/// thread safety, or performance.
class Pool : public Deallocator {
 public:
  constexpr Pool(const Capabilities& capabilities, const Layout& layout)
      : Deallocator(capabilities), layout_(layout) {}

  constexpr const Layout& layout() const { return layout_; }

  /// Returns a chunk of memory with this object's fixed layout.
  ///
  /// Like `pw::allocator::Allocate`, returns null if memory is exhausted.
  ///
  /// @retval The allocated memory.
  void* Allocate() { return DoAllocate(); }

  /// Allocates and constructs an object.
  ///
  /// This method is similar to ``Allocator::New``, except that it is specific
  /// to the pool's layout.
  ///
  /// @tparam   T   Type of object to allocate. Either `Layout::Of<T>` must
  ///               match the pool's layout, or `T` must be an unbounded array
  ///               whose elements a size and alignment that evenly divide the
  ///               pool's layout's size and alignment respectively.
  ///
  /// @{
  template <typename T,
            int&... kExplicitGuard,
            std::enable_if_t<!std::is_array_v<T>, int> = 0,
            typename... Args>
  [[nodiscard]] T* New(Args&&... args) {
    PW_ASSERT(Layout::Of<T>() == layout_);
    void* ptr = Allocate();
    return ptr != nullptr ? new (ptr) T(std::forward<Args>(args)...) : nullptr;
  }

  template <typename T,
            int&... kExplicitGuard,
            typename ElementType = std::remove_extent_t<T>,
            std::enable_if_t<is_bounded_array_v<T>, int> = 0>
  [[nodiscard]] ElementType* New() {
    return NewArray<ElementType>(std::extent_v<T>);
  }

  template <typename T,
            int&... kExplicitGuard,
            typename ElementType = std::remove_extent_t<T>,
            std::enable_if_t<is_unbounded_array_v<T>, int> = 0>
  [[nodiscard]] ElementType* New() {
    return NewArray<ElementType>(layout_.size() / sizeof(ElementType));
  }
  /// @}

  /// Constructs an object and wraps it in a `UniquePtr`
  ///
  /// This method is similar to ``Allocator::MakeUnique``, except that it is
  /// specific to the pool's layout.
  ///
  /// @tparam   T   Type of object to allocate. Either `Layout::Of<T>` must
  ///               match the pool's layout, or `T` must be an unbounded array
  ///               whose elements a size and alignment that evenly divide the
  ///               pool's layout's size and alignment respectively.
  ///
  /// @{
  template <typename T,
            int&... kExplicitGuard,
            std::enable_if_t<!std::is_array_v<T>, int> = 0,
            typename... Args>
  UniquePtr<T> MakeUnique(Args&&... args) {
    return UniquePtr<T>(New<T>(std::forward<Args>(args)...), *this);
  }

  template <typename T,
            int&... kExplicitGuard,
            std::enable_if_t<is_bounded_array_v<T>, int> = 0>
  UniquePtr<T> MakeUnique() {
    using ElementType = std::remove_extent_t<T>;
    return UniquePtr<T>(NewArray<ElementType>(std::extent_v<T>), *this);
  }

  template <typename T,
            int&... kExplicitGuard,
            std::enable_if_t<is_unbounded_array_v<T>, int> = 0>
  UniquePtr<T> MakeUnique() {
    using ElementType = std::remove_extent_t<T>;
    size_t size = layout_.size() / sizeof(ElementType);
    return UniquePtr<T>(NewArray<ElementType>(size), size, *this);
  }
  /// @}

 private:
  /// Virtual `Allocate` function that can be overridden by derived classes.
  virtual void* DoAllocate() = 0;

  // Helper to create arrays.
  template <typename ElementType>
  [[nodiscard]] ElementType* NewArray(size_t count) {
    Layout layout = Layout::Of<ElementType[]>(count);
    PW_ASSERT(layout.size() == layout_.size());
    PW_ASSERT(layout.alignment() <= layout_.alignment());
    void* ptr = DoAllocate();
    return ptr != nullptr ? new (ptr) ElementType[count] : nullptr;
  }

  const Layout layout_;
};

}  // namespace pw::allocator
