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
#include <optional>

#include "pw_status/status.h"

namespace pw::allocator {
/// Describes the layout of a block of memory.
///
/// Layouts are passed to allocators, and consist of a size and a power-of-two
/// alignment. Layouts can be constructed for a type `T` using `Layout::Of`.
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
  /// Creates a Layout for the given type.
  template <typename T>
  static constexpr Layout Of() {
    return Layout(sizeof(T), alignof(T));
  }

  size_t size() const { return size_; }
  size_t alignment() const { return alignment_; }

 private:
  constexpr Layout(size_t size, size_t alignment)
      : size_(size), alignment_(alignment) {}

  size_t size_;
  size_t alignment_;
};

/// Abstract interface for memory allocation.
///
/// This is the most generic and fundamental interface provided by the
/// module, representing any object capable of dynamic memory allocation. Other
/// interfaces may inherit from a base generic Allocator and provide different
/// allocator properties.
///
/// The interface makes no guarantees about its implementation. Consumers of the
/// generic interface must not make any assumptions around allocator behavior,
/// thread safety, or performance.
///
/// NOTE: This interface is in development and should not be considered stable.
class Allocator {
 public:
  constexpr Allocator() = default;
  virtual ~Allocator() = default;

  /// Asks the allocator if it is capable of realloating or deallocating a given
  /// pointer.
  ///
  /// NOTE: This method is in development and should not be considered stable.
  /// Do NOT use it in its current form to determine if this allocator can
  /// deallocate pointers. Callers MUST only `Deallocate` memory using the same
  /// `Allocator` they used to `Allocate` it. This method is currently for
  /// internal use only.
  ///
  /// TODO: b/301677395 - Add dynamic type information to support a
  /// `std::pmr`-style `do_is_equal`. Without this information, it is not
  /// possible to determine whether another allocator has applied additional
  /// constraints to memory that otherwise may appear to be associated with this
  /// allocator.
  ///
  /// @param[in]  ptr         The pointer to be queried.
  /// @param[in]  layout      Describes the memory pointed at by `ptr`.
  ///
  /// @retval UNIMPLEMENTED         This object cannot recognize allocated
  ///                               pointers.
  /// @retval OUT_OF_RANGE          Pointer cannot be re/deallocated by this
  ///                               object.
  /// @retval OK                    This object can re/deallocate the pointer.
  Status Query(const void* ptr, Layout layout) const {
    return DoQuery(ptr, layout.size(), layout.alignment());
  }

  /// Like `Query`, but takes its parameters directly instead of as a `Layout`.
  ///
  /// Callers should almost always prefer `Query`. This method is meant for use
  /// by tests and other allocators implementing the virtual functions below.
  Status QueryUnchecked(const void* ptr, size_t size, size_t alignment) const {
    return DoQuery(ptr, size, alignment);
  }

  /// Allocates a block of memory with the specified size and alignment.
  ///
  /// Returns `nullptr` if the allocation cannot be made, or the `layout` has a
  /// size of 0.
  ///
  /// @param[in]  layout      Describes the memory to be allocated.
  void* Allocate(Layout layout) {
    return DoAllocate(layout.size(), layout.alignment());
  }

  /// Like `Allocate`, but takes its parameters directly instead of as a
  /// `Layout`.
  ///
  /// Callers should almost always prefer `Allocate`. This method is meant for
  /// use by tests and other allocators implementing the virtual functions
  /// below.
  void* AllocateUnchecked(size_t size, size_t alignment) {
    return DoAllocate(size, alignment);
  }

  /// Releases a previously-allocated block of memory.
  ///
  /// The given pointer must have been previously obtained from a call to either
  /// `Allocate` or `Reallocate`; otherwise the behavior is undefined.
  ///
  /// @param[in]  ptr           Pointer to previously-allocated memory.
  /// @param[in]  layout        Describes the memory to be deallocated.
  void Deallocate(void* ptr, Layout layout) {
    return DoDeallocate(ptr, layout.size(), layout.alignment());
  }

  /// Like `Deallocate`, but takes its parameters directly instead of as a
  /// `Layout`.
  ///
  /// Callers should almost always prefer `Deallocate`. This method is meant for
  /// use by tests and other allocators implementing the virtual functions
  /// below.
  void DeallocateUnchecked(void* ptr, size_t size, size_t alignment) {
    return DoDeallocate(ptr, size, alignment);
  }

  /// Modifies the size of an previously-allocated block of memory without
  /// copying any data.
  ///
  /// Returns true if its size was changed without copying data to a new
  /// allocation; otherwise returns false.
  ///
  /// In particular, it always returns true if the `old_layout.size()` equals
  /// `new_szie`, and always returns false if the given pointer is null, the
  /// `old_layout.size()` is 0, or the `new_size` is 0.
  ///
  /// @param[in]  ptr           Pointer to previously-allocated memory.
  /// @param[in]  old_layout    Describes the previously-allocated memory.
  /// @param[in]  new_size      Requested new size for the memory allocation.
  bool Resize(void* ptr, Layout old_layout, size_t new_size) {
    return DoResize(ptr, old_layout.size(), old_layout.alignment(), new_size);
  }

  /// Like `Resize`, but takes its parameters directly instead of as a `Layout`.
  ///
  /// Callers should almost always prefer `Resize`. This method is meant for use
  /// by tests and other allocators implementing the virtual functions below.
  bool ResizeUnchecked(void* ptr,
                       size_t old_size,
                       size_t old_alignment,
                       size_t new_size) {
    return DoResize(ptr, old_size, old_alignment, new_size);
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
  /// Unlike `Resize`, providing a null pointer or a `old_layout` with a size of
  /// 0 will return a new allocation.
  ///
  /// @param[in]  ptr         Pointer to previously-allocated memory.
  /// @param[in]  old_layout  Describes the previously-allocated memory.
  /// @param[in]  new_size    Requested new size for the memory allocation.
  void* Reallocate(void* ptr, Layout old_layout, size_t new_size) {
    return DoReallocate(
        ptr, old_layout.size(), old_layout.alignment(), new_size);
  }

  /// Like `Reallocate`, but takes its parameters directly instead of as a
  /// `Layout`.
  ///
  /// Callers should almost always prefer `Reallocate`. This method is meant for
  /// use by tests and other allocators implementing the virtual functions
  /// below.
  void* ReallocateUnchecked(void* ptr,
                            size_t old_size,
                            size_t old_alignment,
                            size_t new_size) {
    return DoReallocate(ptr, old_size, old_alignment, new_size);
  }

 private:
  /// Virtual `Query` function that can be overridden by derived classes.
  ///
  /// The default implementation of this method simply returns `UNIMPLEMENTED`.
  /// Allocators which dispatch to other allocators need to override this method
  /// in order to be able to direct reallocations and deallocations to
  /// appropriate allocator.
  virtual Status DoQuery(const void*, size_t, size_t) const {
    return Status::Unimplemented();
  }

  /// Virtual `Allocate` function implemented by derived classes.
  virtual void* DoAllocate(size_t size, size_t alignment) = 0;

  /// Virtual `Deallocate` function implemented by derived classes.
  virtual void DoDeallocate(void* ptr, size_t size, size_t alignment) = 0;

  /// Virtual `Resize` function implemented by derived classes.
  virtual bool DoResize(void* ptr,
                        size_t old_size,
                        size_t old_alignment,
                        size_t new_size) = 0;

  /// Virtual `Reallocate` function that can be overridden by derived classes.
  ///
  /// The default implementation will first try to `Resize` the data. If that is
  /// unsuccessful, it will allocate an entirely new block, copy existing data,
  /// and deallocate the given block.
  virtual void* DoReallocate(void* ptr,
                             size_t old_size,
                             size_t old_alignment,
                             size_t new_size);
};

}  // namespace pw::allocator
