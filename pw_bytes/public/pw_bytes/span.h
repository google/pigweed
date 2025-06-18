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

#include <cstddef>
#include <memory>
#include <type_traits>

#include "pw_span/span.h"

namespace pw {

// Aliases for spans of bytes.
using ByteSpan = span<std::byte>;

using ConstByteSpan = span<const std::byte>;

/// Gets a read-only `pw::span<const std::byte>` (`ConstByteSpan`) view of an
/// object.
///
/// The returned span has a static extent.
template <typename T>
span<const std::byte, sizeof(T)> ObjectAsBytes(const T& obj) {
  static_assert(std::is_trivially_copyable_v<T>);
  static_assert(std::has_unique_object_representations_v<T>);

  auto s = pw::span<const T, 1>(std::addressof(obj), 1);
  return pw::as_bytes(s);
}

/// Gets a writable `pw::span<std::byte>` (`ByteSpan`) view of an object.
///
/// The returned span has a static extent.
template <typename T>
span<std::byte, sizeof(T)> ObjectAsWritableBytes(T& obj) {
  static_assert(std::is_trivially_copyable_v<T>);
  static_assert(std::has_unique_object_representations_v<T>);

  auto s = pw::span<T, 1>(std::addressof(obj), 1);
  return pw::as_writable_bytes(s);
}

}  // namespace pw
