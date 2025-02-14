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

#include "pw_allocator/capability.h"
#include "pw_allocator/deallocator.h"
#include "pw_allocator/layout.h"
#include "pw_allocator/unique_ptr.h"
#include "pw_numeric/checked_arithmetic.h"
#include "pw_result/result.h"

namespace pw {

/// Abstract interface for variable-layout memory allocation.
///
/// The interface makes no guarantees about its implementation. Consumers of the
/// generic interface must not make any assumptions around allocator behavior,
/// thread safety, or performance.
class Allocator : public Deallocator {
 public:
  /// Allocates a block of memory with the specified size and alignment.
  ///
  /// Returns `nullptr` if the allocation cannot be made, or the `layout` has a
  /// size of 0.
  ///
  /// @param[in]  layout      Describes the memory to be allocated.
  void* Allocate(Layout layout) {
    return layout.size() != 0 ? DoAllocate(layout) : nullptr;
  }

  /// Constructs an object of type `T` from the given `args`
  ///
  /// The return value is nullable, as allocating memory for the object may
  /// fail. Callers must check for this error before using the resulting
  /// pointer.
  ///
  /// @param[in]  args...     Arguments passed to the object constructor.
  template <typename T, int&... kExplicitGuard, typename... Args>
  T* New(Args&&... args) {
    void* ptr = Allocate(Layout::Of<T>());
    if (ptr == nullptr) {
      return nullptr;
    }
    return new (ptr) T(std::forward<Args>(args)...);
  }

  /// Constructs an array of `count` objects of type `T`
  ///
  /// The return value is nullable, as allocating memory for the object may
  /// fail. Callers must check for this error before using the resulting
  /// pointer.
  ///
  /// @param[in]  count        Number of objects to allocate.
  template <typename T>
  T* NewArray(size_t count) {
    return NewArray<T>(count, alignof(T));
  }

  /// Constructs an `alignment`-byte aligned array of `count` objects of type
  /// `T`
  ///
  /// The return value is nullable, as allocating memory for the object may
  /// fail. Callers must check for this error before using the resulting
  /// pointer.
  ///
  /// @param[in]  count        Number of objects to allocate.
  template <typename T>
  T* NewArray(size_t count, size_t alignment) {
    std::optional<size_t> size = CheckedMul<size_t>(sizeof(T), count);
    if (!size.has_value()) {
      return nullptr;
    }
    Layout layout(size.value(), alignment);
    void* ptr = Allocate(layout);
    return ptr != nullptr ? new (ptr) T[count] : nullptr;
  }

  /// Constructs and object of type `T` from the given `args`, and wraps it in a
  /// `UniquePtr`
  ///
  /// The returned value may contain null if allocating memory for the object
  /// fails. Callers must check for null before using the `UniquePtr`.
  ///
  /// @param[in]  args...     Arguments passed to the object constructor.
  template <typename T, int&... kExplicitGuard, typename... Args>
  [[nodiscard]] UniquePtr<T> MakeUnique(Args&&... args) {
    return Deallocator::WrapUnique<T>(New<T>(std::forward<Args>(args)...));
  }

  /// Constructs an array of `count` objects of type `T`, and wraps it in a
  /// `UniquePtr`
  ///
  /// The returned value may contain null if allocating memory for the object
  /// fails. Callers must check for null before using the `UniquePtr`.
  ///
  /// @param[in]  count        Number of objects to allocate.
  template <typename T>
  [[nodiscard]] UniquePtr<T[]> MakeUniqueArray(size_t count) {
    return Deallocator::WrapUniqueArray<T>(NewArray<T>(count), count);
  }

  /// Constructs an `alignment`-byte aligned array of `count` objects of type
  /// `T`, and wraps it in a `UniquePtr`
  ///
  /// The returned value may contain null if allocating memory for the object
  /// fails. Callers must check for null before using the `UniquePtr`.
  ///
  /// @param[in]  count        Number of objects to allocate.
  template <typename T>
  [[nodiscard]] UniquePtr<T[]> MakeUniqueArray(size_t count, size_t alignment) {
    return Deallocator::WrapUniqueArray<T>(NewArray<T>(count, alignment),
                                           count);
  }

  /// Modifies the size of an previously-allocated block of memory without
  /// copying any data.
  ///
  /// Returns true if its size was changed without copying data to a new
  /// allocation; otherwise returns false.
  ///
  /// In particular, it always returns true if the `old_layout.size()` equals
  /// `new_size`, and always returns false if the given pointer is null, the
  /// `old_layout.size()` is 0, or the `new_size` is 0.
  ///
  /// @param[in]  ptr           Pointer to previously-allocated memory.
  /// @param[in]  new_size      Requested new size for the memory allocation.
  bool Resize(void* ptr, size_t new_size) {
    return ptr != nullptr && new_size != 0 && DoResize(ptr, new_size);
  }

  /// Deprecated version of `Resize` that takes a `Layout`.
  /// Do not use this method. It will be removed.
  /// TODO(b/326509341): Remove when downstream consumers migrate.
  bool Resize(void* ptr, Layout layout, size_t new_size) {
    return ptr != nullptr && new_size != 0 && DoResize(ptr, layout, new_size);
  }

  /// Modifies the size of a previously-allocated block of memory.
  ///
  /// Returns pointer to the modified block of memory, or `nullptr` if the
  /// memory could not be modified.
  ///
  /// The data stored by the memory being modified must be trivially
  /// copyable. If it is not, callers should themselves attempt to `Resize`,
  /// then `Allocate`, move the data, and `Deallocate` as needed.
  ///
  /// If `nullptr` is returned, the block of memory is unchanged. In particular,
  /// if the `new_layout` has a size of 0, the given pointer will NOT be
  /// deallocated.
  ///
  /// TODO(b/331290408): This error condition needs to be better communicated to
  /// module users, who may assume the pointer is freed.
  ///
  /// Unlike `Resize`, providing a null pointer will return a new allocation.
  ///
  /// If the request can be satisfied using `Resize`, the `alignment` parameter
  /// may be ignored.
  ///
  /// @param[in]  ptr         Pointer to previously-allocated memory.
  /// @param[in]  new_layout  Describes the memory to be allocated.
  void* Reallocate(void* ptr, Layout new_layout) {
    if (new_layout.size() == 0) {
      return nullptr;
    }
    if (ptr == nullptr) {
      return Allocate(new_layout);
    }
    return DoReallocate(ptr, new_layout);
  }

  /// Deprecated version of `Reallocate` that takes a `Layout`.
  /// Do not use this method. It will be removed.
  /// TODO(b/326509341): Remove when downstream consumers migrate.
  void* Reallocate(void* ptr, Layout old_layout, size_t new_size) {
    if (new_size == 0) {
      return nullptr;
    }
    if (ptr == nullptr) {
      return Allocate(Layout(new_size, old_layout.alignment()));
    }
    return DoReallocate(ptr, old_layout, new_size);
  }

  /// Returns the total bytes that have been allocated by this allocator, or
  /// `size_t(-1)` if this allocator does not track its total allocated bytes.
  size_t GetAllocated() const { return DoGetAllocated(); }

 protected:
  /// TODO(b/326509341): Remove when downstream consumers migrate.
  constexpr Allocator() = default;

  explicit constexpr Allocator(const Capabilities& capabilities)
      : Deallocator(capabilities) {}

 private:
  /// Virtual `Allocate` function implemented by derived classes.
  ///
  /// @param[in]  layout        Describes the memory to be allocated. Guaranteed
  ///                           to have a non-zero size.
  virtual void* DoAllocate(Layout layout) = 0;

  /// Virtual `Resize` function implemented by derived classes.
  ///
  /// The default implementation simply returns `false`, indicating that
  /// resizing is not supported.
  ///
  /// @param[in]  ptr           Pointer to memory, guaranteed to not be null.
  /// @param[in]  new_size      Requested size, guaranteed to be non-zero..
  virtual bool DoResize(void* /*ptr*/, size_t /*new_size*/) { return false; }

  /// Deprecated version of `DoResize` that takes a `Layout`.
  /// Do not use this method. It will be removed.
  /// TODO(b/326509341): Remove when downstream consumers migrate.
  virtual bool DoResize(void*, Layout, size_t) { return false; }

  /// Virtual `Reallocate` function that can be overridden by derived classes.
  ///
  /// The default implementation will first try to `Resize` the data. If that is
  /// unsuccessful, it will allocate an entirely new block, copy existing data,
  /// and deallocate the given block.
  ///
  /// @param[in]  ptr           Pointer to memory, guaranteed to not be null.
  /// @param[in]  new_layout    Describes the memory to be allocated. Guaranteed
  ///                           to have a non-zero size.
  virtual void* DoReallocate(void* ptr, Layout new_layout);

  /// Deprecated version of `DoReallocate` that takes a `Layout`.
  /// Do not use this method. It will be removed.
  /// TODO(b/326509341): Remove when downstream consumers migrate.
  virtual void* DoReallocate(void* ptr, Layout old_layout, size_t new_size);

  /// Virtual `GetAllocated` function that can be overridden by derived classes.
  ///
  /// The default implementation simply returns `size_t(-1)`, indicating that
  /// tracking total allocated bytes is not supported.
  virtual size_t DoGetAllocated() const { return size_t(-1); }
};

namespace allocator {

// Alias for module consumers using the older name for the above type.
using Allocator = ::pw::Allocator;

}  // namespace allocator
}  // namespace pw
