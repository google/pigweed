// Copyright 2022 The Pigweed Authors
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

#include "pw_containers/vector.h"
#include "pw_status/status.h"
#include "pw_status/status_with_size.h"
#include "pw_string/util.h"

namespace pw {
namespace string {

// Copies the source string to the dest, truncating if the full string does not
// fit. dest will not be null terminated, instead the length is reflected in
// the vector size.
//
// Returns the number of characters written. If the string is truncated, the
// status is ResourceExhausted.
//
// Precondition: The destination and source shall not overlap.
// Precondition: The source shall be a valid pointer.
inline StatusWithSize Copy(const std::string_view& source,
                           pw::Vector<char>& dest) {
  if (dest.capacity() == 0) {
    return StatusWithSize::ResourceExhausted();
  }

  dest.resize(source.size());
  const size_t copied = source.copy(dest.data(), dest.size());

  return StatusWithSize(
      copied == source.size() ? OkStatus() : Status::ResourceExhausted(),
      copied);
}

inline StatusWithSize Copy(const char* source, pw::Vector<char>& dest) {
  PW_DASSERT(source != nullptr);
  return Copy(ClampedCString(source, dest.capacity()), dest);
}

inline StatusWithSize Copy(const pw::Vector<char>& source,
                           std::span<char> dest) {
  if (dest.empty()) {
    return StatusWithSize::ResourceExhausted();
  }

  const size_t copied = std::string_view(source.data(), source.size())
                            .copy(dest.data(), dest.size() - 1);
  dest[copied] = '\0';

  return StatusWithSize(
      copied == source.size() ? OkStatus() : Status::ResourceExhausted(),
      copied);
}

}  // namespace string
}  // namespace pw
