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
  using pointer_type = sync::BorrowedPointer<Allocator, LockType>;

  /// @copydoc Allocator::Allocate
  void* DoAllocate(Layout layout) override {
    pointer_type allocator = borrowable_.acquire();
    return allocator->Allocate(layout);
  }

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, Layout layout) override {
    pointer_type allocator = borrowable_.acquire();
    return allocator->Deallocate(ptr, layout);
  }

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, Layout layout, size_t new_size) override {
    pointer_type allocator = borrowable_.acquire();
    return allocator->Resize(ptr, layout, new_size);
  }

  /// @copydoc Allocator::Query
  Status DoQuery(const void* ptr, Layout layout) const override {
    pointer_type allocator = borrowable_.acquire();
    return allocator->Query(ptr, layout);
  }

  /// @copydoc Allocator::GetLayout
  Result<Layout> DoGetLayout(const void* ptr) const override {
    pointer_type allocator = borrowable_.acquire();
    return allocator->GetLayout(ptr);
  }

  LockType lock_;
  sync::Borrowable<Allocator, LockType> borrowable_;
};

}  // namespace pw::allocator
