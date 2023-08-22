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

#include <cstddef>
#include <type_traits>

#include "pw_log_tokenized/config.h"
#include "pw_tokenizer/base64.h"

namespace pw::log_tokenized {

// Minimum capacity for a string that to hold the Base64-encoded version of a
// PW_LOG_TOKENIZED_ENCODING_BUFFER_SIZE_BYTES tokenized message. This is the
// capacity needed to encode to a `pw::InlineString` and does not include a null
// terminator.
inline constexpr size_t kBase64EncodedBufferSizeBytes =
    tokenizer::Base64EncodedBufferSize(kEncodingBufferSizeBytes);

/// Encodes a binary tokenized log in the prefixed Base64 format. Calls
/// @cpp_func{pw::tokenizer::PrefixedBase64Encode} for a string sized to fit a
/// `kEncodingBufferSizeBytes` tokenized log.
inline InlineString<kBase64EncodedBufferSizeBytes> PrefixedBase64Encode(
    span<const std::byte> binary_message) {
  return tokenizer::PrefixedBase64Encode<kEncodingBufferSizeBytes>(
      binary_message);
}

#ifndef PW_EXCLUDE_FROM_DOXYGEN  // Doxygen fails to parse this, so skip it.

template <typename T,
          typename = std::enable_if_t<sizeof(T) == sizeof(std::byte)>>
inline InlineString<kBase64EncodedBufferSizeBytes> PrefixedBase64Encode(
    const T* log_buffer, size_t size_bytes) {
  return PrefixedBase64Encode(as_bytes(span(log_buffer, size_bytes)));
}

#endif  // PW_EXCLUDE_FROM_DOXYGEN

}  // namespace pw::log_tokenized
