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
#include "pw_result/result.h"

namespace pw::allocator {

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

  /// Constructs and object of type `T` from the given `args`
  ///
  /// The return value is nullable, as allocating memory for the object may
  /// fail. Callers must check for this error before using the resulting
  /// pointer.
  ///
  /// @param[in]  args...     Arguments passed to the object constructor.
  template <typename T, int&... ExplicitGuard, typename... Args>
  T* New(Args&&... args) {
    void* ptr = Allocate(Layout::Of<T>());
    return ptr != nullptr ? new (ptr) T(std::forward<Args>(args)...) : nullptr;
  }

  /// Constructs and object of type `T` from the given `args`, and wraps it in a
  /// `UniquePtr`
  ///
  /// The returned value may contain null if allocating memory for the object
  /// fails. Callers must check for null before using the `UniquePtr`.
  ///
  /// @param[in]  args...     Arguments passed to the object constructor.
  template <typename T, int&... ExplicitGuard, typename... Args>
  [[nodiscard]] UniquePtr<T> MakeUnique(Args&&... args) {
    return Deallocator::WrapUnique<T>(New<T>(std::forward<Args>(args)...));
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

  /// Returns the layout used to allocate a given pointer.
  ///
  /// NOTE: This method will eventually be deprecated. Use `GetAllocatedLayout`
  /// instead.
  ///
  /// @retval OK                Returns the actual layout of allocated memory.
  /// @retval NOT_FOUND         The allocator does not recognize the pointer
  ///                           as one of its allocations.
  /// @retval UNIMPLEMENTED     Allocator cannot recover allocation details.
  Result<Layout> GetLayout(const void* ptr) const {
    return GetAllocatedLayout(*this, ptr);
  }

 protected:
  /// TODO(b/326509341): Remove when downstream consumers migrate.
  constexpr Allocator() = default;

  explicit constexpr Allocator(const Capabilities& capabilities)
      : Deallocator(capabilities) {}

  /// Returns the layout that was requested when allocating a given pointer.
  ///
  /// This optional method can recover details about what memory was requested
  /// from a pointer previously allocated by a given object. The
  /// requested layout may differ from either the layout of usable memory, the
  /// layout of memory used to fulfill the request, or both.
  ///
  /// For example, it may have a smaller size than the usable memory if the
  /// latter was padded to an alignment boundary, or may have a less strict
  /// alignment than the actual memory.
  ///
  /// This method is protected in order to restrict it to object
  /// implementations. It is static and takes an ``allocator`` parameter in
  /// order to allow forwarding allocators to call it on wrapped allocators.
  ///
  /// @param[in]  allocator   The object that provided ``ptr``.
  /// @param[in]  ptr           A pointer to previously allocated memory.
  ///
  /// @retval OK                Returns the originally requested layout.
  /// @retval NOT_FOUND         The allocator does not recognize the pointer
  ///                           as one of its allocations.
  /// @retval UNIMPLEMENTED     Implementation cannot recover allocation
  ///                           details.
  static Result<Layout> GetRequestedLayout(const Allocator& allocator,
                                           const void* ptr);

  /// Returns the layout of the usable memory associated with a given pointer.
  ///
  /// This optional method can recover details about what memory is usable for a
  /// pointer previously allocated by this object. The usable layout
  /// may from either the requested layout, the layout of memory used to fulfill
  /// the request, or both.
  ///
  /// For example, it may have a larger size than the requested layout if it
  /// was padded to an alignment boundary, but may be less than the acutal
  /// memory if the object includes some overhead for metadata.
  ///
  /// This method is protected in order to restrict it to object
  /// implementations. It is static and takes an ``allocator`` parameter in
  /// order to allow forwarding allocators to call it on wrapped allocators.
  ///
  /// @param[in]  allocator      The object that provided ``ptr``.
  /// @param[in]  ptr           A pointer to previously allocated memory.
  ///
  /// @retval OK                Returns the layout of usable memory.
  /// @retval NOT_FOUND         The allocator does not recognize the pointer
  ///                           as one of its allocations.
  /// @retval UNIMPLEMENTED     Implementation cannot recover allocation
  ///                           details.
  static Result<Layout> GetUsableLayout(const Allocator& allocator,
                                        const void* ptr);

  /// Returns the layout of the memory used to allocate a given pointer.
  ///
  /// This optional method can recover details about what memory is usable for a
  /// pointer previously allocated by this object. The layout of memory
  /// used to fulfill a request may differ from either the requested layout, the
  /// layout of the usable memory, or both.
  ///
  /// For example, it may have a larger size than the requested layout or the
  /// layout of usable memory if the object includes some overhead for
  /// metadata.
  ///
  /// This method is protected in order to restrict it to object
  /// implementations. It is static and takes an ``allocator`` parameter in
  /// order to allow forwarding allocators to call it on wrapped allocators.
  ///
  /// @param[in]  allocator   The object that provided ``ptr``.
  /// @param[in]  ptr           A pointer to previously allocated memory.
  ///
  /// @retval OK                Returns the layout of usable memory.
  /// @retval NOT_FOUND         The allocator does not recognize the pointer
  ///                           as one of its allocations.
  /// @retval UNIMPLEMENTED     Implementation cannot recover allocation
  ///                           details.
  static Result<Layout> GetAllocatedLayout(const Allocator& allocator,
                                           const void* ptr);

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

  /// Virtual `GetRequested` function that can be overridden by derived classes.
  ///
  /// The default implementation of this method simply returns `UNIMPLEMENTED`,
  /// indicating the allocator cannot recover layouts from allocated pointers.
  ///
  /// @param[in]  ptr           Pointer to memory, guaranteed to not be null.
  virtual Result<Layout> DoGetRequestedLayout(const void*) const;

  /// Virtual `Query` function that can be overridden by derived classes.
  virtual Result<Layout> DoGetUsableLayout(const void*) const;

  /// Virtual `Query` function that can be overridden by derived classes.
  virtual Result<Layout> DoGetAllocatedLayout(const void*) const;
};

}  // namespace pw::allocator
