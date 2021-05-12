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

#include "pw_function/internal/function.h"

namespace pw {

// pw::Function is a wrapper for an aribtrary callable object. It can be used by
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
//   bool ElementsArePostive(const pw::Vector<int>& items) {
//     return All(items, [](const int& i) { return i > 0; });
//   }
//
//   bool IsEven(const int& i) { return i % 2 == 0; }
//
//   bool ElementsAreEven(const pw::Vector<int>& items) {
//     return All(items, IsEven);
//   }
//
template <typename T>
using Function = function_internal::Function<T>;

// A Closure is a function that does not take any arguments and returns nothing.
using Closure = Function<void()>;

// nullptr comparisions for functions.
template <typename T>
bool operator==(const Function<T>& f, std::nullptr_t) {
  return !f;
}

template <typename T>
bool operator!=(const Function<T>& f, std::nullptr_t) {
  return !!f;
}

template <typename T>
bool operator==(std::nullptr_t, const Function<T>& f) {
  return !f;
}

template <typename T>
bool operator!=(std::nullptr_t, const Function<T>& f) {
  return !!f;
}

}  // namespace pw
