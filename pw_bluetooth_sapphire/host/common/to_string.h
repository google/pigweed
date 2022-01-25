// Copyright 2022 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_TO_STRING_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_TO_STRING_H_

#include <string>
#include <type_traits>
#include <utility>

namespace bt {
namespace detail {

// Contains a true value if type |const T&| has a valid |.ToString()| method.
template <typename T, typename = void>
struct HasToStringMemberFn : std::false_type {};

template <typename T>
struct HasToStringMemberFn<T, std::void_t<decltype(std::declval<const T&>().ToString())>>
    : std::is_same<std::string, decltype(std::declval<const T&>().ToString())> {};

template <typename T>
constexpr bool HasToStringMemberFnV = HasToStringMemberFn<T>::value;

}  // namespace detail

namespace internal {

// Instantiates on |T| to provide an implementation for bt_str(…) if T has a .ToString() method.
template <typename T>
std::enable_if_t<detail::HasToStringMemberFnV<T>, std::string> ToString(const T& value) {
  return value.ToString();
}

// Contains a true value if there is a valid overload for |bt::internal::ToString(const T&)|.
template <typename T, typename = void>
struct HasToString : std::false_type {};

template <typename T>
struct HasToString<T, std::void_t<decltype(::bt::internal::ToString(std::declval<const T&>()))>>
    : std::is_same<std::string, decltype(::bt::internal::ToString(std::declval<const T&>()))> {};

template <typename T>
constexpr bool HasToStringV = HasToString<T>::value;

}  // namespace internal

// Compatibility for ostream-style string conversions for ToString-able types in namespace bt
// _only_, due to argument-dependent lookup (ADL) rules. std::ostream is implied through a template
// parameter to avoid directly including <ostream>.
//
// This is particularly useful for GoogleTest expectations. When a check failure against a type like
// bt::UUID prints this:
//   24-byte object <00-00 00-00 00-00 00-00 FB-34 9B-5F 80-00 00-80 00-10 00-00 0D-18 00-00>
// then including this file will cause its value printer to use this overload. Note that only
// including this file is not enough for types in other namespaces, including those nested in bt.
// For example, using this with bt::gap::Peer in a test requires the following:
//   namespace bt::gap {
//   using ::bt::operator<<;
//   }  // namespace bt::gap
template <typename T, typename OStream>
std::enable_if_t<internal::HasToStringV<T>, OStream&> operator<<(OStream& os, const T& t) {
  return os << ::bt::internal::ToString(t);
}

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_TO_STRING_H_
