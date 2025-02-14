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

#include "pw_bloat/bloat_this_binary.h"
#include "pw_containers/inline_deque.h"
#include "pw_containers/size_report/size_report.h"

namespace pw::containers::size_report {

/// Invokes methods of the pw::InlineDeque type.
///
/// This method is used both to measure inline deques directly, as well as to
/// provide a baseline for measuring other types that use inline deques and want
/// to only measure their contributions to code size.
template <typename T, int&... kExplicitGuard, typename Iterator>
int MeasureInlineDeque(Iterator first, Iterator last, uint32_t mask) {
  mask = SetBaseline(mask);
  auto& inline_deque = GetContainer<InlineDeque<T, kNumItems>>();
  inline_deque.assign(first, last);
  mask = MeasureContainer(inline_deque, mask);
  PW_BLOAT_COND(inline_deque.full(), mask);

  T& item1 = inline_deque.front();
  T& itemN = inline_deque.back();
  PW_BLOAT_EXPR(inline_deque.pop_front(), mask);
  PW_BLOAT_EXPR(inline_deque.pop_back(), mask);
  PW_BLOAT_EXPR(inline_deque.push_front(itemN), mask);
  PW_BLOAT_EXPR(inline_deque.push_back(item1), mask);
  PW_BLOAT_EXPR(inline_deque.clear(), mask);

  size_t new_size = kNumItems + 2;
  PW_BLOAT_EXPR(inline_deque.resize(new_size), mask);

  return inline_deque.size() == new_size ? 0 : 1;
}

}  // namespace pw::containers::size_report
