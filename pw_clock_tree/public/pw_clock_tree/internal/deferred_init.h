// Copyright 2025 The Pigweed Authors
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

#include <array>
#include <atomic>
#include <mutex>
#include <new>

#include "pw_sync/interrupt_spin_lock.h"

namespace pw::clock_tree::internal {

// TODO: https://pwbug.dev/436920878 - Move this to a public home.

/// DeferredInit is constexpr-constructible and allows for the deferred
/// initialization of an object that is not constexpr-constructible.
/// The object is constructed on first access (via the -> or * operators).
/// T must have a default (parameterless) constructor.
template <typename T>
class DeferredInit {
 public:
  constexpr DeferredInit() {}

  T* operator->() { return object(); }
  T& operator*() { return *object(); }

 private:
  alignas(T) std::array<std::byte, sizeof(T)> storage_;
  std::atomic<bool> constructed_ = false;
  sync::InterruptSpinLock construct_lock_;

  T* object() {
    LazyConstruct();
    return std::launder(reinterpret_cast<T*>(storage_.data()));
  }

  void LazyConstruct() {
    // constructed_ can only transition from false -> true,
    // and never back to false, so it is safe to test it directly.
    if (constructed_.load(std::memory_order_acquire)) {
      return;
    }

    // We might need to construct the object.
    std::lock_guard lock(construct_lock_);

    // It's possible another thread beat us to the construct_lock_
    // and has already constructed the object.
    if (constructed_) {
      return;
    }

    // We are the first one here, so construct the object.
    new (storage_.data()) T;
    constructed_.store(true, std::memory_order_release);
  }
};

}  // namespace pw::clock_tree::internal
