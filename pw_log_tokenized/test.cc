// Copyright 2021 The Pigweed Authors
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

// Create a fake version of the tokenization macro.
#undef PW_LOG_TOKENIZED_ENCODE_MESSAGE
#define PW_LOG_TOKENIZED_ENCODE_MESSAGE(payload, message, ...) \
  CaptureTokenizerArgs(payload,                                \
                       PW_MACRO_ARG_COUNT(__VA_ARGS__),        \
                       message PW_COMMA_ARGS(__VA_ARGS__))

#include <cstring>
#include <string_view>
#include <tuple>

#include "gtest/gtest.h"
#include "pw_log_tokenized/log_tokenized.h"
#include "pw_preprocessor/arguments.h"
#include "pw_preprocessor/compiler.h"

namespace pw::log_tokenized {
namespace {

struct {
  Metadata metadata = Metadata(0);
  const char* format_string = "";
  size_t arg_count = 0;
} last_log{};

void CaptureTokenizerArgs(pw_tokenizer_Payload payload,
                          size_t arg_count,
                          const char* message,
                          ...) PW_PRINTF_FORMAT(3, 4);

void CaptureTokenizerArgs(pw_tokenizer_Payload payload,
                          size_t arg_count,
                          const char* message,
                          ...) {
  last_log.metadata = payload;
  last_log.format_string = message;
  last_log.arg_count = arg_count;
}

constexpr uintptr_t kModuleToken =
    PW_TOKENIZER_STRING_TOKEN(PW_LOG_MODULE_NAME) &
    ((1u << PW_LOG_TOKENIZED_MODULE_BITS) - 1);

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
  EXPECT_EQ(last_log.metadata.level(), 0u);
  EXPECT_EQ(last_log.metadata.flags(), 0u);
  EXPECT_EQ(last_log.metadata.module(), kModuleToken);
  EXPECT_EQ(last_log.arg_count, 0u);
}

TEST(LogTokenized, LogMetadata_DifferentValues) {
  PW_LOG_TOKENIZED_TO_GLOBAL_HANDLER_WITH_PAYLOAD(55, 36, "hello%s", "?");
  EXPECT_EQ(last_log.metadata.level(), 55u);
  EXPECT_EQ(last_log.metadata.flags(), 36u);
  EXPECT_EQ(last_log.metadata.module(), kModuleToken);
  EXPECT_EQ(last_log.arg_count, 1u);
}

TEST(LogTokenized, LogMetadata_MaxValues) {
  PW_LOG_TOKENIZED_TO_GLOBAL_HANDLER_WITH_PAYLOAD(63, 1023, "hello %d", 1);
  EXPECT_EQ(last_log.metadata.level(), 63u);
  EXPECT_EQ(last_log.metadata.flags(), 1023u);
  EXPECT_EQ(last_log.metadata.module(), kModuleToken);
  EXPECT_EQ(last_log.arg_count, 1u);
}

}  // namespace
}  // namespace pw::log_tokenized
