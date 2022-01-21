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

}  // namespace internal
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_TO_STRING_H_
