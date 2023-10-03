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

#include "pw_polyfill/standard_library/namespace.h"

_PW_POLYFILL_BEGIN_NAMESPACE_STD

#define __cpp_lib_transformation_trait_aliases 201304L
#define __cpp_lib_type_trait_variable_templates 201510L

template <decltype(sizeof(0)) kLength,
          decltype(sizeof(0)) kAlignment>  // no default
struct aligned_storage {
  struct type {
    alignas(kAlignment) unsigned char __data[kLength];
  };
};

template <decltype(sizeof(0)) kLength,
          decltype(sizeof(0)) kAlignment>  // no default
using aligned_storage_t = typename aligned_storage<kLength, kAlignment>::type;

#define __cpp_lib_integral_constant_callable 201304L

template <typename T, T kValue>
struct integral_constant {
  using value_type = T;
  using type = integral_constant;

  static constexpr T value = kValue;

  constexpr operator value_type() const noexcept { return value; }

  constexpr value_type operator()() const noexcept { return value; }
};

#define __cpp_lib_bool_constant 201505L

template <bool kValue>
using bool_constant = integral_constant<bool, kValue>;

using true_type = bool_constant<true>;
using false_type = bool_constant<false>;

template <typename T>
struct is_aggregate : bool_constant<__is_aggregate(T)> {};

template <typename T>
static constexpr bool is_aggregate_v = is_aggregate<T>::value;

template <typename T>
struct is_array : false_type {};
template <typename T>
struct is_array<T[]> : true_type {};
template <typename T, decltype(sizeof(int)) kSize>
struct is_array<T[kSize]> : true_type {};
template <typename T>
inline constexpr bool is_array_v = is_array<T>::value;

template <typename T>
struct is_const : false_type {};
template <typename T>
struct is_const<const T> : true_type {};
template <typename T>
inline constexpr bool is_const_v = is_const<T>::value;

template <typename T>
struct is_lvalue_reference : false_type {};
template <typename T>
struct is_lvalue_reference<T&> : true_type {};
template <typename T>
inline constexpr bool is_lvalue_reference_v = is_lvalue_reference<T>::value;

template <typename T>
struct is_rvalue_reference : false_type {};
template <typename T>
struct is_rvalue_reference<T&&> : true_type {};
template <typename T>
inline constexpr bool is_rvalue_reference_v = is_rvalue_reference<T>::value;

template <typename T>
struct remove_cv;  // Forward declaration

namespace impl {

template <typename T>
struct is_floating_point : false_type {};

template <>
struct is_floating_point<float> : true_type {};
template <>
struct is_floating_point<double> : true_type {};
template <>
struct is_floating_point<long double> : true_type {};

}  // namespace impl

template <typename T>
struct is_floating_point
    : impl::is_floating_point<typename remove_cv<T>::type> {};

template <typename T>
inline constexpr bool is_floating_point_v = is_floating_point<T>::value;

namespace impl {

template <typename T>
struct is_integral : false_type {};

template <>
struct is_integral<bool> : true_type {};
template <>
struct is_integral<char> : true_type {};
template <>
struct is_integral<char16_t> : true_type {};
template <>
struct is_integral<char32_t> : true_type {};
template <>
struct is_integral<wchar_t> : true_type {};

template <>
struct is_integral<short> : true_type {};
template <>
struct is_integral<unsigned short> : true_type {};
template <>
struct is_integral<int> : true_type {};
template <>
struct is_integral<unsigned int> : true_type {};
template <>
struct is_integral<long> : true_type {};
template <>
struct is_integral<unsigned long> : true_type {};
template <>
struct is_integral<long long> : true_type {};
template <>
struct is_integral<unsigned long long> : true_type {};

}  // namespace impl

template <typename T>
struct is_integral : impl::is_integral<typename remove_cv<T>::type> {};

template <typename T>
inline constexpr bool is_integral_v = is_integral<T>::value;

template <typename T>
struct is_arithmetic
    : bool_constant<is_integral_v<T> || is_floating_point_v<T>> {};

template <typename T>
inline constexpr bool is_arithmetic_v = is_arithmetic<T>::value;

#define __cpp_lib_is_null_pointer 201309L

template <typename T>
struct is_null_pointer : false_type {};

template <>
struct is_null_pointer<decltype(nullptr)> : true_type {};

template <typename T>
inline constexpr bool is_null_pointer_v = is_null_pointer<T>::value;

template <typename T>
struct is_pointer : false_type {};

template <typename T>
struct is_pointer<T*> : true_type {};

template <typename T>
inline constexpr bool is_pointer_v = is_pointer<T>::value;

template <typename T, typename U>
struct is_same : false_type {};

template <typename T>
struct is_same<T, T> : true_type {};

template <typename T, typename U>
inline constexpr bool is_same_v = is_same<T, U>::value;

namespace impl {

template <typename T, bool = is_arithmetic<T>::value>
struct is_signed : integral_constant<bool, T(-1) < T(0)> {};

template <typename T>
struct is_signed<T, false> : false_type {};

}  // namespace impl

template <typename T>
struct is_signed : impl::is_signed<T>::type {};

template <typename T>
inline constexpr bool is_signed_v = is_signed<T>::value;

template <typename T>
struct is_unsigned : bool_constant<!is_signed_v<T>> {};

template <typename T>
inline constexpr bool is_unsigned_v = is_unsigned<T>::value;

template <typename T>
struct is_void : is_same<void, typename remove_cv<T>::type> {};

template <typename T>
inline constexpr bool is_void_v = is_void<T>::value;

template <bool kBool, typename TrueType, typename FalseType>
struct conditional {
  using type = TrueType;
};

template <typename TrueType, typename FalseType>
struct conditional<false, TrueType, FalseType> {
  using type = FalseType;
};

template <bool kBool, typename TrueType, typename FalseType>
using conditional_t = typename conditional<kBool, TrueType, FalseType>::type;

#define __cpp_lib_logical_traits 201510L

template <typename...>
struct conjunction;

template <>
struct conjunction<> : true_type {};

template <typename T>
struct conjunction<T> : T {};

template <typename First, typename... Others>
struct conjunction<First, Others...>
    : conditional_t<bool(First::value), conjunction<Others...>, First> {};

template <typename... Types>
inline constexpr bool conjunction_v = conjunction<Types...>::value;

template <typename...>
struct disjunction;

template <>
struct disjunction<> : false_type {};

template <typename T>
struct disjunction<T> : T {};

template <typename First, typename... Others>
struct disjunction<First, Others...>
    : conditional_t<bool(First::value), First, disjunction<Others...>> {};

template <typename... Types>
inline constexpr bool disjunction_v = disjunction<Types...>::value;

template <typename T>
struct negation : bool_constant<!bool(T::value)> {};

template <typename T>
inline constexpr bool negation_v = negation<T>::value;

template <bool kEnable, typename T = void>
struct enable_if {
  using type = T;
};

template <typename T>
struct enable_if<false, T> {};

template <bool kEnable, typename T = void>
using enable_if_t = typename enable_if<kEnable, T>::type;

template <typename T>
struct remove_const {
  using type = T;
};

template <typename T>
struct remove_const<const T> {
  using type = T;
};

template <typename T>
using remove_const_t = typename remove_const<T>::type;

template <typename T>
struct remove_volatile {
  using type = T;
};

template <typename T>
struct remove_volatile<volatile T> {
  using type = T;
};

template <typename T>
using remove_volatile_t = typename remove_volatile<T>::type;

template <typename T>
struct remove_cv {
  using type = remove_volatile_t<remove_const_t<T>>;
};

template <typename T>
using remove_cv_t = typename remove_cv<T>::type;

template <typename T>
struct remove_extent {
  using type = T;
};

template <typename T>
struct remove_extent<T[]> {
  using type = T;
};

template <typename T, decltype(sizeof(0)) kSize>
struct remove_extent<T[kSize]> {
  using type = T;
};

template <typename T>
using remove_extent_t = typename remove_extent<T>::type;

template <typename T>
struct remove_pointer {
  using type = T;
};

template <typename T>
struct remove_pointer<T*> {
  using type = T;
};

template <typename T>
struct remove_pointer<T* const> {
  using type = T;
};

template <typename T>
struct remove_pointer<T* volatile> {
  using type = T;
};

template <typename T>
struct remove_pointer<T* const volatile> {
  using type = T;
};

template <typename T>
using remove_pointer_t = typename remove_pointer<T>::type;

template <typename T>
struct remove_reference {
  using type = T;
};

template <typename T>
struct remove_reference<T&> {
  using type = T;
};

template <typename T>
struct remove_reference<T&&> {
  using type = T;
};

template <typename T>
using remove_reference_t = typename remove_reference<T>::type;

// NOT IMPLEMENTED: This implementation is INCOMPLETE, as it does not cover
// function types.
template <typename T>
struct decay {
 private:
  using U = remove_reference_t<T>;

 public:
  using type =
      conditional_t<is_array<U>::value, remove_extent_t<U>*, remove_cv_t<U>>;
};

template <typename T>
using decay_t = typename decay<T>::type;

#define __cpp_lib_type_identity 201806

template <class T>
struct type_identity {
  using type = T;
};

template <typename T>
using type_identity_t = typename type_identity<T>::type;

#define __cpp_lib_void_t 201411L

template <typename...>
using void_t = void;

namespace impl {

template <typename T>
type_identity<T&> AddLValueReference(int);

template <typename T>
type_identity<T> AddLValueReference(...);

template <typename T>
type_identity<T&&> AddRValueReference(int);

template <typename T>
type_identity<T> AddRValueReference(...);

}  // namespace impl

template <class T>
struct add_lvalue_reference : decltype(impl::AddLValueReference<T>(0)) {};

template <typename T>
using add_lvalue_reference_t = typename add_lvalue_reference<T>::type;

template <class T>
struct add_rvalue_reference : decltype(impl::AddRValueReference<T>(0)) {};

template <typename T>
using add_rvalue_reference_t = typename add_rvalue_reference<T>::type;

template <typename T>
add_rvalue_reference_t<T> declval() noexcept;

namespace impl {

template <class T>
struct add_const {
  typedef const T type;
};

}  // namespace impl

template <class T>
using add_const_t = typename impl::add_const<T>::type;

template <typename T>
struct make_signed;

template <typename T>
struct make_unsigned;

#define _PW_MAKE_SIGNED_SPECIALIZATION(base, signed_type, unsigned_type) \
  template <>                                                            \
  struct make_signed<base> {                                             \
    using type = signed_type;                                            \
  };                                                                     \
  template <>                                                            \
  struct make_unsigned<base> {                                           \
    using type = unsigned_type;                                          \
  }

_PW_MAKE_SIGNED_SPECIALIZATION(char, signed char, unsigned char);
_PW_MAKE_SIGNED_SPECIALIZATION(signed char, signed char, unsigned char);
_PW_MAKE_SIGNED_SPECIALIZATION(unsigned char, signed char, unsigned char);

_PW_MAKE_SIGNED_SPECIALIZATION(short, signed short, unsigned short);
_PW_MAKE_SIGNED_SPECIALIZATION(unsigned short, signed short, unsigned short);

_PW_MAKE_SIGNED_SPECIALIZATION(int, signed int, unsigned int);
_PW_MAKE_SIGNED_SPECIALIZATION(unsigned int, signed int, unsigned int);

_PW_MAKE_SIGNED_SPECIALIZATION(long, signed long, unsigned long);
_PW_MAKE_SIGNED_SPECIALIZATION(unsigned long, signed long, unsigned long);

_PW_MAKE_SIGNED_SPECIALIZATION(long long, signed short, unsigned short);
_PW_MAKE_SIGNED_SPECIALIZATION(unsigned long long,
                               signed short,
                               unsigned short);

// Skip specializations for char8_t, etc.

template <typename T>
using make_signed_t = typename make_signed<T>::type;

template <typename T>
using make_unsigned_t = typename make_unsigned<T>::type;

namespace impl {

template <typename>
using templated_true = true_type;

template <typename T>
auto returnable(int) -> templated_true<T()>;

template <typename>
auto returnable(...) -> false_type;

template <typename From, typename To>
auto convertible(int)
    -> templated_true<decltype(declval<void (&)(To)>()(declval<From>()))>;

template <typename, typename>
auto convertible(...) -> false_type;

}  // namespace impl

template <typename From, typename To>
struct is_convertible
    : bool_constant<(decltype(impl::returnable<To>(0))() &&
                     decltype(impl::convertible<From, To>(0))()) ||
                    (is_void_v<From> && is_void_v<To>)> {};

template <typename T, typename U>
inline constexpr bool is_convertible_v = is_convertible<T, U>::value;

template <typename T>
struct alignment_of : integral_constant<decltype(sizeof(int)), alignof(T)> {};
template <typename T>
inline constexpr decltype(sizeof(int)) alignment_of_v = alignment_of<T>::value;

#define PW_STDLIB_UNIMPLEMENTED(name) \
  [[deprecated(#name " is NOT IMPLEMENTED in pw_minimal_cpp_stdlib!")]]

// NOT IMPLEMENTED: Stubs are provided for these traits classes, but they do not
// return useful values. Many of these would require compiler builtins.
#define PW_BOOLEAN_TRAIT_NOT_SUPPORTED(name) \
  template <typename T>                      \
  struct name : false_type {};               \
  template <typename T>                      \
  PW_STDLIB_UNIMPLEMENTED(name)              \
  inline constexpr bool name##_v = name<T>::value

#define PW_BOOLEAN_TRAIT_NOT_SUPPORTED_2(name) \
  template <typename T, typename U>            \
  struct name : false_type {};                 \
  template <typename T, typename U>            \
  PW_STDLIB_UNIMPLEMENTED(name)                \
  inline constexpr bool name##_v = name<T, U>::value

#define PW_BOOLEAN_TRAIT_NOT_SUPPORTED_VARARGS(name) \
  template <typename T, typename... Args>            \
  struct name : false_type {};                       \
  template <typename T, typename... Args>            \
  PW_STDLIB_UNIMPLEMENTED(name)                      \
  inline constexpr bool name##_v = name<T, Args...>::value

PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_class);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_enum);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_function);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_member_function_pointer);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_member_object_pointer);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_union);

PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_compound);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_fundamental);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_member_pointer);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_object);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_reference);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_scalar);

PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_abstract);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_empty);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_final);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_pod);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_polymorphic);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_standard_layout);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_trivial);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_trivially_copyable);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_volatile);

PW_BOOLEAN_TRAIT_NOT_SUPPORTED_VARARGS(is_constructible);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED_VARARGS(is_nothrow_constructible);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED_VARARGS(is_trivially_constructible);

PW_BOOLEAN_TRAIT_NOT_SUPPORTED_VARARGS(is_default_constructible);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED_VARARGS(is_nothrow_default_constructible);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED_VARARGS(is_trivially_default_constructible);

PW_BOOLEAN_TRAIT_NOT_SUPPORTED_VARARGS(is_copy_constructible);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED_VARARGS(is_nothrow_copy_constructible);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED_VARARGS(is_trivially_copy_constructible);

PW_BOOLEAN_TRAIT_NOT_SUPPORTED_VARARGS(is_move_constructible);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED_VARARGS(is_nothrow_move_constructible);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED_VARARGS(is_trivially_move_constructible);

PW_BOOLEAN_TRAIT_NOT_SUPPORTED_VARARGS(is_assignable);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED_VARARGS(is_nothrow_assignable);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED_VARARGS(is_trivially_assignable);

PW_BOOLEAN_TRAIT_NOT_SUPPORTED_VARARGS(is_copy_assignable);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED_VARARGS(is_nothrow_copy_assignable);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED_VARARGS(is_trivially_copy_assignable);

PW_BOOLEAN_TRAIT_NOT_SUPPORTED_VARARGS(is_move_assignable);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED_VARARGS(is_nothrow_move_assignable);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED_VARARGS(is_trivially_move_assignable);

PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_destructible);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_nothrow_destructible);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_trivially_destructible);

PW_BOOLEAN_TRAIT_NOT_SUPPORTED(has_virtual_destructor);

template <typename T>
struct extent : integral_constant<decltype(sizeof(int)), 1> {};
template <typename T>
PW_STDLIB_UNIMPLEMENTED(extent)
inline constexpr decltype(sizeof(int)) extent_v = extent<T>::value;

template <typename T>
struct rank : integral_constant<decltype(sizeof(int)), 1> {};
template <typename T>
PW_STDLIB_UNIMPLEMENTED(rank)
inline constexpr decltype(sizeof(int)) rank_v = extent<T>::value;

PW_BOOLEAN_TRAIT_NOT_SUPPORTED_2(is_base_of);

PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_invocable_r);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_invocable);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_nothrow_invocable_r);
PW_BOOLEAN_TRAIT_NOT_SUPPORTED(is_nothrow_invocable);

template <typename T>
struct invoke_result {};
template <typename T>
using invoke_result_t = typename invoke_result<T>::type;

template <typename T>
struct underlying_type {
  using type = T;
};
template <typename T>
using underlying_type_t = typename underlying_type<T>::type;

#undef PW_BOOLEAN_TRAIT_NOT_SUPPORTED
#undef PW_BOOLEAN_TRAIT_NOT_SUPPORTED_2
#undef PW_BOOLEAN_TRAIT_NOT_SUPPORTED_VARARGS
#undef PW_STDLIB_UNIMPLEMENTED

_PW_POLYFILL_END_NAMESPACE_STD
