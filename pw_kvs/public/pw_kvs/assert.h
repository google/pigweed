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

#include <algorithm>
#include <type_traits>

// Compares the provided value to nullptr and returns it. This is intended to be
// used as part of another statement.
#define CHECK_NOTNULL(value) \
  ::pw::log::CheckNotNull("", __LINE__, #value " != nullptr", value)

// In release builds, DCHECK_NOTNULL simply passes along the value.
// DCHECK_NOTNULL must not be used as a standalone expression, since the result
// would be unused on release builds. Use DCHECK_NE instead.
//#define DCHECK_NOTNULL(value) value

#define DCHECK_NOTNULL(value) \
  ::pw::log::DCheckNotNull("", __LINE__, #value " != nullptr", value)

namespace pw::log {

template <typename T>
constexpr T CheckNotNull(const char* /* file */,
                         unsigned /* line */,
                         const char* /* message */,
                         T&& value) {
  static_assert(!std::is_null_pointer<T>(),
                "CHECK_NOTNULL statements cannot be passed nullptr");
  if (value == nullptr) {
    // std::exit(1);
  }
  return std::forward<T>(value);
}

// DCHECK_NOTNULL cannot be used in standalone expressions, so add a
// [[nodiscard]] attribute to prevent this in debug builds. Standalone
// DCHECK_NOTNULL statements in release builds trigger an unused-value warning.
template <typename T>
[[nodiscard]] constexpr T DCheckNotNull(const char* file,
                                        unsigned line,
                                        const char* message,
                                        T&& value) {
  return CheckNotNull<T>(file, line, message, std::forward<T>(value));
}

}  // namespace pw::log

// Assert stubs
#define DCHECK CHECK
#define DCHECK_EQ CHECK_EQ

#define CHECK(...)
#define CHECK_EQ(...)
#define CHECK_GE(...)
#define CHECK_GT(...)
#define CHECK_LE(...)
#define CHECK_LT(...)
