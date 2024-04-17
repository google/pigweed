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

namespace pw::allocator {

/// Abstract interface for releasing memory.
class Deallocator {
 public:
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
    std::destroy_at(ptr);
    Deallocate(ptr);
  }

  /// Returns the total amount of memory provided by this object.
  ///
  /// This is an optional method. Some memory providers may not have an easily
  /// defined capacity, e.g. the system allocator. If implemented, the returned
  /// capacity may be less than the memory originally given to an allocator,
  /// e.g. if the allocator must align the region of memory, its capacity may be
  /// reduced.
  StatusWithSize GetCapacity() const { return DoGetCapacity(); }

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

  /// Returns whether a given memory allocation was provided by this reource.
  ///
  /// This method MUST only be used to dispatch between two or more objects
  /// assoicated with non-overlapping regions of memory. Do NOT use it to
  /// determine if this object can deallocate pointers. Callers MUST only
  /// deallocate memory using the same ``Deallocator`` that provided it.
  ///
  /// This method is protected in order to restrict it to object
  /// implementations. It is static and takes an ``deallocator`` parameter in
  /// order to allow forwarding allocators to call it on wrapped allocators.
  ///
  /// @param[in]  deallocator   The object that provided ``ptr``.
  /// @param[in]  ptr           The pointer to be queried.
  /// @param[in]  layout        Describes the memory pointed at by `ptr`.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: This object can re/deallocate the pointer.
  ///
  ///    OUT_OF_RANGE: Pointer cannot be re/deallocated by this object.
  ///
  ///    UNIMPLEMENTED: This object cannot recognize allocated pointers.
  ///
  /// @endrst
  static Status Query(const Deallocator& deallocator, const void* ptr) {
    return deallocator.DoQuery(ptr);
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

  /// Virtual `GetCapacity` function that can be overridden by derived classes.
  virtual StatusWithSize DoGetCapacity() const;

  /// Virtual `Query` function that can be overridden by derived classes.
  virtual Status DoQuery(const void*) const;

  const Capabilities capabilities_;
};

}  // namespace pw::allocator
