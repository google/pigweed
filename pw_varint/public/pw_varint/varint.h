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

/// @file pw_varint/varint.h
///
/// The `pw_varint` module provides functions for encoding and decoding variable
/// length integers or varints. For smaller values, varints require less memory
/// than a fixed-size encoding. For example, a 32-bit (4-byte) integer requires
/// 1–5 bytes when varint-encoded.
///
/// `pw_varint` supports custom variable-length encodings with different
/// terminator bit values and positions (@cpp_enum{pw::varint::Format}).
/// The basic encoding for unsigned integers is Little Endian Base 128 (LEB128).
/// ZigZag encoding is also supported, which maps negative integers to positive
/// integers to improve encoding density for LEB128.
///
/// <a
/// href=https://developers.google.com/protocol-buffers/docs/encoding#varints>
/// Protocol Buffers</a> and @rstref{HDLC <module-pw_hdlc>} use variable-length
/// integer encodings for integers.

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "pw_preprocessor/compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Maximum size of an LEB128-encoded `uint32_t`.
#define PW_VARINT_MAX_INT32_SIZE_BYTES 5

/// Maximum size of an LEB128-encoded `uint64_t`.
#define PW_VARINT_MAX_INT64_SIZE_BYTES 10

/// Encodes a 32-bit integer as LEB128.
///
/// @returns the number of bytes written
size_t pw_varint_Encode32(uint32_t integer,
                          void* output,
                          size_t output_size_bytes);

/// Encodes a 64-bit integer as LEB128.
///
/// @returns the number of bytes written
size_t pw_varint_Encode64(uint64_t integer,
                          void* output,
                          size_t output_size_bytes);

/// Zig-zag encodes an `int32_t`, returning it as a `uint32_t`.
static inline uint32_t pw_varint_ZigZagEncode32(int32_t n) {
  return (uint32_t)((uint32_t)n << 1) ^ (uint32_t)(n >> (sizeof(n) * 8 - 1));
}

/// Zig-zag encodes an `int64_t`, returning it as a `uint64_t`.
static inline uint64_t pw_varint_ZigZagEncode64(int64_t n) {
  return (uint64_t)((uint64_t)n << 1) ^ (uint64_t)(n >> (sizeof(n) * 8 - 1));
}

/// Extracts and encodes 7 bits from the integer. Sets the top bit to indicate
/// more data is coming, which must be cleared if this was the last byte.
static inline uint8_t pw_varint_EncodeOneByte32(uint32_t* integer) {
  const uint8_t bits = (uint8_t)((*integer & 0x7Fu) | 0x80u);
  *integer >>= 7;
  return bits;
}

/// @copydoc pw_varint_EncodeOneByte32
static inline uint8_t pw_varint_EncodeOneByte64(uint64_t* integer) {
  const uint8_t bits = (uint8_t)((*integer & 0x7Fu) | 0x80u);
  *integer >>= 7;
  return bits;
}

/// Zig-zag decodes a `uint64_t`, returning it as an `int64_t`.
static inline int32_t pw_varint_ZigZagDecode32(uint32_t n)
    PW_NO_SANITIZE("unsigned-integer-overflow") {
  return (int32_t)((n >> 1) ^ (~(n & 1) + 1));
}

/// Zig-zag decodes a `uint64_t`, returning it as an `int64_t`.
static inline int64_t pw_varint_ZigZagDecode64(uint64_t n)
    PW_NO_SANITIZE("unsigned-integer-overflow") {
  return (int64_t)((n >> 1) ^ (~(n & 1) + 1));
}

/// Decodes an LEB128-encoded integer to a `uint32_t`.
/// @returns the number of bytes read; 0 if decoding failed
size_t pw_varint_Decode32(const void* input,
                          size_t input_size_bytes,
                          uint32_t* output);

/// Decodes an LEB128-encoded integer to a `uint64_t`.
/// @returns the number of bytes read; 0 if decoding failed
size_t pw_varint_Decode64(const void* input,
                          size_t input_size_bytes,
                          uint64_t* output);

/// Decodes one byte of an LEB128-encoded integer to a `uint32_t`.
/// @returns true if there is more data to decode (top bit is set).
static inline bool pw_varint_DecodeOneByte32(uint8_t byte,
                                             size_t count,
                                             uint32_t* value) {
  *value |= (uint32_t)(byte & 0x7fu) << (count * 7);
  return (byte & 0x80u) != 0u;
}

/// Decodes one byte of an LEB128-encoded integer to a `uint64_t`.
/// @returns true if there is more data to decode (top bit is set).
static inline bool pw_varint_DecodeOneByte64(uint8_t byte,
                                             size_t count,
                                             uint64_t* value) {
  *value |= (uint64_t)(byte & 0x7fu) << (count * 7);
  return (byte & 0x80u) != 0u;
}

/// Macro that returns the encoded size of up to a 64-bit integer. This is
/// inefficient, but is a constant expression if the input is a constant. Use
/// `pw_varint_EncodedSizeBytes` for runtime encoded size calculation.
#define PW_VARINT_ENCODED_SIZE_BYTES(value)        \
  ((unsigned long long)value < (1u << 7)      ? 1u \
   : (unsigned long long)value < (1u << 14)   ? 2u \
   : (unsigned long long)value < (1u << 21)   ? 3u \
   : (unsigned long long)value < (1u << 28)   ? 4u \
   : (unsigned long long)value < (1llu << 35) ? 5u \
   : (unsigned long long)value < (1llu << 42) ? 6u \
   : (unsigned long long)value < (1llu << 49) ? 7u \
   : (unsigned long long)value < (1llu << 56) ? 8u \
   : (unsigned long long)value < (1llu << 63) ? 9u \
                                              : 10u)

/// Returns the size of a `uint64_t` when encoded as a varint (LEB128).
size_t pw_varint_EncodedSizeBytes(uint64_t integer);

/// Describes a custom varint format.
typedef enum {
  PW_VARINT_ZERO_TERMINATED_LEAST_SIGNIFICANT = 0,
  PW_VARINT_ZERO_TERMINATED_MOST_SIGNIFICANT = 1,
  PW_VARINT_ONE_TERMINATED_LEAST_SIGNIFICANT = 2,
  PW_VARINT_ONE_TERMINATED_MOST_SIGNIFICANT = 3,
} pw_varint_Format;

/// Encodes a `uint64_t` using a custom varint format.
size_t pw_varint_EncodeCustom(uint64_t integer,
                              void* output,
                              size_t output_size,
                              pw_varint_Format format);

/// Decodes a `uint64_t` using a custom varint format.
size_t pw_varint_DecodeCustom(const void* input,
                              size_t input_size,
                              uint64_t* output,
                              pw_varint_Format format);

#ifdef __cplusplus

}  // extern "C"

#include <limits>
#include <type_traits>

#include "lib/stdcompat/bit.h"
#include "pw_polyfill/language_feature_macros.h"
#include "pw_span/span.h"

namespace pw {
namespace varint {

/// Maximum size of a varint (LEB128) encoded `uint32_t`.
PW_INLINE_VARIABLE constexpr size_t kMaxVarint32SizeBytes =
    PW_VARINT_MAX_INT32_SIZE_BYTES;

/// Maximum size of a varint (LEB128) encoded `uint64_t`.
PW_INLINE_VARIABLE constexpr size_t kMaxVarint64SizeBytes =
    PW_VARINT_MAX_INT64_SIZE_BYTES;

/// ZigZag encodes a signed integer. This maps small negative numbers to small,
/// unsigned positive numbers, which improves their density for LEB128 encoding.
///
/// ZigZag encoding works by moving the sign bit from the most-significant bit
/// to the least-significant bit. For the signed `k`-bit integer `n`, the
/// formula is:
///
/// @code{.cpp}
///   (n << 1) ^ (n >> (k - 1))
/// @endcode
///
/// See the following for a description of ZigZag encoding:
///   https://developers.google.com/protocol-buffers/docs/encoding#types
template <typename T>
constexpr std::make_unsigned_t<T> ZigZagEncode(T n) {
  static_assert(std::is_signed<T>(), "Zig-zag encoding is for signed integers");
  using U = std::make_unsigned_t<T>;
  return static_cast<U>(static_cast<U>(n) << 1) ^
         static_cast<U>(n >> (sizeof(T) * 8 - 1));
}

/// ZigZag decodes a signed integer.
///
/// The calculation is done modulo `std::numeric_limits<T>::max()+1`, so the
/// unsigned integer overflows are intentional.
template <typename T>
constexpr std::make_signed_t<T> ZigZagDecode(T n)
    PW_NO_SANITIZE("unsigned-integer-overflow") {
  static_assert(std::is_unsigned<T>(),
                "Zig-zag decoding is for unsigned integers");
  return static_cast<std::make_signed_t<T>>((n >> 1) ^ (~(n & 1) + 1));
}

/// @brief Computes the size of an integer when encoded as a varint.
///
/// @param integer The integer whose encoded size is to be computed. `integer`
/// can be signed or unsigned.
///
/// @returns The size of `integer` when encoded as a varint.
template <typename T,
          typename = std::enable_if_t<std::is_integral<T>::value ||
                                      std::is_convertible<T, uint64_t>::value>>
constexpr size_t EncodedSize(T integer) {
  if (integer == 0) {
    return 1;
  }
  return static_cast<size_t>(
      (64 - cpp20::countl_zero(static_cast<uint64_t>(integer)) + 6) / 7);
}

/// Encodes a `uint64_t` with Little-Endian Base 128 (LEB128) encoding.
/// @returns the number of bytes written; 0 if the buffer is too small
inline size_t EncodeLittleEndianBase128(uint64_t integer,
                                        const span<std::byte>& output) {
  return pw_varint_Encode64(integer, output.data(), output.size());
}

/// Encodes the provided integer using a variable-length encoding and returns
/// the number of bytes written.
///
/// The encoding is the same as used in protocol buffers. Signed integers are
/// ZigZag encoded to remove leading 1s from small negative numbers, then the
/// resulting number is encoded as Little Endian Base 128 (LEB128). Unsigned
/// integers are encoded directly as LEB128.
///
/// Returns the number of bytes written or 0 if the result didn't fit in the
/// encoding buffer.
template <typename T>
size_t Encode(T integer, const span<std::byte>& output) {
  if (std::is_signed<T>()) {
    using Signed =
        std::conditional_t<std::is_signed<T>::value, T, std::make_signed_t<T>>;
    return EncodeLittleEndianBase128(ZigZagEncode(static_cast<Signed>(integer)),
                                     output);
  } else {
    using Unsigned = std::
        conditional_t<std::is_signed<T>::value, std::make_unsigned_t<T>, T>;
    return EncodeLittleEndianBase128(static_cast<Unsigned>(integer), output);
  }
}

/// Decodes a varint-encoded value. If reading into a signed integer, the value
/// is ZigZag decoded.
///
/// Returns the number of bytes read from the input if successful. Returns zero
/// if the result does not fit in a `int64_t`/ `uint64_t` or if the input is
/// exhausted before the number terminates. Reads a maximum of 10 bytes.
///
/// The following example decodes multiple varints from a buffer:
///
/// @code{.cpp}
///
///   while (!data.empty()) {
///     int64_t value;
///     size_t bytes = Decode(data, &value);
///
///     if (bytes == 0u) {
///       return Status::DataLoss();
///     }
///     results.push_back(value);
///     data = data.subspan(bytes)
///   }
///
/// @endcode
inline size_t Decode(const span<const std::byte>& input, int64_t* output) {
  uint64_t value = 0;
  size_t bytes_read = pw_varint_Decode64(input.data(), input.size(), &value);
  *output = pw_varint_ZigZagDecode64(value);
  return bytes_read;
}

/// @overload
inline size_t Decode(const span<const std::byte>& input, uint64_t* value) {
  return pw_varint_Decode64(input.data(), input.size(), value);
}

/// Describes a custom varint format.
enum class Format {
  kZeroTerminatedLeastSignificant = PW_VARINT_ZERO_TERMINATED_LEAST_SIGNIFICANT,
  kZeroTerminatedMostSignificant = PW_VARINT_ZERO_TERMINATED_MOST_SIGNIFICANT,
  kOneTerminatedLeastSignificant = PW_VARINT_ONE_TERMINATED_LEAST_SIGNIFICANT,
  kOneTerminatedMostSignificant = PW_VARINT_ONE_TERMINATED_MOST_SIGNIFICANT,
};

/// Encodes a varint in a custom format.
inline size_t Encode(uint64_t value, span<std::byte> output, Format format) {
  return pw_varint_EncodeCustom(value,
                                output.data(),
                                output.size(),
                                static_cast<pw_varint_Format>(format));
}

/// Decodes a varint from a custom format.
inline size_t Decode(span<const std::byte> input,
                     uint64_t* value,
                     Format format) {
  return pw_varint_DecodeCustom(
      input.data(), input.size(), value, static_cast<pw_varint_Format>(format));
}

/// @brief Returns the maximum (max) integer value that can be encoded as a
/// varint into the specified number of bytes.
///
/// The following table lists the max value for each byte size:
///
/// | Bytes | Max value                 |
/// | ----- | ------------------------- |
/// | 1     |                       127 |
/// | 2     |                    16,383 |
/// | 3     |                 2,097,151 |
/// | 4     |               268,435,455 |
/// | 5     |            34,359,738,367 |
/// | 6     |         4,398,046,511,103 |
/// | 7     |       562,949,953,421,311 |
/// | 8     |    72,057,594,037,927,935 |
/// | 9     | 9,223,372,036,854,775,807 |
/// | 10    |        (uint64 max value) |
///
/// @param bytes The size of the varint, in bytes. 5 bytes are needed for the
/// max `uint32` value. 10 bytes are needed for the max `uint64` value.
///
/// @return The max integer value for a varint of size `bytes`.
constexpr uint64_t MaxValueInBytes(size_t bytes) {
  return bytes >= kMaxVarint64SizeBytes ? std::numeric_limits<uint64_t>::max()
                                        : (uint64_t(1) << (7 * bytes)) - 1;
}

}  // namespace varint
}  // namespace pw

#endif  // __cplusplus
