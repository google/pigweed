// Copyright 2019 The Pigweed Authors
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

#include "pw_string/type_to_string.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <limits>

#include "lib/stdcompat/bit.h"

namespace pw::string {

StatusWithSize IntToHexString(uint64_t value,
                              span<char> buffer,
                              uint_fast8_t min_width) {
  const uint_fast8_t digits = std::max(HexDigitCount(value), min_width);

  if (digits >= buffer.size()) {
    return internal::HandleExhaustedBuffer(buffer);
  }

  for (int i = static_cast<int>(digits) - 1; i >= 0; --i) {
    buffer[static_cast<size_t>(i)] = "0123456789abcdef"[value & 0xF];
    value >>= 4;
  }

  buffer[digits] = '\0';
  return StatusWithSize(digits);
}

StatusWithSize FloatAsIntToString(float value, span<char> buffer) {
  // If it's finite and fits in an int64_t, print it as a rounded integer.
  if (std::isfinite(value) &&
      std::abs(value) <
          static_cast<float>(std::numeric_limits<int64_t>::max())) {
    return IntToString<int64_t>(static_cast<int64_t>(std::roundf(value)),
                                buffer);
  }

  // Otherwise, print inf or NaN, if they fit.
  if (const size_t written = 3 + std::signbit(value); written < buffer.size()) {
    char* out = buffer.data();
    if (std::signbit(value)) {
      *out++ = '-';
    }
    std::memcpy(out, std::isnan(value) ? "NaN" : "inf", sizeof("NaN"));
    return StatusWithSize(written);
  }

  return internal::HandleExhaustedBuffer(buffer);
}

StatusWithSize BoolToString(bool value, span<char> buffer) {
  return CopyEntireStringOrNull(value ? "true" : "false", buffer);
}

StatusWithSize PointerToString(const void* pointer, span<char> buffer) {
  if (pointer == nullptr) {
    return CopyEntireStringOrNull(kNullPointerString, buffer);
  }
  return IntToHexString(reinterpret_cast<uintptr_t>(pointer), buffer);
}

StatusWithSize CopyEntireStringOrNull(std::string_view value,
                                      span<char> buffer) {
  if (value.size() >= buffer.size()) {
    return internal::HandleExhaustedBuffer(buffer);
  }

  std::memcpy(buffer.data(), value.data(), value.size());
  buffer[value.size()] = '\0';
  return StatusWithSize(value.size());
}

}  // namespace pw::string
