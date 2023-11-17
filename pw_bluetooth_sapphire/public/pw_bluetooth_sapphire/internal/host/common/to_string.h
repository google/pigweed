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

#pragma once
#include <string>
#include <type_traits>
#include <utility>

namespace bt {
namespace detail {

// Contains a true value if type |const T&| has a valid |.ToString()| method.
template <typename T, typename = void>
struct HasToStringMemberFn : std::false_type {};

template <typename T>
struct HasToStringMemberFn<
    T,
    std::void_t<decltype(std::declval<const T&>().ToString())>>
    : std::is_same<std::string, decltype(std::declval<const T&>().ToString())> {
};

template <typename T>
constexpr bool HasToStringMemberFnV = HasToStringMemberFn<T>::value;

}  // namespace detail

namespace internal {

// Instantiates on |T| to provide an implementation for bt_str(…) if T has a
// .ToString() method.
template <typename T>
std::enable_if_t<detail::HasToStringMemberFnV<T>, std::string> ToString(
    const T& value) {
  return value.ToString();
}

// Contains a true value if there is a valid overload for
// |bt::internal::ToString(const T&)|.
template <typename T, typename = void>
struct HasToString : std::false_type {};

template <typename T>
struct HasToString<
    T,
    std::void_t<decltype(::bt::internal::ToString(std::declval<const T&>()))>>
    : std::is_same<std::string,
                   decltype(::bt::internal::ToString(
                       std::declval<const T&>()))> {};

template <typename T>
constexpr bool HasToStringV = HasToString<T>::value;

}  // namespace internal

// Compatibility for ostream-style string conversions for ToString-able types in
// namespace bt _only_, due to argument-dependent lookup (ADL) rules.
// std::ostream is implied through a template parameter to avoid directly
// including <ostream>.
//
// This is particularly useful for GoogleTest expectations. When a check failure
// against a type like bt::UUID prints this:
//   24-byte object <00-00 00-00 00-00 00-00 FB-34 9B-5F 80-00 00-80 00-10 00-00
//   0D-18 00-00>
// then including this file will cause its value printer to use this overload.
// Note that only including this file is not enough for types in other
// namespaces, including those nested in bt. For example, using this with
// bt::gap::Peer in a test requires the following:
//   namespace bt::gap {
//   using ::bt::operator<<;
//   }  // namespace bt::gap
template <typename T,
          typename OStream,
          typename = std::enable_if_t<internal::HasToStringV<T>>>
auto& operator<<(OStream& os, const T& t) {
  return os << ::bt::internal::ToString(t);
}

}  // namespace bt

// Convenience macro for printf-style formatting of an object with a ToString()
// function overload e.g.:
//   bt_log(INFO, "tag", "foo happened: %s", bt_str(id));
//
// This library provides an ToString() overload that forwards to .ToString()
// method if it exists.
#define bt_str(id) (::bt::internal::ToString(id).c_str())
