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

#include <array>
#include <type_traits>
#include <utility>

#include "pw_polyfill/standard_library/namespace.h"

#ifndef __cpp_lib_to_array
#define __cpp_lib_to_array 201907L

_PW_POLYFILL_BEGIN_NAMESPACE_STD

namespace impl {

template <typename T, size_t size, size_t... indices>
constexpr array<remove_cv_t<T>, size> CopyArray(const T (&values)[size],
                                                index_sequence<indices...>) {
  return {{values[indices]...}};
}

template <typename T, size_t size, size_t... indices>
constexpr array<remove_cv_t<T>, size> MoveArray(T(&&values)[size],
                                                index_sequence<indices...>) {
  return {{move(values[indices])...}};
}

}  // namespace impl

template <typename T, size_t size>
constexpr array<remove_cv_t<T>, size> to_array(T (&values)[size]) {
  return impl::CopyArray(values, make_index_sequence<size>{});
}

template <typename T, size_t size>
constexpr array<remove_cv_t<T>, size> to_array(T(&&values)[size]) {
  return impl::MoveArray(move(values), make_index_sequence<size>{});
}

_PW_POLYFILL_END_NAMESPACE_STD

#endif  // __cpp_lib_to_array
