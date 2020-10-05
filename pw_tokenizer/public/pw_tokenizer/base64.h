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

// Encodes a binary tokenized message as prefixed Base64 with a null terminator.
// Returns the encoded string length (excluding the null terminator). Returns 0
// if the buffer is too small. Always null terminates if the output buffer is
// not empty.
//
// Equivalent to pw::tokenizer::PrefixedBase64Encode.
size_t pw_tokenizer_PrefixedBase64Encode(const void* binary_message,
                                         size_t binary_size_bytes,
                                         void* output_buffer,
                                         size_t output_buffer_size_bytes);

// Decodes a prefixed Base64 tokenized message to binary. Returns the size of
// the decoded binary data. The resulting data is ready to be passed to
// pw::tokenizer::Detokenizer::Detokenize. Returns 0 if the buffer is too small,
// the expected prefix character is missing, or the Base64 data is corrupt.
//
// Equivalent to pw::tokenizer::PrefixedBase64Encode.
size_t pw_tokenizer_PrefixedBase64Decode(const void* base64_message,
                                         size_t base64_size_bytes,
                                         void* output_buffer,
                                         size_t output_buffer_size);

PW_EXTERN_C_END

#ifdef __cplusplus

#include <span>
#include <string_view>

#include "pw_base64/base64.h"
#include "pw_containers/vector.h"
#include "pw_tokenizer/config.h"

namespace pw::tokenizer {

inline constexpr char kBase64Prefix = PW_TOKENIZER_BASE64_PREFIX;

// Returns the size of a Base64-encoded tokenized message. Includes the prefix
// character ($) and the encoded data, but excludes the null terminator.
constexpr size_t Base64EncodedSize(size_t data) {
  return sizeof(kBase64Prefix) + base64::EncodedSize(data);
}

// The minimum buffer size that can hold a tokenized message that is
// PW_TOKENIZER_CFG_ENCODING_BUFFER_SIZE_BYTES long encoded as prefixed Base64.
inline constexpr size_t kBase64EncodedBufferSize =
    Base64EncodedSize(PW_TOKENIZER_CFG_ENCODING_BUFFER_SIZE_BYTES) +
    sizeof('\0');

// Encodes a binary tokenized message as prefixed Base64 with a null terminator.
// Returns the encoded string length (excluding the null terminator). Returns 0
// if the buffer is too small. Always null terminates if the output buffer is
// not empty.
inline size_t PrefixedBase64Encode(std::span<const std::byte> binary_message,
                                   std::span<char> output_buffer) {
  return pw_tokenizer_PrefixedBase64Encode(binary_message.data(),
                                           binary_message.size(),
                                           output_buffer.data(),
                                           output_buffer.size());
}

// Also accept a std::span<const uint8_t> for the binary message.
inline size_t PrefixedBase64Encode(std::span<const uint8_t> binary_message,
                                   std::span<char> output_buffer) {
  return PrefixedBase64Encode(std::as_bytes(binary_message), output_buffer);
}

// Encodes to a pw::Vector, which defaults to fit a message
// PW_TOKENIZER_CFG_ENCODING_BUFFER_SIZE_BYTES long encoded as prefixed Base64.
// The returned vector always contains a null-terminated Base64 string. If
// size() is zero, the binary message did not fit.
template <size_t buffer_size = kBase64EncodedBufferSize>
Vector<char, buffer_size> PrefixedBase64Encode(
    std::span<const std::byte> binary_message) {
  static_assert(buffer_size >= Base64EncodedSize(sizeof(uint32_t)));

  Vector<char, buffer_size> output;
  const size_t encoded_size = Base64EncodedSize(binary_message.size());

  // Make sure the encoded data and a null terminator can fit.
  if (encoded_size + sizeof('\0') > buffer_size) {
    output[0] = '\0';
    return output;
  }

  output.resize(encoded_size);
  output[0] = kBase64Prefix;
  base64::Encode(binary_message, &output[1]);
  output[encoded_size] = '\0';
  return output;
}

// Encode to a pw::Vector from std::span<const uint8_t>.
template <size_t buffer_size = kBase64EncodedBufferSize>
Vector<char, buffer_size> PrefixedBase64Encode(
    std::span<const uint8_t> binary_message) {
  return PrefixedBase64Encode(std::as_bytes(binary_message));
}

// Decodes a prefixed Base64 tokenized message to binary. Returns the size of
// the decoded binary data. The resulting data is ready to be passed to
// pw::tokenizer::Detokenizer::Detokenize.
inline size_t PrefixedBase64Decode(std::string_view base64_message,
                                   std::span<std::byte> output_buffer) {
  return pw_tokenizer_PrefixedBase64Decode(base64_message.data(),
                                           base64_message.size(),
                                           output_buffer.data(),
                                           output_buffer.size());
}

// Decodes a prefixed Base64 tokenized message to binary in place. Returns the
// size of the decoded binary data.
inline size_t PrefixedBase64DecodeInPlace(std::span<std::byte> buffer) {
  return pw_tokenizer_PrefixedBase64Decode(
      buffer.data(), buffer.size(), buffer.data(), buffer.size());
}

}  // namespace pw::tokenizer

#endif  // __cplusplus
