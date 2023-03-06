// Copyright 2022 The Pigweed Authors
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

#include "pw_tokenizer/encode_args.h"

#include "gtest/gtest.h"

namespace pw {
namespace tokenizer {

static_assert(MinEncodingBufferSizeBytes<>() == 4);
static_assert(MinEncodingBufferSizeBytes<bool>() == 4 + 2);
static_assert(MinEncodingBufferSizeBytes<char>() == 4 + 2);
static_assert(MinEncodingBufferSizeBytes<short>() == 4 + 3);
static_assert(MinEncodingBufferSizeBytes<int>() == 4 + 5);
static_assert(MinEncodingBufferSizeBytes<long long>() == 4 + 10);
static_assert(MinEncodingBufferSizeBytes<float>() == 4 + 4);
static_assert(MinEncodingBufferSizeBytes<double>() == 4 + 4);
static_assert(MinEncodingBufferSizeBytes<const char*>() == 4 + 1);
static_assert(MinEncodingBufferSizeBytes<void*>() == 4 + 5 ||
              MinEncodingBufferSizeBytes<void*>() == 4 + 10);

static_assert(MinEncodingBufferSizeBytes<int, double>() == 4 + 5 + 4);
static_assert(MinEncodingBufferSizeBytes<int, int, const char*>() ==
              4 + 5 + 5 + 1);
static_assert(
    MinEncodingBufferSizeBytes<const char*, long long, int, short>() ==
    4 + 1 + 10 + 5 + 3);

}  // namespace tokenizer
}  // namespace pw
