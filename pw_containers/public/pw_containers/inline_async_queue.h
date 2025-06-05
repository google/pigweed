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

#include <cstddef>
#include <cstdint>

#include "pw_async2/context.h"
#include "pw_async2/poll.h"
#include "pw_async2/try.h"
#include "pw_containers/inline_async_deque.h"
#include "pw_containers/inline_queue.h"
#include "pw_containers/internal/async_count_and_capacity.h"

namespace pw {

/// Async wrapper around InlineQueue.
template <typename T, size_t kCapacity = containers::internal::kGenericSized>
class InlineAsyncQueue : public containers::internal::BasicInlineQueueImpl<
                             InlineAsyncQueue<T, kCapacity>,
                             InlineAsyncDeque<T, kCapacity>> {
 private:
  using Deque = InlineAsyncDeque<T, kCapacity>;

 public:
  using const_iterator = typename Deque::const_iterator;
  using const_pointer = typename Deque::const_pointer;
  using const_reference = typename Deque::const_reference;
  using difference_type = typename Deque::difference_type;
  using iterator = typename Deque::iterator;
  using pointer = typename Deque::pointer;
  using reference = typename Deque::reference;
  using size_type = typename Deque::size_type;
  using value_type = typename Deque::value_type;

  /// Returns `Pending` until space for `num` elements is available.
  async2::Poll<> PendAvailable(async2::Context& context, size_type num = 1) {
    return deque_.PendAvailable(context, num);
  }

 private:
  template <typename, typename>
  friend class containers::internal::GenericQueue;

  Deque& deque() { return deque_; }
  const Deque& deque() const { return deque_; }

  Deque deque_;
};

}  // namespace pw
