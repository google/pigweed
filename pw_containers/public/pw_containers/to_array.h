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

#include <array>
#include <cstddef>
#include <type_traits>
#include <utility>

namespace pw {
namespace containers {
namespace impl {

template <typename T, size_t kSize, size_t... kIndices>
constexpr std::array<std::remove_cv_t<T>, kSize> CopyArray(
    const T (&values)[kSize], std::index_sequence<kIndices...>) {
  return {{values[kIndices]...}};
}

template <typename T, size_t kSize, size_t... kIndices>
constexpr std::array<std::remove_cv_t<T>, kSize> MoveArray(
    T (&&values)[kSize], std::index_sequence<kIndices...>) {
  return {{std::move(values[kIndices])...}};
}

}  // namespace impl

// pw::containers::to_array is C++14-compatible implementation of C++20's
// std::to_array.
template <typename T, size_t kSize>
constexpr std::array<std::remove_cv_t<T>, kSize> to_array(T (&values)[kSize]) {
  return impl::CopyArray(values, std::make_index_sequence<kSize>{});
}

template <typename T, size_t kSize>
constexpr std::array<std::remove_cv_t<T>, kSize> to_array(T (&&values)[kSize]) {
  return impl::MoveArray(std::move(values), std::make_index_sequence<kSize>{});
}

}  // namespace containers
}  // namespace pw
