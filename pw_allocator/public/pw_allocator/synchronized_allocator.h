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

#include "pw_allocator/allocator.h"
#include "pw_sync/borrow.h"

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
 public:
  constexpr SynchronizedAllocator(Allocator& allocator) noexcept
      : Allocator(allocator.capabilities()), borrowable_(allocator, lock_) {}

 private:
  using Pointer = sync::BorrowedPointer<Allocator, LockType>;

  /// @copydoc Allocator::Allocate
  void* DoAllocate(Layout layout) override {
    Pointer allocator = borrowable_.acquire();
    return allocator->Allocate(layout);
  }

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr) override {
    Pointer allocator = borrowable_.acquire();
    return allocator->Deallocate(ptr);
  }

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, Layout) override { DoDeallocate(ptr); }

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, size_t new_size) override {
    Pointer allocator = borrowable_.acquire();
    return allocator->Resize(ptr, new_size);
  }

  void* DoReallocate(void* ptr, Layout new_layout) override {
    Pointer allocator = borrowable_.acquire();
    return allocator->Reallocate(ptr, new_layout);
  }

  /// @copydoc Allocator::Query
  Status DoQuery(const void* ptr) const override {
    Pointer allocator = borrowable_.acquire();
    return Query(*allocator, ptr);
  }

  /// @copydoc Allocator::GetCapacity
  StatusWithSize DoGetCapacity() const override {
    Pointer allocator = borrowable_.acquire();
    return allocator->GetCapacity();
  }

  /// @copydoc Allocator::GetRequestedLayout
  Result<Layout> DoGetRequestedLayout(const void* ptr) const override {
    Pointer allocator = borrowable_.acquire();
    return GetRequestedLayout(*allocator, ptr);
  }

  /// @copydoc Allocator::GetUsableLayout
  Result<Layout> DoGetUsableLayout(const void* ptr) const override {
    Pointer allocator = borrowable_.acquire();
    return GetUsableLayout(*allocator, ptr);
  }

  /// @copydoc Allocator::DoGetAllocatedLayout
  Result<Layout> DoGetAllocatedLayout(const void* ptr) const override {
    Pointer allocator = borrowable_.acquire();
    return GetAllocatedLayout(*allocator, ptr);
  }

  LockType lock_;
  sync::Borrowable<Allocator, LockType> borrowable_;
};

}  // namespace pw::allocator
