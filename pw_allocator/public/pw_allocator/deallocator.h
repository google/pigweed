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
#include <limits>

#include "pw_allocator/capability.h"
#include "pw_allocator/layout.h"
#include "pw_allocator/unique_ptr.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"

namespace pw {

/// Abstract interface for releasing memory.
class Deallocator {
 protected:
  // Alias types and variables needed for SFINAE below.
  template <typename T>
  static constexpr bool is_bounded_array_v =
      allocator::internal::is_bounded_array_v<T>;

  template <typename T>
  static constexpr bool is_unbounded_array_v =
      allocator::internal::is_unbounded_array_v<T>;

 public:
  using Capabilities = allocator::Capabilities;
  using Capability = allocator::Capability;
  using Layout = allocator::Layout;

  virtual ~Deallocator() = default;

  constexpr const Capabilities& capabilities() const { return capabilities_; }

  /// Returns whether a given capabilityis enabled for this object.
  bool HasCapability(Capability capability) const {
    return capabilities_.has(capability);
  }

  /// Returns the total amount of memory provided by this object.
  ///
  /// This is an optional method. Some memory providers may not have an easily
  /// defined capacity, e.g. the system allocator. If not implemented, this
  /// will return std::numeric_limits<size_t>::max(). A zero value may be used
  /// to indicate an allocator that has not been initialized.
  ///
  /// If implemented, the returned capacity may be less than the memory
  /// originally given to an allocator, e.g. if the allocator must align the
  /// region of memory, its capacity may be reduced.
  size_t GetCapacity() const { return DoGetCapacity(); }

  /// Releases a previously-allocated block of memory.
  ///
  /// The given pointer must have been previously provided by this memory
  /// resource; otherwise the behavior is undefined.
  ///
  /// @param[in]  ptr           Pointer to previously-allocated memory.
  void Deallocate(void* ptr) {
    if (ptr != nullptr) {
      DoDeallocate(ptr);
    }
  }

  /// Deprecated version of `Deallocate` that takes a `Layout`.
  /// Do not use this method. It will be removed.
  /// TODO(b/326509341): Remove when downstream consumers migrate.
  void Deallocate(void* ptr, Layout layout) {
    if (ptr != nullptr) {
      DoDeallocate(ptr, layout);
    }
  }

  /// Destroys the object at ``ptr`` and deallocates the associated memory.
  ///
  /// The given pointer must have been previously obtained from a call to
  /// ``New`` using the same object; otherwise the behavior is undefined.
  ///
  /// @param[in] ptr      Pointer to previously-allocated object.
  template <typename T>
  void Delete(T* ptr) {
    if (!capabilities_.has(Capability::kSkipsDestroy)) {
      std::destroy_at(ptr);
    }
    Deallocate(ptr);
  }

  /// Returns whether the given object is the same as this one.
  ///
  /// This method is used instead of ``operator==`` in keeping with
  /// ``std::pmr::memory_resource::is_equal``. There currently is no
  /// corresponding virtual ``DoIsEqual``, as objects that would
  /// require ``dynamic_cast`` to properly determine equality are not supported.
  /// This method will allow the interface to remain unchanged should a future
  /// need for such objects arise.
  ///
  /// @param[in]  other       Object to compare with this object.
  bool IsEqual(const Deallocator& other) const { return this == &other; }

  // See WrapUnique below. Disallow calls with explicitly-sized array types like
  // `T[kN]`.
  template <typename T,
            int&... kExplicitGuard,
            std::enable_if_t<is_bounded_array_v<T>, int> = 0,
            typename... Args>
  void WrapUnique(Args&&...) = delete;

 protected:
  /// TODO(b/326509341): Remove when downstream consumers migrate.
  constexpr Deallocator() = default;

  explicit constexpr Deallocator(const Capabilities& capabilities)
      : capabilities_(capabilities) {}

  /// Wraps an object of type ``T`` in a ``UniquePtr``
  ///
  /// @param[in]  ptr         Pointer to memory provided by this object.
  template <typename T, std::enable_if_t<!std::is_array_v<T>, int> = 0>
  [[nodiscard]] UniquePtr<T> WrapUnique(T* ptr) {
    return UniquePtr<T>(ptr, this);
  }

  /// Wraps an array of type ``T`` in a ``UniquePtr``
  ///
  /// @param[in]  ptr         Pointer to memory provided by this object.
  /// @param[in]  size        The size of the array.
  template <typename T,
            int&... kExplicitGuard,
            typename ElementType = std::remove_extent_t<T>,
            std::enable_if_t<is_unbounded_array_v<T>, int> = 0>
  [[nodiscard]] UniquePtr<T> WrapUnique(ElementType* ptr, size_t size) {
    return UniquePtr<T>(ptr, size, this);
  }

  /// Deprecated version of `WrapUnique` with a different name and templated on
  /// the object type instead of the array type.
  /// Do not use this method. It will be removed.
  /// TODO(b/326509341): Remove when downstream consumers migrate.
  template <typename T>
  [[nodiscard]] UniquePtr<T[]> WrapUniqueArray(T* ptr, size_t size) {
    return WrapUnique<T[]>(ptr, size);
  }

  /// Indicates what kind of layout information to retrieve using `GetLayout`.
  enum class LayoutType {
    /// If supported, `GetInfo` will return `OK` with the `Layout` of the
    /// requested memory associated with the given pointer, or `NOT_FOUND` if
    /// the pointer is not recognized.
    ///
    /// The requested layout may differ from either the layout of usable memory,
    /// the layout of memory used to fulfill the request, or both.
    ///
    /// For example, it may have a smaller size than the usable memory if the
    /// latter was padded to an alignment boundary, or may have a less strict
    /// alignment than the actual memory.
    kRequested,

    /// If supported, `GetInfo` will return `OK` with the `Layout` of the
    /// usable memory associated with the given pointer, or `NOT_FOUND` if
    /// the pointer is not recognized.

    /// The usable layout may from either the requested layout, the layout of
    /// memory used to fulfill the request, or both.
    ///
    /// For example, it may have a larger size than the requested layout if it
    /// was padded to an alignment boundary, but may be less than the acutal
    /// memory if the object includes some overhead for metadata, such as a
    /// block header.
    kUsable,

    /// If supported, `GetInfo` will return `OK` with the `Layout` of the
    /// allocated memory associated with the given pointer, or `NOT_FOUND` if
    /// the pointer is not recognized.
    ///
    /// The layout of memory used to fulfill a request may differ from either
    /// the requested layout, the layout of the usable memory, or both.
    ///
    /// For example, it may have a larger size than the requested layout or the
    /// layout of usable memory if the object includes some overhead for
    /// metadata, such as a block header.
    kAllocated,
  };

  /// Returns deallocator-specific layout information about an allocation.
  ///
  /// Deallocators may support any number of `LayoutType`s. See that type for
  /// what each supported type returns. For unsupported types, this method
  /// returns an empty layout of zero size and minimal alignment.
  static Layout GetLayout(const Deallocator& deallocator,
                          LayoutType layout_type,
                          const void* ptr) {
    return deallocator.DoGetLayout(layout_type, ptr);
  }

  // Convenience wrappers.
  Layout GetRequestedLayout(const void* ptr) const {
    return DoGetLayout(LayoutType::kRequested, ptr);
  }

  Layout GetUsableLayout(const void* ptr) const {
    return DoGetLayout(LayoutType::kUsable, ptr);
  }

  Layout GetAllocatedLayout(const void* ptr) const {
    return DoGetLayout(LayoutType::kAllocated, ptr);
  }

  /// Returns whether the deallocator recognizes a pointer as one of its
  /// outstanding allocations.
  ///
  /// This MUST only be used to dispatch between two or more objects
  /// associated with non-overlapping regions of memory. Do NOT use it to
  /// determine if this object can deallocate pointers. Callers MUST only
  /// deallocate memory using the same ``Deallocator`` that provided it.
  static bool Recognizes(const Deallocator& deallocator, const void* ptr) {
    return deallocator.DoRecognizes(ptr);
  }

 private:
  /// Virtual `Deallocate` function implemented by derived classes.
  ///
  /// @param[in]  ptr           Pointer to memory, guaranteed to not be null.
  virtual void DoDeallocate(void*) {
    // This method will be pure virtual once consumer migrate from the deprected
    // version that takes a `Layout` parameter. In the meantime, the check that
    // this method is implemented is deferred to run-time.
    PW_ASSERT(false);
  }

  /// Deprecated version of `DoDeallocate` that takes a `Layout`.
  /// Do not use this method. It will be removed.
  virtual void DoDeallocate(void* ptr, Layout) { DoDeallocate(ptr); }

  /// @copydoc GetCapacity
  virtual size_t DoGetCapacity() const {
    return std::numeric_limits<size_t>::max();
  }

  /// @copydoc Recognizes
  virtual bool DoRecognizes(const void*) const { return false; }

  /// @copydoc GetLayout
  virtual Layout DoGetLayout(LayoutType, const void*) const { return Layout(); }

  const Capabilities capabilities_;
};

namespace allocator {

// Alias for module consumers using the older name for the above type.
using Deallocator = ::pw::Deallocator;

}  // namespace allocator
}  // namespace pw
