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

#include "pw_async2/context.h"
#include "pw_async2/waker.h"
#include "pw_containers/inline_queue.h"

namespace pw::async2 {
namespace internal {

class WakerQueueBase {
 public:
  constexpr WakerQueueBase(InlineQueue<Waker>& queue) : queue_(queue) {}

  /// Returns the number of wakers in the queue.
  size_t size() const { return queue_.size(); }

  /// Returns ``true`` if there are no wakers in the queue.
  bool empty() const { return queue_.empty(); }

  /// Returns ``true`` if the queue has no remaining space.
  bool full() const { return queue_.full(); }

  /// Wakes the first ``Waker`` in the queue.
  void WakeOne() { WakeMany(1); }

  /// Wakes ``count`` Wakers from the front of the queue.
  void WakeMany(size_t count);

  /// Wakes every ``Waker`` in the queue.
  void WakeAll() { WakeMany(queue_.size()); }

  /// Adds a waker to the end of the queue if there is space. Returns ``true``
  /// if the waker was added, or ``false`` if the queue is full.
  ///
  /// NOTE: Prefer using ``PW_ASYNC_STORE_WAKER`` instead of this API directly
  /// as it supports specifying a wait reason.
  bool Add(Waker&& waker);

 private:
  InlineQueue<Waker>& queue_;
};

}  // namespace internal

/// @submodule{pw_async2,combinators}

/// A ``WakerQueue`` is an ordered list of ``Waker``s that allows multiple tasks
/// to wait on the same asynchronous operation.
///
/// @copydoc internal::WakerQueueBase
template <size_t kCapacity>
class WakerQueue : public internal::WakerQueueBase {
 public:
  constexpr WakerQueue() : internal::WakerQueueBase(queue_) {}

 private:
  InlineQueue<Waker, kCapacity> queue_;
};

/// @}

}  // namespace pw::async2
