// Copyright 2021 The Pigweed Authors
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

#include "lib/fit/function.h"
#include "pw_function/config.h"

namespace pw {

// pw::Function is a wrapper for an arbitrary callable object. It can be used by
// callback-based APIs to allow callers to provide any type of callable.
//
// Example:
//
//   template <typename T>
//   bool All(const pw::Vector<T>& items,
//            pw::Function<bool(const T& item)> predicate) {
//     for (const T& item : items) {
//       if (!predicate(item)) {
//         return false;
//       }
//     }
//     return true;
//   }
//
//   bool ElementsArePositive(const pw::Vector<int>& items) {
//     return All(items, [](const int& i) { return i > 0; });
//   }
//
//   bool IsEven(const int& i) { return i % 2 == 0; }
//
//   bool ElementsAreEven(const pw::Vector<int>& items) {
//     return All(items, IsEven);
//   }
//

template <typename Callable,
          size_t inline_target_size =
              function_internal::config::kInlineCallableSize>
using Function = fit::function_impl<
    inline_target_size,
    /*require_inline=*/!function_internal::config::kEnableDynamicAllocation,
    Callable>;

using Closure = Function<void()>;

}  // namespace pw
