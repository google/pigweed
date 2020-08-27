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

#include "pw_polyfill/standard_library/namespace.h"

_PW_POLYFILL_BEGIN_NAMESPACE_STD

// Defines std:foo_t aliases for typename foo::type. This is a small subset of
// <type_traits> which may be expanded as needed.
#ifndef __cpp_lib_transformation_trait_aliases
#define __cpp_lib_transformation_trait_aliases 201304L

template <decltype(sizeof(int)) Len, decltype(sizeof(int)) Align>
using aligned_storage_t = typename aligned_storage<Len, Align>::type;

template <typename... T>
using common_type_t = typename common_type<T...>::type;

template <bool B, typename T, typename F>
using conditional_t = typename conditional<B, T, F>::type;

template <typename T>
using decay_t = typename decay<T>::type;

template <bool B, typename T = void>
using enable_if_t = typename enable_if<B, T>::type;

template <typename T>
using make_signed_t = typename make_signed<T>::type;

template <typename T>
using make_unsigned_t = typename make_unsigned<T>::type;

template <typename T>
using remove_cv_t = typename remove_cv<T>::type;

template <typename T>
using remove_pointer_t = typename remove_pointer<T>::type;

template <typename T>
using remove_reference_t = typename remove_reference<T>::type;

#endif  // __cpp_lib_transformation_trait_aliases

#ifndef __cpp_lib_is_null_pointer
#define __cpp_lib_is_null_pointer 201309L

template <typename T>
struct is_null_pointer : std::is_same<decltype(nullptr), std::remove_cv_t<T>> {
};

#endif  // __cpp_lib_is_null_pointer

_PW_POLYFILL_END_NAMESPACE_STD
