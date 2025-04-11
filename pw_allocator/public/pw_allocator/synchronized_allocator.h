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
#include <mutex>

#include "pw_allocator/allocator.h"
#include "pw_sync/borrow.h"
#include "pw_sync/lock_annotations.h"

namespace pw::allocator {

/// Wraps an `Allocator` with a lock to synchronize access.
///
/// Depending on the `LockType`, this object may be thread- and/or interrupt-
/// safe. For example, `SynchronizedAllocator<pw::sync::Mutex>` is thread-safe,
/// while `SynchronizedAllocator<pw::sync::InterruptSpinLock>` is thread- and
/// interrupt-safe.
///
/// @tparam LockType  The type of the lock used to synchronize allocator access.
///                   Must be default-constructible.
template <typename LockType>
class SynchronizedAllocator : public Allocator {
 private:
  using Pointer = sync::BorrowedPointer<Allocator, LockType>;

 public:
  SynchronizedAllocator(Allocator& allocator) noexcept
      : Allocator(allocator.capabilities()),
        allocator_(allocator),
        borrowable_(allocator_, lock_) {}

  /// Returns a borrowed pointer to the allocator.
  ///
  /// When an allocator being wrapped implements an interface that extends
  /// `pw::Allocator`, this method can be used to safely access a downcastable
  /// pointer. The usual warnings apply to the returned value; namely the caller
  /// MUST NOT leak the raw pointer.
  ///
  /// Example:
  /// @code{.cpp}
  ///   pw::allocator::BestFitAllocator<> best_fit(heap);
  ///   pw::allocator::SynchronizedAllocator<pw::sync::Mutex> synced(best_fit);
  ///   // ...
  ///   auto borrowed = synced.Borrow();
  ///   auto allocator =
  ///     static_cast<pw::allocator::BestFitAllocator<>&>(*borrowed);
  /// @endcode
  Pointer Borrow() const { return borrowable_.acquire(); }

 private:
  /// @copydoc Allocator::Allocate
  void* DoAllocate(Layout layout) override {
    std::lock_guard lock(lock_);
    return allocator_.Allocate(layout);
  }

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr) override {
    std::lock_guard lock(lock_);
    return allocator_.Deallocate(ptr);
  }

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, Layout) override { DoDeallocate(ptr); }

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, size_t new_size) override {
    std::lock_guard lock(lock_);
    return allocator_.Resize(ptr, new_size);
  }

  void* DoReallocate(void* ptr, Layout new_layout) override {
    std::lock_guard lock(lock_);
    return allocator_.Reallocate(ptr, new_layout);
  }

  /// @copydoc Allocator::GetAlloc
  size_t DoGetAllocated() const override {
    std::lock_guard lock(lock_);
    return allocator_.GetAllocated();
  }

  /// @copydoc Deallocator::GetCapacity
  size_t DoGetCapacity() const override {
    std::lock_guard lock(lock_);
    return allocator_.GetCapacity();
  }

  /// @copydoc Deallocator::GetInfo
  Layout DoGetLayout(LayoutType layout_type, const void* ptr) const override {
    std::lock_guard lock(lock_);
    return GetLayout(allocator_, layout_type, ptr);
  }

  /// @copydoc Deallocator::Recognizes
  bool DoRecognizes(const void* ptr) const override {
    std::lock_guard lock(lock_);
    return Recognizes(allocator_, ptr);
  }

  Allocator& allocator_ PW_GUARDED_BY(lock_);
  mutable LockType lock_;
  sync::Borrowable<Allocator, LockType> borrowable_;
};

/// Tag type used to indicate synchronization is NOT desired.
///
/// This can be useful with allocator parameters for module configuration, e.g.
/// PW_MALLOC_LOCK_TYPE.
struct NoSync {
  constexpr void lock() {}
  constexpr void unlock() {}
};

}  // namespace pw::allocator
