// Copyright 2020 The Pigweed Authors
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

#include <type_traits>

namespace pw::kvs {

namespace internal {
template <typename T, typename = decltype(span(std::declval<T>()))>
constexpr bool ConvertsToSpan(int) {
  return true;
}

// If the expression span(T) fails, then the type can't be converted to a span.
template <typename T>
constexpr bool ConvertsToSpan(...) {
  return false;
}

}  // namespace internal

// Traits class to detect if the type is a span.
template <typename T>
using ConvertsToSpan =
    std::bool_constant<internal::ConvertsToSpan<std::remove_reference_t<T>>(0)>;

}  // namespace pw::kvs
