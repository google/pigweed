// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIB_STDCOMPAT_TYPE_TRAITS_H_
#define LIB_STDCOMPAT_TYPE_TRAITS_H_

#include <cstddef>
#include <type_traits>

#include "internal/type_traits.h"  // IWYU pragma: keep
#include "version.h"               // IWYU pragma: keep

namespace cpp17 {

using std::void_t;

using std::bool_constant;
using std::conjunction;
using std::conjunction_v;
using std::disjunction;
using std::disjunction_v;
using std::negation;
using std::negation_v;

using std::is_array_v;
using std::is_class_v;
using std::is_enum_v;
using std::is_floating_point_v;
using std::is_function_v;
using std::is_integral_v;
using std::is_lvalue_reference_v;
using std::is_member_function_pointer_v;
using std::is_member_object_pointer_v;
using std::is_null_pointer_v;
using std::is_pointer_v;
using std::is_rvalue_reference_v;
using std::is_union_v;
using std::is_void_v;

using std::is_arithmetic_v;
using std::is_compound_v;
using std::is_fundamental_v;
using std::is_member_pointer_v;
using std::is_object_v;
using std::is_reference_v;
using std::is_scalar_v;

using std::is_abstract_v;
using std::is_const_v;
using std::is_empty_v;
using std::is_final_v;
using std::is_pod_v;
using std::is_polymorphic_v;
using std::is_signed_v;
using std::is_standard_layout_v;
using std::is_trivial_v;
using std::is_trivially_copyable_v;
using std::is_unsigned_v;
using std::is_volatile_v;

using std::is_constructible_v;
using std::is_nothrow_constructible_v;
using std::is_trivially_constructible_v;

using std::is_default_constructible_v;
using std::is_nothrow_default_constructible_v;
using std::is_trivially_default_constructible_v;

using std::is_copy_constructible_v;
using std::is_nothrow_copy_constructible_v;
using std::is_trivially_copy_constructible_v;

using std::is_move_constructible_v;
using std::is_nothrow_move_constructible_v;
using std::is_trivially_move_constructible_v;

using std::is_assignable_v;
using std::is_nothrow_assignable_v;
using std::is_trivially_assignable_v;

using std::is_copy_assignable_v;
using std::is_nothrow_copy_assignable_v;
using std::is_trivially_copy_assignable_v;

using std::is_move_assignable_v;
using std::is_nothrow_move_assignable_v;
using std::is_trivially_move_assignable_v;

using std::is_destructible_v;
using std::is_nothrow_destructible_v;
using std::is_trivially_destructible_v;

using std::has_virtual_destructor_v;

using std::alignment_of_v;
using std::extent_v;
using std::rank_v;

using std::is_base_of_v;
using std::is_convertible_v;
using std::is_same_v;

using std::is_aggregate;
using std::is_aggregate_v;

using std::is_invocable;
using std::is_invocable_r;
using std::is_nothrow_invocable;
using std::is_nothrow_invocable_r;

using std::is_invocable_r_v;
using std::is_invocable_v;
using std::is_nothrow_invocable_r_v;
using std::is_nothrow_invocable_v;

using std::invoke_result;
using std::invoke_result_t;

}  // namespace cpp17

namespace cpp20 {

#if defined(__cpp_lib_bounded_array_traits) && __cpp_lib_bounded_array_traits >= 201902L && \
    !defined(LIB_STDCOMPAT_USE_POLYFILLS)

using std::is_bounded_array;
using std::is_bounded_array_v;

using std::is_unbounded_array;
using std::is_unbounded_array_v;

#else  // Provide polyfills for std::is_{,un}bounded_array{,_v}

template <typename T>
struct is_bounded_array : std::false_type {};
template <typename T, std::size_t N>
struct is_bounded_array<T[N]> : std::true_type {};

template <typename T>
static constexpr bool is_bounded_array_v = is_bounded_array<T>::value;

template <typename T>
struct is_unbounded_array : std::false_type {};
template <typename T>
struct is_unbounded_array<T[]> : std::true_type {};

template <typename T>
static constexpr bool is_unbounded_array_v = is_unbounded_array<T>::value;

#endif  // __cpp_lib_bounded_array_traits >= 201902L && !defined(LIB_STDCOMPAT_USE_POLYFILLS)

#if defined(__cpp_lib_remove_cvref) && __cpp_lib_remove_cvref >= 201711L && \
    !defined(LIB_STDCOMPAT_USE_POLYFILLS)

using std::remove_cvref;
using std::remove_cvref_t;

#else  // Provide polyfill for std::remove_cvref{,_t}

template <typename T>
struct remove_cvref {
  using type = std::remove_cv_t<std::remove_reference_t<T>>;
};

template <typename T>
using remove_cvref_t = typename remove_cvref<T>::type;

#endif  // __cpp_lib_remove_cvref >= 201711L && !defined(LIB_STDCOMPAT_USE_POLYFILLS)

#if defined(__cpp_lib_type_identity) && __cpp_lib_type_identity >= 201806L && \
    !defined(LIB_STDCOMPAT_USE_POLYFILLS)

using std::type_identity;
using std::type_identity_t;

#else  // Provide polyfill for std::type_identity{,_t}

template <typename T>
struct type_identity {
  using type = T;
};

template <typename T>
using type_identity_t = typename type_identity<T>::type;

#endif  // __cpp_lib_type_identity >= 201806L && !defined(LIB_STDCOMPAT_USE_POLYFILLS)

#if defined(__cpp_lib_is_constant_evaluated) && __cpp_lib_is_constant_evaluated >= 201811L && \
    !defined(LIB_STDCOMPAT_USE_POLYFILLS)

#define LIB_STDCOMPAT_CONSTEVAL_SUPPORT 1
using std::is_constant_evaluated;

#else  // Provide polyfill for std::is_constant_evaluated

#ifdef __has_builtin
#if __has_builtin(__builtin_is_constant_evaluated)

#define LIB_STDCOMPAT_CONSTEVAL_SUPPORT 1
inline constexpr bool is_constant_evaluated() noexcept { return __builtin_is_constant_evaluated(); }

#endif  // __has_builtin(__builtin_is_constant_evaluated)
#endif  // __has_builtin

#ifndef LIB_STDCOMPAT_CONSTEVAL_SUPPORT

#define LIB_STDCOMPAT_CONSTEVAL_SUPPORT 0
inline constexpr bool is_constant_evaluated() noexcept { return false; }

#endif  // LIB_STDCOMPAT_CONSTEVAL_SUPPORT

#endif  // __cpp_lib_is_constant_evaluated >= 201811L && !defined(LIB_STDCOMPAT_USE_POLYFILLS)

}  // namespace cpp20

namespace cpp23 {

#if defined(__cpp_lib_is_scoped_enum) && __cpp_lib_is_scoped_enum >= 202011L && \
    !defined(LIB_STDCOMPAT_USE_POLYFILLS)

using std::is_scoped_enum;
using std::is_scoped_enum_v;

#else  // Provide polyfill for std::is_scoped_enum{,_v}

template <typename T, typename = void>
struct is_scoped_enum : std::false_type {};

template <typename T>
struct is_scoped_enum<T, std::enable_if_t<cpp17::is_enum_v<T>>>
    : cpp17::bool_constant<!cpp17::is_convertible_v<T, std::underlying_type_t<T>>> {};

template <typename T>
static constexpr bool is_scoped_enum_v = is_scoped_enum<T>::value;

#endif  // __cpp_lib_is_scoped_enum >= 202011L && !defined(LIB_STDCOMPAT_USE_POLYFILLS)

}  // namespace cpp23

#endif  // LIB_STDCOMPAT_TYPE_TRAITS_H_
