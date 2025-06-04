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
#include "pw_containers/inline_deque.h"
#include "pw_containers/internal/async_count_and_capacity.h"

namespace pw {

/// Async wrapper around BasicInlineDeque.
template <typename T,
          typename SizeType,
          size_t kCapacity = containers::internal::kGenericSized>
class BasicInlineAsyncDeque
    : public containers::internal::BasicInlineDequeImpl<
          T,
          containers::internal::AsyncCountAndCapacity<uint16_t>,
          kCapacity> {
 private:
  using Base = containers::internal::BasicInlineDequeImpl<
      T,
      containers::internal::AsyncCountAndCapacity<uint16_t>,
      kCapacity>;

 public:
  using typename Base::const_iterator;
  using typename Base::const_pointer;
  using typename Base::const_reference;
  using typename Base::difference_type;
  using typename Base::iterator;
  using typename Base::pointer;
  using typename Base::reference;
  using typename Base::size_type;
  using typename Base::value_type;

  /// Returns `Pending` until space for `num` elements is available.
  async2::Poll<> PendAvailable(async2::Context& context, size_type num = 1) {
    return Base::count_and_capacity().PendAvailable(context, num);
  }
};

/// Async wrapper around InlineDeque.
template <typename T, size_t kCapacity = containers::internal::kGenericSized>
using InlineAsyncDeque = BasicInlineAsyncDeque<T, uint16_t, kCapacity>;

}  // namespace pw
