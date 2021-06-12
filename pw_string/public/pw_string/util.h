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
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"
#include "pw_string/internal/length.h"

namespace pw {
namespace string {

// Safe alternative to the string_view constructor to avoid the risk of an
// unbounded implicit or explicit use of strlen.
//
// This is strongly recommended over using something like C11's strnlen_s as
// a string_view does not require null-termination.
constexpr std::string_view ClampedCString(std::span<const char> str) {
  return std::string_view(str.data(),
                          internal::ClampedLength(str.data(), str.size()));
}

constexpr std::string_view ClampedCString(const char* str, size_t max_len) {
  return ClampedCString(std::span<const char>(str, max_len));
}

// Safe alternative to strlen to calculate the null-terminated length of the
// string within the specified span, excluding the null terminator. Like C11's
// strnlen_s, the scan for the null-terminator is bounded.
//
// Returns:
//   null-terminated length of the string excluding the null terminator.
//   OutOfRange - if the string is not null-terminated.
//
// Precondition: The string shall be at a valid pointer.
constexpr pw::Result<size_t> NullTerminatedLength(std::span<const char> str) {
  PW_DASSERT(str.data() != nullptr);

  const size_t length = internal::ClampedLength(str.data(), str.size());
  if (length == str.size()) {
    return Status::OutOfRange();
  }

  return length;
}

constexpr pw::Result<size_t> NullTerminatedLength(const char* str,
                                                  size_t max_len) {
  return NullTerminatedLength(std::span<const char>(str, max_len));
}

// Copies the source string to the dest, truncating if the full string does not
// fit. Always null terminates if dest.size() or num > 0.
//
// Returns the number of characters written, excluding the null terminator. If
// the string is truncated, the status is ResourceExhausted.
//
// Precondition: The destination and source shall not overlap.
// Precondition: The source shall be a valid pointer.
PW_CONSTEXPR_CPP20 inline StatusWithSize Copy(const std::string_view& source,
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

PW_CONSTEXPR_CPP20 inline StatusWithSize Copy(const char* source,
                                              std::span<char> dest) {
  PW_DASSERT(source != nullptr);
  return Copy(ClampedCString(source, dest.size()), dest);
}

PW_CONSTEXPR_CPP20 inline StatusWithSize Copy(const char* source,
                                              char* dest,
                                              size_t num) {
  return Copy(source, std::span<char>(dest, num));
}

}  // namespace string
}  // namespace pw
