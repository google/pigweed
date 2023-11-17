// Copyright 2023 The Pigweed Authors
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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_LIB_CPP_TYPE_TO_STD_ARRAY_H_
#define SRC_CONNECTIVITY_BLUETOOTH_LIB_CPP_TYPE_TO_STD_ARRAY_H_

#include <array>
#include <cstddef>
#include <type_traits>

namespace bt_lib_cpp_type {

// For object type T, provide a member |type| = T if T is not an array,
// otherwise |type| = std::array of elements of T (recursively if T is a
// multidimensional array). cv-qualifiers are retained.
template <typename T, typename Enable = void>
struct ToStdArray;  // undefined base case for references, zero length arrays,
                    // etc.

template <typename T>
struct ToStdArray<
    T,
    std::enable_if_t<std::is_object_v<T> && !std::is_array_v<T>>> {
  using type = T;
};

// Note that this specialization doesn't accept zero-length arrays, i.e. T[0].
template <typename T, size_t N>
struct ToStdArray<T[N]> {
  using type = std::array<typename ToStdArray<T>::type, N>;
};

// This accepts array function parameters and flexible array struct members, but
// not pointers.
template <typename T>
struct ToStdArray<T[]> {
  using type = std::array<typename ToStdArray<T>::type, 0>;
};

template <typename T>
using ToStdArrayT = typename ToStdArray<T>::type;

}  // namespace bt_lib_cpp_type

#endif  // SRC_CONNECTIVITY_BLUETOOTH_LIB_CPP_TYPE_TO_STD_ARRAY_H_
