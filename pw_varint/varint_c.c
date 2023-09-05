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

#include "pw_varint/varint.h"

#define VARINT_ENCODE_FUNCTION_BODY(output)                                \
  size_t written = 0;                                                      \
  uint8_t* buffer = (uint8_t*)output;                                      \
                                                                           \
  do {                                                                     \
    /* Grab 7 bits; the eighth bit is set to indicate more data coming. */ \
    buffer[written++] = (integer & 0x7F) | 0x80;                           \
    integer >>= 7;                                                         \
  } while (integer != 0u);                                                 \
                                                                           \
  buffer[written - 1] &= 0x7f;                                             \
  return written

size_t pw_varint_Encode32(uint32_t integer, void* buffer_of_at_least_5_bytes) {
  VARINT_ENCODE_FUNCTION_BODY(buffer_of_at_least_5_bytes);
}

size_t pw_varint_Encode64(uint64_t integer, void* buffer_of_at_least_10_bytes) {
  VARINT_ENCODE_FUNCTION_BODY(buffer_of_at_least_10_bytes);
}

#define VARINT_DECODE_FUNCTION_BODY(bits)                                    \
  uint##bits##_t decoded_value = 0;                                          \
  uint_fast8_t count = 0;                                                    \
  const uint8_t* buffer = (const uint8_t*)(input);                           \
                                                                             \
  /* Only read up to the largest possible encoded size. */                   \
  const size_t max_count =                                                   \
      input_size_bytes < PW_VARINT_MAX_INT##bits##_SIZE_BYTES                \
          ? input_size_bytes                                                 \
          : PW_VARINT_MAX_INT##bits##_SIZE_BYTES;                            \
                                                                             \
  do {                                                                       \
    if (count >= max_count) {                                                \
      return 0;                                                              \
    }                                                                        \
                                                                             \
    /* Add the bottom seven bits of the next byte to the result. */          \
    decoded_value |= (uint##bits##_t)(buffer[count] & 0x7fu) << (7 * count); \
                                                                             \
    /* Stop decoding if the top bit is not set. */                           \
  } while ((buffer[count++] & 0x80u) != 0);                                  \
                                                                             \
  *output = decoded_value;                                                   \
  return count

size_t pw_varint_Decode32(const void* input,
                          size_t input_size_bytes,
                          uint32_t* output) {
  VARINT_DECODE_FUNCTION_BODY(32);
}

size_t pw_varint_Decode64(const void* input,
                          size_t input_size_bytes,
                          uint64_t* output) {
  VARINT_DECODE_FUNCTION_BODY(64);
}
