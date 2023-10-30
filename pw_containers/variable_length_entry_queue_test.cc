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

#include "pw_containers/variable_length_entry_queue.h"

#include <cstring>
#include <string_view>
#include <variant>

#include "gtest/gtest.h"
#include "pw_containers_private/variable_length_entry_queue_test_oracle.h"

namespace {

struct PushOverwrite {
  std::string_view data;
};
struct Push {
  std::string_view data;
};
struct Pop {};
struct Clear {};
struct SizeEquals {
  size_t expected;
};

using TestStep = std::variant<PushOverwrite, Push, Pop, Clear, SizeEquals>;

// Copies an entry, which might be wrapped, to a single std::vector.
std::vector<std::byte> ReadEntry(
    const pw_VariableLengthEntryQueue_Iterator& it) {
  auto entry = pw_VariableLengthEntryQueue_GetEntry(&it);
  std::vector<std::byte> value(entry.size_1 + entry.size_2);
  EXPECT_EQ(value.size(),
            pw_VariableLengthEntryQueue_Entry_Copy(
                &entry, value.data(), entry.size_1 + entry.size_2));
  return value;
}

#define ASSERT_CONTENTS_EQ(oracle, queue)                                      \
  auto oracle_it = oracle.begin();                                             \
  auto queue_it = pw_VariableLengthEntryQueue_Begin(queue);                    \
  const auto queue_end = pw_VariableLengthEntryQueue_End(queue);               \
  uint32_t entries_compared = 0;                                               \
  while (oracle_it != oracle.end() &&                                          \
         !pw_VariableLengthEntryQueue_Iterator_Equal(&queue_it, &queue_end)) { \
    ASSERT_EQ(*oracle_it++, ReadEntry(queue_it));                              \
    pw_VariableLengthEntryQueue_Iterator_Advance(&queue_it);                   \
    entries_compared += 1;                                                     \
  }                                                                            \
  ASSERT_EQ(entries_compared, oracle.size())

// Declares a test that performs a series of operations on a
// VariableLengthEntryQueue and the "oracle" class, and checks that they match
// after every step.
#define DATA_DRIVEN_TEST(program, max_entry_size)                              \
  TEST(VariableLengthEntryQueue,                                               \
       DataDrivenTest_##program##_MaxSizeBytes##max_entry_size) {              \
    pw::containers::VariableLengthEntryQueueTestOracle oracle(max_entry_size); \
    PW_VARIABLE_LENGTH_ENTRY_QUEUE_DECLARE(c_queue, max_entry_size);           \
                                                                               \
    for (const TestStep& step : program) {                                     \
      /* Take the action */                                                    \
      if (auto ow = std::get_if<PushOverwrite>(&step); ow != nullptr) {        \
        pw_VariableLengthEntryQueue_PushOverwrite(                             \
            c_queue, ow->data.data(), static_cast<uint32_t>(ow->data.size())); \
        oracle.push_overwrite(pw::as_bytes(pw::span(ow->data)));               \
      } else if (auto push = std::get_if<Push>(&step); push != nullptr) {      \
        pw_VariableLengthEntryQueue_Push(                                      \
            c_queue,                                                           \
            push->data.data(),                                                 \
            static_cast<uint32_t>(push->data.size()));                         \
        oracle.push(pw::as_bytes(pw::span(push->data)));                       \
      } else if (std::holds_alternative<Pop>(step)) {                          \
        pw_VariableLengthEntryQueue_Pop(c_queue);                              \
        oracle.pop();                                                          \
      } else if (auto size = std::get_if<SizeEquals>(&step);                   \
                 size != nullptr) {                                            \
        size_t actual = pw_VariableLengthEntryQueue_Size(c_queue);             \
        ASSERT_EQ(oracle.size(), actual);                                      \
        ASSERT_EQ(size->expected, actual);                                     \
      } else if (std::holds_alternative<Clear>(step)) {                        \
        pw_VariableLengthEntryQueue_Clear(c_queue);                            \
        oracle.clear();                                                        \
      } else {                                                                 \
        FAIL() << "Unhandled case";                                            \
      }                                                                        \
      /* Check size and other functions */                                     \
      ASSERT_EQ(pw_VariableLengthEntryQueue_Size(c_queue), oracle.size());     \
      ASSERT_EQ(pw_VariableLengthEntryQueue_SizeBytes(c_queue),                \
                oracle.size_bytes());                                          \
      ASSERT_EQ(pw_VariableLengthEntryQueue_MaxSizeBytes(c_queue),             \
                oracle.max_size_bytes());                                      \
      ASSERT_CONTENTS_EQ(oracle, c_queue);                                     \
    }                                                                          \
  }                                                                            \
  static_assert(true, "use a semicolon")

constexpr TestStep kPop[] = {
    SizeEquals{0},
    PushOverwrite{""},
    SizeEquals{1},
    Pop{},
    SizeEquals{0},
};

DATA_DRIVEN_TEST(kPop, 0);  // Only holds one empty entry.
DATA_DRIVEN_TEST(kPop, 1);
DATA_DRIVEN_TEST(kPop, 6);

constexpr TestStep kOverwriteLargeEntriesWithSmall[] = {
    PushOverwrite{"12345"},
    PushOverwrite{"abcde"},
    PushOverwrite{""},
    PushOverwrite{""},
    PushOverwrite{""},
    PushOverwrite{""},
    PushOverwrite{""},
    PushOverwrite{""},
    SizeEquals{6},
    Pop{},
    Pop{},
    Pop{},
    Pop{},
    Pop{},
    Pop{},
    SizeEquals{0},
};
DATA_DRIVEN_TEST(kOverwriteLargeEntriesWithSmall, 5);
DATA_DRIVEN_TEST(kOverwriteLargeEntriesWithSmall, 6);
DATA_DRIVEN_TEST(kOverwriteLargeEntriesWithSmall, 7);

constexpr TestStep kOverwriteVaryingSizes012[] = {
    PushOverwrite{""},   PushOverwrite{""},   PushOverwrite{""},
    PushOverwrite{""},   PushOverwrite{""},   PushOverwrite{"1"},
    PushOverwrite{"2"},  PushOverwrite{""},   PushOverwrite{"3"},
    PushOverwrite{"4"},  PushOverwrite{""},   PushOverwrite{"5"},
    PushOverwrite{"6"},  PushOverwrite{"ab"}, PushOverwrite{"cd"},
    PushOverwrite{""},   PushOverwrite{"ef"}, PushOverwrite{"gh"},
    PushOverwrite{"ij"},
};
DATA_DRIVEN_TEST(kOverwriteVaryingSizes012, 2);
DATA_DRIVEN_TEST(kOverwriteVaryingSizes012, 3);

constexpr TestStep kOverwriteVaryingSizesUpTo4[] = {
    PushOverwrite{""},
    PushOverwrite{""},
    PushOverwrite{""},
    PushOverwrite{"1"},
    PushOverwrite{"2"},
    PushOverwrite{"3"},
    PushOverwrite{"ab"},
    PushOverwrite{"cd"},
    PushOverwrite{"ef"},
    PushOverwrite{"123"},
    PushOverwrite{"456"},
    PushOverwrite{"789"},
    PushOverwrite{"abcd"},
    PushOverwrite{"efgh"},
    PushOverwrite{"ijkl"},
    Pop{},
    SizeEquals{0},
};
DATA_DRIVEN_TEST(kOverwriteVaryingSizesUpTo4, 4);
DATA_DRIVEN_TEST(kOverwriteVaryingSizesUpTo4, 5);
DATA_DRIVEN_TEST(kOverwriteVaryingSizesUpTo4, 6);

constexpr char kBigEntryBytes[196]{};

constexpr TestStep kTwoBytePrefix[] = {
    PushOverwrite{std::string_view(kBigEntryBytes, 128)},
    PushOverwrite{std::string_view(kBigEntryBytes, 128)},
    PushOverwrite{std::string_view(kBigEntryBytes, 127)},
    PushOverwrite{std::string_view(kBigEntryBytes, 128)},
    PushOverwrite{std::string_view(kBigEntryBytes, 127)},
    SizeEquals{1},
    Pop{},
    SizeEquals{0},
};
DATA_DRIVEN_TEST(kTwoBytePrefix, 128);
DATA_DRIVEN_TEST(kTwoBytePrefix, 129);

constexpr TestStep kClear[] = {
    Push{"abcdefg"},
    PushOverwrite{""},
    PushOverwrite{""},
    PushOverwrite{"a"},
    PushOverwrite{"b"},
    Clear{},
    SizeEquals{0},
    Clear{},
};
DATA_DRIVEN_TEST(kClear, 7);
DATA_DRIVEN_TEST(kClear, 100);

TEST(VariableLengthEntryQueue, DeclareMacro) {
  PW_VARIABLE_LENGTH_ENTRY_QUEUE_DECLARE(queue, 123);

  constexpr size_t kArraySizeBytes =
      123 + 1 /*prefix*/ + 1 /* end */ + 3 /* round up */ +
      PW_VARIABLE_LENGTH_ENTRY_QUEUE_HEADER_SIZE_UINT32 * 4;
  static_assert(sizeof(queue) == kArraySizeBytes);
  EXPECT_EQ(pw_VariableLengthEntryQueue_RawStorageSizeBytes(queue),
            kArraySizeBytes - 3 /* padding isn't included */);

  EXPECT_EQ(pw_VariableLengthEntryQueue_MaxSizeBytes(queue), 123u);
  EXPECT_EQ(pw_VariableLengthEntryQueue_SizeBytes(queue), 0u);
  EXPECT_TRUE(pw_VariableLengthEntryQueue_Empty(queue));
}

TEST(VariableLengthEntryQueue, InitializeExistingBuffer) {
  constexpr size_t kArraySize =
      10 + PW_VARIABLE_LENGTH_ENTRY_QUEUE_HEADER_SIZE_UINT32;
  uint32_t queue[kArraySize];
  pw_VariableLengthEntryQueue_Init(queue, kArraySize);

  EXPECT_EQ(pw_VariableLengthEntryQueue_RawStorageSizeBytes(queue),
            sizeof(queue));
  EXPECT_EQ(pw_VariableLengthEntryQueue_MaxSizeBytes(queue),
            sizeof(uint32_t) * 10u - 1 /*prefix*/ - 1 /*end*/);
  EXPECT_EQ(pw_VariableLengthEntryQueue_SizeBytes(queue), 0u);
  EXPECT_EQ(pw_VariableLengthEntryQueue_Size(queue), 0u);
  EXPECT_TRUE(pw_VariableLengthEntryQueue_Empty(queue));
}

TEST(VariableLengthEntryQueue, MaxSizeElement) {
  // Test max size elements for a few sizes. Commented out statements fail an
  // assert because the elements are too large.
  PW_VARIABLE_LENGTH_ENTRY_QUEUE_DECLARE(q16, 126);
  PW_VARIABLE_LENGTH_ENTRY_QUEUE_DECLARE(q17, 127);
  PW_VARIABLE_LENGTH_ENTRY_QUEUE_DECLARE(q18, 128);
  PW_VARIABLE_LENGTH_ENTRY_QUEUE_DECLARE(q19, 129);

  pw_VariableLengthEntryQueue_PushOverwrite(q16, kBigEntryBytes, 126);
  pw_VariableLengthEntryQueue_PushOverwrite(q17, kBigEntryBytes, 126);
  pw_VariableLengthEntryQueue_PushOverwrite(q18, kBigEntryBytes, 126);
  pw_VariableLengthEntryQueue_PushOverwrite(q19, kBigEntryBytes, 126);

  // pw_VariableLengthEntryQueue_PushOverwrite(q16, kBigEntryBytes, 127);
  pw_VariableLengthEntryQueue_PushOverwrite(q17, kBigEntryBytes, 127);
  pw_VariableLengthEntryQueue_PushOverwrite(q18, kBigEntryBytes, 127);
  pw_VariableLengthEntryQueue_PushOverwrite(q19, kBigEntryBytes, 127);

  // pw_VariableLengthEntryQueue_PushOverwrite(q16, kBigEntryBytes, 128);
  // pw_VariableLengthEntryQueue_PushOverwrite(q17, kBigEntryBytes, 128);
  pw_VariableLengthEntryQueue_PushOverwrite(q18, kBigEntryBytes, 128);
  pw_VariableLengthEntryQueue_PushOverwrite(q19, kBigEntryBytes, 128);

  // pw_VariableLengthEntryQueue_PushOverwrite(q16, kBigEntryBytes, 129);
  // pw_VariableLengthEntryQueue_PushOverwrite(q17, kBigEntryBytes, 129);
  // pw_VariableLengthEntryQueue_PushOverwrite(q18, kBigEntryBytes, 129);
  pw_VariableLengthEntryQueue_PushOverwrite(q19, kBigEntryBytes, 129);

  EXPECT_EQ(pw_VariableLengthEntryQueue_Size(q16), 1u);
  EXPECT_EQ(pw_VariableLengthEntryQueue_Size(q17), 1u);
  EXPECT_EQ(pw_VariableLengthEntryQueue_Size(q18), 1u);
  EXPECT_EQ(pw_VariableLengthEntryQueue_Size(q19), 1u);
}

}  // namespace
