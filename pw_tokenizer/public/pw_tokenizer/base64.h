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

// This file provides functions for working with the prefixed Base64 format for
// tokenized messages. This format is useful for transmitting tokenized messages
// as plain text.
//
// The format uses a prefix character ($), followed by the Base64 version of the
// tokenized message. For example, consider a tokenized message with token
// 0xfeb35a42 and encoded argument 0x13. This messsage would be encoded as
// follows:
//
//            Binary: 42 5a b3 fe 13  [5 bytes]
//
//   Prefixed Base64: $Qlqz/hM=       [9 bytes]
//
#pragma once

#include <stddef.h>

#include "pw_preprocessor/util.h"

// This character is used to mark the start of a Base64-encoded tokenized
// message. For consistency, it is recommended to always use $ if possible.
// If required, a different non-Base64 character may be used as a prefix.
#define PW_TOKENIZER_BASE64_PREFIX '$'

PW_EXTERN_C_START

// Encodes a binary tokenized message as prefixed Base64. Returns the size of
// the number of characters written to output_buffer. Returns 0 if the buffer is
// too small.
//
// Equivalent to pw::tokenizer::PrefixedBase64Encode.
size_t pw_TokenizerPrefixedBase64Encode(const void* binary_message,
                                        size_t binary_size_bytes,
                                        void* output_buffer,
                                        size_t output_buffer_size_bytes);
// Decodes a prefixed Base64 tokenized message to binary. Returns the size of
// the decoded binary data. The resulting data is ready to be passed to
// pw::tokenizer::Detokenizer::Detokenize. Returns 0 if the buffer is too small,
// the expected prefix character is missing, or the Base64 data is corrupt.
//
// Equivalent to pw::tokenizer::PrefixedBase64Encode.
size_t pw_TokenizerPrefixedBase64Decode(const void* base64_message,
                                        size_t base64_size_bytes,
                                        void* output_buffer,
                                        size_t output_buffer_size);

PW_EXTERN_C_END

#ifdef __cplusplus

#include <span>
#include <string_view>

namespace pw::tokenizer {

inline constexpr char kBase64Prefix = PW_TOKENIZER_BASE64_PREFIX;

// Encodes a binary tokenized message as prefixed Base64. Returns the size of
// the number of characters written to output_buffer. Returns 0 if the buffer is
// too small or does not start with kBase64Prefix.
inline size_t PrefixedBase64Encode(std::span<const std::byte> binary_message,
                                   std::span<char> output_buffer) {
  return pw_TokenizerPrefixedBase64Encode(binary_message.data(),
                                          binary_message.size(),
                                          output_buffer.data(),
                                          output_buffer.size());
}

// Also accept a std::span<const uint8_t> for the binary message.
inline size_t PrefixedBase64Encode(std::span<const uint8_t> binary_message,
                                   std::span<char> output_buffer) {
  return PrefixedBase64Encode(std::as_bytes(binary_message), output_buffer);
}

// Decodes a prefixed Base64 tokenized message to binary. Returns the size of
// the decoded binary data. The resulting data is ready to be passed to
// pw::tokenizer::Detokenizer::Detokenize.
inline size_t PrefixedBase64Decode(std::string_view base64_message,
                                   std::span<std::byte> output_buffer) {
  return pw_TokenizerPrefixedBase64Decode(base64_message.data(),
                                          base64_message.size(),
                                          output_buffer.data(),
                                          output_buffer.size());
}

// Decodes a prefixed Base64 tokenized message to binary in place. Returns the
// size of the decoded binary data.
inline size_t PrefixedBase64DecodeInPlace(std::span<std::byte> buffer) {
  return pw_TokenizerPrefixedBase64Decode(
      buffer.data(), buffer.size(), buffer.data(), buffer.size());
}

}  // namespace pw::tokenizer

#endif  // __cplusplus
