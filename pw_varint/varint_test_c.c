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

// These tests call the pw_varint module API from C. The return values are
// checked in the main C++ tests.

#include <stddef.h>

#include "pw_varint/varint.h"

size_t pw_varint_CallEncode32(uint32_t value,
                              void* out_encoded,
                              size_t out_encoded_size) {
  return pw_varint_Encode32(value, out_encoded, out_encoded_size);
}

size_t pw_varint_CallEncode64(uint64_t value,
                              void* out_encoded,
                              size_t out_encoded_size) {
  return pw_varint_Encode64(value, out_encoded, out_encoded_size);
}

size_t pw_varint_CallZigZagAndVarintEncode64(int64_t value,
                                             void* out_encoded,
                                             size_t out_encoded_size) {
  return pw_varint_Encode64(
      pw_varint_ZigZagEncode64(value), out_encoded, out_encoded_size);
}

size_t pw_varint_CallDecode32(const void* encoded,
                              size_t encoded_size,
                              uint32_t* out_value) {
  return pw_varint_Decode32(encoded, encoded_size, out_value);
}

size_t pw_varint_CallDecode64(const void* encoded,
                              size_t encoded_size,
                              uint64_t* out_value) {
  return pw_varint_Decode64(encoded, encoded_size, out_value);
}

size_t pw_varint_CallZigZagAndVarintDecode64(const void* encoded,
                                             size_t encoded_size,
                                             int64_t* out_value) {
  uint64_t value = 0;
  const size_t bytes_read = pw_varint_Decode64(encoded, encoded_size, &value);
  *out_value = pw_varint_ZigZagDecode64(value);
  return bytes_read;
}
