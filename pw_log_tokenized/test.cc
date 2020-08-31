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

#define PW_LOG_MODULE_NAME "This is the log module name!"

#include <cstring>
#include <string_view>

#include "gtest/gtest.h"
#include "pw_log_tokenized/log_tokenized.h"

namespace pw::log_tokenized {
namespace {

Metadata metadata(0);
size_t encoded_data_size = 0;

extern "C" void pw_tokenizer_HandleEncodedMessageWithPayload(
    pw_tokenizer_Payload payload, const uint8_t[], size_t size) {
  metadata = payload;
  encoded_data_size = size;
}

constexpr uintptr_t kModuleToken =
    PW_TOKENIZER_STRING_TOKEN(PW_LOG_MODULE_NAME) &
    ((1u << _PW_LOG_TOKENIZED_MODULE_BITS) - 1);

constexpr Metadata test1 = Metadata::Set<0, 0, 0>();
static_assert(test1.level() == 0);
static_assert(test1.module() == 0);
static_assert(test1.flags() == 0);

constexpr Metadata test2 = Metadata::Set<3, 2, 1>();
static_assert(test2.level() == 3);
static_assert(test2.module() == 2);
static_assert(test2.flags() == 1);

constexpr Metadata test3 = Metadata::Set<63, 65535, 1023>();
static_assert(test3.level() == 63);
static_assert(test3.module() == 65535);
static_assert(test3.flags() == 1023);

TEST(LogTokenized, LogMetadata_Zero) {
  PW_LOG_TOKENIZED_TO_GLOBAL_HANDLER_WITH_PAYLOAD(0, 0, "hello");
  EXPECT_EQ(metadata.level(), 0u);
  EXPECT_EQ(metadata.flags(), 0u);
  EXPECT_EQ(metadata.module(), kModuleToken);
  EXPECT_EQ(encoded_data_size, 4u /* token */);
}

TEST(LogTokenized, LogMetadata_DifferentValues) {
  PW_LOG_TOKENIZED_TO_GLOBAL_HANDLER_WITH_PAYLOAD(55, 36, "hello%s", "?");
  EXPECT_EQ(metadata.level(), 55u);
  EXPECT_EQ(metadata.flags(), 36u);
  EXPECT_EQ(metadata.module(), kModuleToken);
  EXPECT_EQ(encoded_data_size, 4u /* token */ + 2u /* encoded string */);
}

TEST(LogTokenized, LogMetadata_MaxValues) {
  PW_LOG_TOKENIZED_TO_GLOBAL_HANDLER_WITH_PAYLOAD(63, 1023, "hello %d", 1);
  EXPECT_EQ(metadata.level(), 63u);
  EXPECT_EQ(metadata.flags(), 1023u);
  EXPECT_EQ(metadata.module(), kModuleToken);
  EXPECT_EQ(encoded_data_size, 4u /* token */ + 1u /* encoded integer */);
}

}  // namespace
}  // namespace pw::log_tokenized
