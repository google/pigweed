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

size_t pw_varint_CallEncode32(uint32_t integer,
                              void* output,
                              size_t output_size_bytes) {
  return pw_varint_Encode32(integer, output, output_size_bytes);
}

size_t pw_varint_CallEncode64(uint64_t integer,
                              void* output,
                              size_t output_size_bytes) {
  return pw_varint_Encode64(integer, output, output_size_bytes);
}

size_t pw_varint_CallZigZagAndVarintEncode64(int64_t integer,
                                             void* output,
                                             size_t output_size_bytes) {
  return pw_varint_Encode64(
      pw_varint_ZigZagEncode64(integer), output, output_size_bytes);
}

size_t pw_varint_CallDecode32(void* input,
                              size_t input_size,
                              uint32_t* output) {
  return pw_varint_Decode32(input, input_size, output);
}

size_t pw_varint_CallDecode64(void* input,
                              size_t input_size,
                              uint64_t* output) {
  return pw_varint_Decode64(input, input_size, output);
}

size_t pw_varint_CallZigZagAndVarintDecode64(void* input,
                                             size_t input_size,
                                             int64_t* output) {
  uint64_t value = 0;
  const size_t bytes_read = pw_varint_Decode64(input, input_size, &value);
  *output = pw_varint_ZigZagDecode64(value);
  return bytes_read;
}
