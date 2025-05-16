// Copyright 2025 The Pigweed Authors
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
#include <array>
#include <cstdint>
#include <string_view>

#include "pw_assert/assert.h"
#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_string/hex.h"
#include "pw_string/string.h"

namespace pw::uuid {

/// Represents a 128-bit universally unique identifier (UUID).
class Uuid {
 public:
  /// Size of the UUID in bytes.
  static constexpr size_t kSizeBytes = 16;
  /// Length of the UUID's string representation.
  static constexpr size_t kStringSize =
      std::string_view{"xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"}.size();

  /// Create a Uuid from a const uint8_t span
  ///
  /// @param span span containing uuid
  static constexpr Result<Uuid> FromSpan(span<const uint8_t> span) {
    Status status = ValidateSpan(span);
    if (!status.ok()) {
      return status;
    }
    return Uuid(span);
  }

  /// Create a Uuid from a const std::byte span
  ///
  /// @param span span containing uuid
  static Result<Uuid> FromSpan(ConstByteSpan span) {
    return FromSpan(
        {reinterpret_cast<const uint8_t*>(span.data()), span.size()});
  }

  /// Create a Uuid from a string
  ///
  /// @param string string containing uuid
  static constexpr Result<Uuid> FromString(std::string_view string) {
    Status status = ValidateString(string);
    if (!status.ok()) {
      return status;
    }
    return Uuid(string);
  }

  /// Return the backing span holding the uuid
  constexpr pw::span<const uint8_t, kSizeBytes> GetSpan() const {
    return uuid_;
  }

  constexpr bool operator==(const Uuid& other) const {
    for (size_t i = 0; i < kSizeBytes; i++) {
      if (uuid_[i] != other.uuid_[i]) {
        return false;
      }
    }
    return true;
  }

  constexpr bool operator!=(const Uuid& other) const {
    return !(*this == other);
  }

  /// Convert the Uuid to a human readable string
  constexpr InlineString<kStringSize> ToString() const {
    InlineString<kStringSize> out = {};

    for (size_t i = uuid_.size(); i-- != 0;) {
      out += string::NibbleToHex(uuid_[i] >> 4);
      out += string::NibbleToHex(uuid_[i] & 0xf);
      if ((i == 12) || (i == 10) || (i == 8) || (i == 6)) {
        out += '-';
      }
    }
    return out;
  }

  // Default constructor initializes the uuid to the Nil UUID
  constexpr explicit Uuid() : uuid_() {}

 private:
  /// Create a Uuid from a const uint8_t span
  ///
  /// @param span span containing uuid
  constexpr explicit Uuid(span<const uint8_t> span) : uuid_() {
    PW_ASSERT(span.size() == uuid_.size());
    for (size_t i = 0; i < uuid_.size(); i++) {
      uuid_[i] = span[i];
    }
  }

  /// Create a Uuid from a string
  ///
  /// @param string string containing uuid
  constexpr explicit Uuid(std::string_view uuid_str) : uuid_() {
    size_t out_hex_index = 2 * uuid_.size();  // UUID is stored little-endian.
    for (size_t i = 0; i < kStringSize; i++) {
      // Indices at which we expect to find a hyphen ('-') in a UUID string.
      if (i == 8 || i == 13 || i == 18 || i == 23) {
        PW_ASSERT(uuid_str[i] == '-');
        continue;
      }
      PW_ASSERT(uuid_str[i] != '\0');

      out_hex_index--;
      uint16_t value = string::HexToNibble(uuid_str[i]);
      PW_ASSERT(value <= 0xf);
      if (out_hex_index % 2 == 0) {
        uuid_[out_hex_index / 2] |= static_cast<uint8_t>(value);
      } else {
        uuid_[out_hex_index / 2] = static_cast<uint8_t>(value << 4);
      }
    }
  }

  /// Validate a UUID string
  ///
  /// @param uuid_str string containing uuid
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The uuid is valid.
  ///    FAILED_PRECONDITION: The string is the wrong size
  ///    INVALID_ARGUMENT: The string is malformed
  static constexpr Status ValidateString(std::string_view uuid_str) {
    if (kStringSize != uuid_str.size()) {
      return Status::FailedPrecondition();
    }
    for (size_t i = 0; i < uuid_str.size(); i++) {
      if (i == 8 || i == 13 || i == 18 || i == 23) {
        if (uuid_str[i] != '-') {
          return Status::InvalidArgument();
        }
        continue;
      }
      if (string::IsHexDigit(uuid_str[i]) == 0) {
        return Status::InvalidArgument();
      }
    }
    return OkStatus();
  }

  /// Validate a UUID span
  ///
  /// @param span span containing uuid
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: The uuid is valid.
  ///    FAILED_PRECONDITION: The span is the wrong size
  static constexpr Status ValidateSpan(span<const uint8_t> span) {
    if (span.size() != kSizeBytes) {
      return Status::FailedPrecondition();
    }
    return OkStatus();
  }

  std::array<uint8_t, kSizeBytes> uuid_;
};

}  // namespace pw::uuid
