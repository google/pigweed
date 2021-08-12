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

#include <utility>

#include "pw_polyfill/standard_library/namespace.h"

#ifndef __cpp_lib_integer_sequence
#define __cpp_lib_integer_sequence 201304L

_PW_POLYFILL_BEGIN_NAMESPACE_STD

template <typename T, T... kSequence>
struct integer_sequence {
  static constexpr size_t size() noexcept { return sizeof...(kSequence); }
};

namespace impl {

// In the absence of a compiler builtin for this, have MakeSequence expand
// recursively to enumerate all indices up to count.
template <size_t kCount, typename T, T... kSequence>
struct MakeSequence : MakeSequence<kCount - 1, T, kCount - 1, kSequence...> {};

template <typename T, T... kSequence>
struct MakeSequence<0, T, kSequence...>
    : std::integer_sequence<T, kSequence...> {};

}  // namespace impl

template <size_t... kSequence>
using index_sequence = integer_sequence<size_t, kSequence...>;

template <typename T, T kCount>
using make_integer_sequence = impl::MakeSequence<kCount, T>;

template <size_t kCount>
using make_index_sequence = make_integer_sequence<size_t, kCount>;

template <typename... T>
using index_sequence_for = make_index_sequence<sizeof...(T)>;

_PW_POLYFILL_END_NAMESPACE_STD

#endif  // __cpp_lib_integer_sequence
