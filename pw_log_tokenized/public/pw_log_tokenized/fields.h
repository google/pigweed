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

#include <string_view>

#include "pw_log_tokenized/config.h"
#include "pw_status/status_with_size.h"

namespace pw::log_tokenized {

/// Parses metadata fields from a tokenized log format string.
///
/// This function iterates through a string, parsing key-value pairs that are
/// formatted in the `pw_log_tokenized` style. For each / field found, it
/// invokes the provided `field_consumer` function with the / extracted key and
/// value.
///
/// @param[in] string The string to parse.
/// @param[in] field_consumer A function or lambda to be called for each
///     parsed field. It must accept two `std::string_view` arguments:
///     the key and the value.
/// @returns @pw_status{StatusWithSize} instead of the number of fields parsed.
///     If the key is unterminated, returns @pw_status{DATA_LOSS} with the
///     number of fields parsed.
template <typename Function>
constexpr StatusWithSize ParseFields(
    std::string_view string,
    Function field_consumer,
    std::string_view field_prefix = PW_LOG_TOKENIZED_FIELD_PREFIX,
    std::string_view key_val_separator = PW_LOG_TOKENIZED_KEY_VALUE_SEPARATOR) {
  size_t fields_parsed = 0;
  if (string.empty() || string.find(field_prefix) != 0) {
    return StatusWithSize(0);
  }

  size_t value_end = 0;
  for (size_t key_start = field_prefix.size(); key_start < string.size();
       key_start = value_end + field_prefix.size()) {
    const size_t key_end = string.find(key_val_separator, key_start);
    if (key_end == std::string_view::npos) {
      return StatusWithSize::DataLoss(fields_parsed);
    }

    const size_t value_start = key_end + key_val_separator.size();
    value_end = string.find(field_prefix, value_start);
    if (value_end == std::string_view::npos) {
      value_end = string.size();
    }

    field_consumer(string.substr(key_start, key_end - key_start),
                   string.substr(value_start, value_end - value_start));
    fields_parsed += 1;
  }
  return StatusWithSize(fields_parsed);
}

}  // namespace pw::log_tokenized
