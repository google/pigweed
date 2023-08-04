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

// Functions for encoding and decoding data in Base64 as specified by RFC 3548
// and RFC 4648. See https://tools.ietf.org/html/rfc4648
#pragma once

#include <stdbool.h>
#include <stddef.h>

// C-compatible versions of a subset of the pw_base64 module.
#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// Returns the size of the given number of bytes when encoded as Base64. Base64
//
// Equivalent to pw::base64::EncodedSize().
#define PW_BASE64_ENCODED_SIZE(binary_size_bytes) \
  (((size_t)binary_size_bytes + 2) / 3 * 4)  // +2 to round up to a 3-byte group

// Encodes the provided data in Base64 and writes the result to the buffer.
// Exactly PW_BASE64_ENCODED_SIZE(binary_size_bytes) bytes will be written. The
// output buffer *MUST* be large enough for the encoded output!
//
// Equivalent to pw::base64::Encode().
void pw_Base64Encode(const void* binary_data,
                     const size_t binary_size_bytes,
                     char* output);

// Evaluates to the maximum size of decoded Base64 data in bytes.
//
// Equivalent to pw::base64::MaxDecodedSize().
#define PW_BASE64_MAX_DECODED_SIZE(base64_size_bytes) \
  (((size_t)base64_size_bytes) / 4 * 3)

// Decodes the provided Base64 data into raw binary. The output buffer *MUST* be
// at least PW_BASE64_MAX_DECODED_SIZE bytes large.
//
// Equivalent to pw::base64::Decode().
size_t pw_Base64Decode(const char* base64,
                       size_t base64_size_bytes,
                       void* output);

// Returns true if the provided string is valid Base64 encoded data. Accepts
// either the standard (+/) or URL-safe (-_) alphabets.
//
// Equivalent to pw::base64::IsValid().
bool pw_Base64IsValid(const char* base64_data, size_t base64_size);

// C++ API, which uses the C functions internally.
#ifdef __cplusplus
}  // extern "C"

#include <string_view>
#include <type_traits>

#include "pw_span/span.h"
#include "pw_string/string.h"

namespace pw::base64 {

/// @param[in] binary_size_bytes The size of the binary data in bytes, before
/// encoding.
///
/// @returns The size of `binary_size_bytes` after Base64 encoding.
///
/// @note Base64 encodes 3-byte groups into 4-character strings. The final group
/// is padded to be 3 bytes if it only has 1 or 2.
constexpr size_t EncodedSize(size_t binary_size_bytes) {
  return PW_BASE64_ENCODED_SIZE(binary_size_bytes);
}

/// Encodes the provided data in Base64 and writes the result to the buffer.
///
/// @param[in] binary The binary data to encode.
///
/// @param[out] The output buffer where the encoded data is placed. Exactly
/// `EncodedSize(binary_size_bytes)` bytes is written.
///
/// @note Encodes to the standard alphabet with `+` and `/` for characters `62`
/// and `63`.
///
/// @pre
/// * The output buffer **MUST** be large enough for the encoded output!
/// * The input and output buffers **MUST NOT** be the same; encoding cannot
///   occur in place.
///
/// @warning The resulting string in the output is **NOT** null-terminated!
inline void Encode(span<const std::byte> binary, char* output) {
  pw_Base64Encode(binary.data(), binary.size_bytes(), output);
}

/// Encodes the provided data in Base64 if the result fits in the provided
/// buffer.
///
/// @param[in] binary The binary data to encode.
///
/// @param[out] output_buffer The output buffer where the encoded data is
/// placed.
///
/// @warning The resulting string in the output is **NOT** null-terminated!
///
/// @returns The number of bytes written. Returns `0` if the output buffer
/// is too small.
size_t Encode(span<const std::byte> binary, span<char> output_buffer);

/// Appends Base64 encoded binary data to the provided `pw::InlineString`.
///
/// @param[in] binary The binary data that has already been Base64-encoded.
///
/// @param[out] output The `pw::InlineString` that `binary` is appended to.
///
/// If the data does not fit in the string, an assertion fails.
void Encode(span<const std::byte> binary, InlineString<>& output);

/// Creates a `pw::InlineString<>` large enough to hold
/// `kMaxBinaryDataSizeBytes` of binary data when encoded as Base64 and encodes
/// the provided span into it.
template <size_t kMaxBinaryDataSizeBytes>
inline InlineString<EncodedSize(kMaxBinaryDataSizeBytes)> Encode(
    span<const std::byte> binary) {
  InlineString<EncodedSize(kMaxBinaryDataSizeBytes)> output;
  Encode(binary, output);
  return output;
}

/// Calculates the maximum size of Base64-encoded data after decoding.
///
/// @param[in] base64_size_bytes The size of the Base64-encoded data.
///
/// @pre `base64_size_bytes` must be a multiple of 4, since Base64 encodes
/// 3-byte groups into 4-character strings.
///
/// @returns The maximum size of the Base64-encoded data represented by
/// `base64_bytes_size` after decoding. If the last 3-byte group has padding,
/// the actual decoded size will be 1 or 2 bytes less than the value returned
/// by `MaxDecodedSize()`.
constexpr size_t MaxDecodedSize(size_t base64_size_bytes) {
  return PW_BASE64_MAX_DECODED_SIZE(base64_size_bytes);
}

/// Decodes the provided Base64 data into raw binary.
///
/// @pre
/// * The output buffer **MUST** be at least `MaxDecodedSize()` bytes large.
/// * This function does NOT check that the input is valid! Use `IsValid()`
///   or the four-argument overload to check the input formatting.
///
/// @param[in] base64 The Base64 data that should be decoded. Can be encoded
/// with either the standard (`+/`) or URL-safe (`-_`) alphabet. The data must
/// be padded to 4-character blocks with `=`.
///
/// @param[out] output The output buffer where the raw binary will be placed.
/// The output buffer may be the same as the input buffer; decoding can occur
/// in place.
///
/// @returns The number of bytes that were decoded.
inline size_t Decode(std::string_view base64, void* output) {
  return pw_Base64Decode(base64.data(), base64.size(), output);
}

/// Decodes the provided Base64 data, if the data is valid and fits in the
/// output buffer.
///
/// @returns The number of bytes written, which will be `0` if the data is
/// invalid or doesn't fit.
size_t Decode(std::string_view base64, span<std::byte> output_buffer);

/// Decodes a `pw::InlineString<>` in place.
template <typename T>
inline void DecodeInPlace(InlineBasicString<T>& buffer) {
  static_assert(sizeof(T) == sizeof(char));
  buffer.resize(Decode(buffer, buffer.data()));
}

/// @param[in] base64 The string to check. Can be encoded with either the
/// standard (`+/`) or URL-safe (`-_`) alphabet.
///
/// @returns `true` if the provided string is valid Base64-encoded data.
inline bool IsValid(std::string_view base64) {
  return pw_Base64IsValid(base64.data(), base64.size());
}

}  // namespace pw::base64

#endif  // __cplusplus
