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

#include "pw_allocator/capability.h"
#include "pw_allocator/layout.h"
#include "pw_allocator/unique_ptr.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"

namespace pw {

/// Abstract interface for releasing memory.
class Deallocator {
 public:
  using Capabilities = allocator::Capabilities;
  using Capability = allocator::Capability;
  using Layout = allocator::Layout;

  virtual ~Deallocator() = default;

  const Capabilities& capabilities() const { return capabilities_; }

  /// Returns whether a given capabilityis enabled for this object.
  bool HasCapability(Capability capability) const {
    return capabilities_.has(capability);
  }

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

  /// Returns the total amount of memory provided by this object.
  ///
  /// This is an optional method. Some memory providers may not have an easily
  /// defined capacity, e.g. the system allocator. If implemented, the returned
  /// capacity may be less than the memory originally given to an allocator,
  /// e.g. if the allocator must align the region of memory, its capacity may be
  /// reduced.
  StatusWithSize GetCapacity() const {
    auto result = DoGetInfo(InfoType::kCapacity, nullptr);
    return StatusWithSize(result.status(), Layout::Unwrap(result).size());
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

 protected:
  /// TODO(b/326509341): Remove when downstream consumers migrate.
  constexpr Deallocator() = default;

  explicit constexpr Deallocator(const Capabilities& capabilities)
      : capabilities_(capabilities) {}

  /// Wraps an object of type ``T`` in a ``UniquePtr``
  ///
  /// @param[in]  ptr         Pointer to memory provided by this object.
  template <typename T>
  [[nodiscard]] UniquePtr<T> WrapUnique(T* ptr) {
    return UniquePtr<T>(UniquePtr<T>::kPrivateConstructor, ptr, this);
  }

  /// Indicates what kind of information to retrieve using `GetInfo`.
  ///
  /// Note that this enum is considered open, and may be extended in the future.
  /// As a result, implementers of `DoGetInfo` should include a default case
  /// that handles unrecognized info types. If building with `-Wswitch-enum`,
  /// you will also want to locally disable that diagnostic and build with
  /// `-Wswitch-default` instead, e.g.:
  ///
  /// @code{.cpp}
  /// class MyAllocator : public Allocator {
  ///  private:
  ///   Layout MyGetLayoutFromPointer(const void* ptr) { /* ... */ }
  ///
  ///   Result<Layout> DoGetInfo(InfoType info_type, const void* ptr) override {
  ///     PW_MODIFY_DIAGNOSTICS_PUSH();
  ///     PW_MODIFY_DIAGNOSTIC(ignored, "-Wswitch-enum");
  ///     switch(info_type) {
  ///       case InfoType::kAllocatedLayoutOf:
  ///         return MyGetLayoutFromPointer(ptr);
  ///       default:
  ///         return Status::Unimplmented();
  ///     }
  ///     PW_MODIFY_DIAGNOSTICS_POP();
  ///   }
  /// };
  /// @endcode
  ///
  /// See also `GetInfo`.
  enum class InfoType {
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
    kRequestedLayoutOf,

    /// If supported, `GetInfo` will return `OK` with the `Layout` of the
    /// usable memory associated with the given pointer, or `NOT_FOUND` if
    /// the pointer is not recognized.

    /// The usable layout may from either the requested layout, the layout of
    /// memory used to fulfill the request, or both.
    ///
    /// For example, it may have a larger size than the requested layout if it
    /// was padded to an alignment boundary, but may be less than the acutal
    /// memory if the object includes some overhead for metadata.
    kUsableLayoutOf,

    /// If supported, `GetInfo` will return `OK` with the `Layout` of the
    /// allocated memory associated with the given pointer, or `NOT_FOUND` if
    /// the pointer is not recognized.
    ///
    /// The layout of memory used to fulfill a request may differ from either
    /// the requested layout, the layout of the usable memory, or both.
    ///
    /// For example, it may have a larger size than the requested layout or the
    /// layout of usable memory if the object includes some overhead for
    /// metadata.
    kAllocatedLayoutOf,

    /// If supported, `GetInfo` will return `OK` with a `Layout` whose size
    /// is the total number of bytes that can be allocated by this object, and
    /// whose alignment is the minimum alignment of any allocation.
    ///
    /// The given pointer is ignored.
    kCapacity,

    /// If supported, `GetInfo` will return `OK` with a default `Layout` if the
    /// given pointer was provided by this object, or `NOT_FOUND`.
    ///
    /// This MUST only be used to dispatch between two or more objects
    /// associated with non-overlapping regions of memory. Do NOT use it to
    /// determine if this object can deallocate pointers. Callers MUST only
    /// deallocate memory using the same ``Deallocator`` that provided it.
    kRecognizes,
  };

  /// Returns deallocator-specific information about allocations.
  ///
  /// Deallocators may support any number of `InfoType`s. See that type for what
  /// each supported type returns. For unsupported types, this method returns
  /// `UNIMPLEMENTED`.
  Result<Layout> GetInfo(InfoType info_type, const void* ptr) const {
    return DoGetInfo(info_type, ptr);
  }

  /// @copydoc GetInfo
  ///
  /// This method is protected in order to restrict it to object
  /// implementations. It is static and takes an ``deallocator`` parameter in
  /// order to allow forwarding allocators to call it on wrapped allocators.
  static Result<Layout> GetInfo(const Deallocator& deallocator,
                                InfoType info_type,
                                const void* ptr) {
    return deallocator.DoGetInfo(info_type, ptr);
  }

  /// Convenience wrapper of `DoGetInfo` for getting the requested layout
  /// associated with a pointer.
  Result<Layout> GetRequestedLayout(const void* ptr) const {
    return DoGetInfo(InfoType::kRequestedLayoutOf, ptr);
  }

  /// Static version of `GetRequestedLayout` that allows forwarding allocators
  /// to call it on wrapped allocators.
  static Result<Layout> GetRequestedLayout(const Deallocator& deallocator,
                                           const void* ptr) {
    return deallocator.GetRequestedLayout(ptr);
  }

  /// Convenience wrapper of `DoGetInfo` for getting the usable layout
  /// associated with a pointer.
  Result<Layout> GetUsableLayout(const void* ptr) const {
    return DoGetInfo(InfoType::kUsableLayoutOf, ptr);
  }

  /// Static version of `GetUsableLayout` that allows forwarding allocators to
  /// call it on wrapped allocators.
  static Result<Layout> GetUsableLayout(const Deallocator& deallocator,
                                        const void* ptr) {
    return deallocator.GetUsableLayout(ptr);
  }

  /// Convenience wrapper of `DoGetInfo` for getting the allocated layout
  /// associated with a pointer.
  Result<Layout> GetAllocatedLayout(const void* ptr) const {
    return DoGetInfo(InfoType::kAllocatedLayoutOf, ptr);
  }

  /// Static version of `GetAllocatedLayout` that allows forwarding allocators
  /// to call it on wrapped allocators.
  static Result<Layout> GetAllocatedLayout(const Deallocator& deallocator,
                                           const void* ptr) {
    return deallocator.GetAllocatedLayout(ptr);
  }

  /// Convenience wrapper of `DoGetInfo` for getting whether the allocator
  /// recognizes a pointer.
  bool Recognizes(const void* ptr) const {
    return DoGetInfo(InfoType::kRecognizes, ptr).ok();
  }

  /// Static version of `Recognizes` that allows forwarding allocators to call
  /// it on wrapped allocators.
  static bool Recognizes(const Deallocator& deallocator, const void* ptr) {
    return deallocator.Recognizes(ptr);
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

  /// Virtual `GetInfo` function that can be overridden by derived classes.
  virtual Result<Layout> DoGetInfo(InfoType, const void*) const {
    return Status::Unimplemented();
  }

  const Capabilities capabilities_;
};

namespace allocator {

// Alias for module consumers using the older name for the above type.
using Deallocator = ::pw::Deallocator;

}  // namespace allocator
}  // namespace pw
