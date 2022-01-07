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

#ifndef __cpp_lib_bool_constant
#define __cpp_lib_bool_constant 201505L
template <bool kValue>
using bool_constant = integral_constant<bool, kValue>;
#endif  // __cpp_lib_bool_constant

#ifndef __cpp_lib_logical_traits
#define __cpp_lib_logical_traits 201510L
template <typename Value>
struct negation : bool_constant<!bool(Value::value)> {};

template <typename...>
struct conjunction : std::true_type {};
template <typename B1>
struct conjunction<B1> : B1 {};
template <typename B1, typename... Bn>
struct conjunction<B1, Bn...>
    : std::conditional_t<bool(B1::value), conjunction<Bn...>, B1> {};

template <typename...>
struct disjunction : std::false_type {};
template <typename B1>
struct disjunction<B1> : B1 {};
template <typename B1, typename... Bn>
struct disjunction<B1, Bn...>
    : std::conditional_t<bool(B1::value), B1, disjunction<Bn...>> {};

#endif  // __cpp_lib_logical_traits

_PW_POLYFILL_END_NAMESPACE_STD
