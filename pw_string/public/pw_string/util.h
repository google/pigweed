// Copyright 2021 The Pigweed Authors
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
#include <span>
#include <string_view>

#include "pw_assert/assert.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"

namespace pw {
namespace string {

// Calculates the length of a null-terminated string up to the specified maximum
// length. If str is nullptr, returns 0.
//
// This function is a constexpr version of C11's strnlen_s.
constexpr size_t Length(const char* str, size_t max_len) {
  size_t length = 0;

  if (str != nullptr) {
    for (; length < max_len; ++length) {
      if (str[length] == '\0') {
        break;
      }
    }
  }

  return length;
}

// Copies the source string to the dest, truncating if the full string does not
// fit. Always null terminates if dest.size() or num > 0.
//
// Returns the number of characters written, excluding the null terminator. If
// the string is truncated, the status is ResourceExhausted.
//
// Precondition: The destination and source shall not overlap.
// Precondition: The source shall be a valid pointer.
constexpr StatusWithSize Copy(const std::string_view& source,
                              std::span<char> dest) {
  if (dest.empty()) {
    return StatusWithSize::ResourceExhausted();
  }

  const size_t copied = source.copy(dest.data(), dest.size() - 1);
  dest[copied] = '\0';

  return StatusWithSize(
      copied == source.size() ? OkStatus() : Status::ResourceExhausted(),
      copied);
}

constexpr StatusWithSize Copy(const char* source, std::span<char> dest) {
  PW_DASSERT(source != nullptr);
  return Copy(std::string_view(source, Length(source, dest.size())), dest);
}

constexpr StatusWithSize Copy(const char* source, char* dest, size_t num) {
  return Copy(source, std::span<char>(dest, num));
}

}  // namespace string
}  // namespace pw
