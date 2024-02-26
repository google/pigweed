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

#include "pw_containers_private/variable_length_entry_queue_test_oracle.h"
#include "pw_unit_test/framework.h"

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

// Declares a test that performs a series of operations on the C and C++
// versions of VariableLengthEntryQueue and the "oracle" class, and checks that
// they match after every step.
#define DATA_DRIVEN_TEST(program, max_entry_size)                              \
  TEST(VariableLengthEntryQueue,                                               \
       DataDrivenTest_##program##_MaxSizeBytes##max_entry_size) {              \
    pw::VariableLengthEntryQueue<max_entry_size> cpp_queue;                    \
    PW_VARIABLE_LENGTH_ENTRY_QUEUE_DECLARE(c_queue, max_entry_size);           \
    pw::containers::VariableLengthEntryQueueTestOracle oracle(max_entry_size); \
                                                                               \
    /* Check the queue sizes */                                                \
    static_assert(sizeof(cpp_queue) == sizeof(c_queue));                       \
    ASSERT_EQ(cpp_queue.raw_storage().data(),                                  \
              reinterpret_cast<const std::byte*>(&cpp_queue));                 \
    ASSERT_EQ(cpp_queue.raw_storage().size_bytes(),                            \
              pw_VariableLengthEntryQueue_RawStorageSizeBytes(c_queue));       \
                                                                               \
    for (const TestStep& step : program) {                                     \
      /* Take the action */                                                    \
      if (auto ow = std::get_if<PushOverwrite>(&step); ow != nullptr) {        \
        cpp_queue.push_overwrite(pw::as_bytes(pw::span(ow->data)));            \
        pw_VariableLengthEntryQueue_PushOverwrite(                             \
            c_queue, ow->data.data(), static_cast<uint32_t>(ow->data.size())); \
        oracle.push_overwrite(pw::as_bytes(pw::span(ow->data)));               \
      } else if (auto push = std::get_if<Push>(&step); push != nullptr) {      \
        cpp_queue.push(pw::as_bytes(pw::span(push->data)));                    \
        pw_VariableLengthEntryQueue_Push(                                      \
            c_queue,                                                           \
            push->data.data(),                                                 \
            static_cast<uint32_t>(push->data.size()));                         \
        oracle.push(pw::as_bytes(pw::span(push->data)));                       \
      } else if (std::holds_alternative<Pop>(step)) {                          \
        cpp_queue.pop();                                                       \
        pw_VariableLengthEntryQueue_Pop(c_queue);                              \
        oracle.pop();                                                          \
      } else if (auto size = std::get_if<SizeEquals>(&step);                   \
                 size != nullptr) {                                            \
        const size_t actual = cpp_queue.size();                                \
        ASSERT_EQ(actual, pw_VariableLengthEntryQueue_Size(c_queue));          \
        ASSERT_EQ(oracle.size(), actual);                                      \
        ASSERT_EQ(size->expected, actual);                                     \
      } else if (std::holds_alternative<Clear>(step)) {                        \
        cpp_queue.clear();                                                     \
        pw_VariableLengthEntryQueue_Clear(c_queue);                            \
        oracle.clear();                                                        \
      } else {                                                                 \
        FAIL() << "Unhandled case";                                            \
      }                                                                        \
      /* Check sizes */                                                        \
      ASSERT_EQ(cpp_queue.size(), oracle.size());                              \
      ASSERT_EQ(cpp_queue.size_bytes(), oracle.size_bytes());                  \
      ASSERT_EQ(cpp_queue.max_size_bytes(), oracle.max_size_bytes());          \
                                                                               \
      ASSERT_EQ(pw_VariableLengthEntryQueue_Size(c_queue), oracle.size());     \
      ASSERT_EQ(pw_VariableLengthEntryQueue_SizeBytes(c_queue),                \
                oracle.size_bytes());                                          \
      ASSERT_EQ(pw_VariableLengthEntryQueue_MaxSizeBytes(c_queue),             \
                oracle.max_size_bytes());                                      \
                                                                               \
      /* Compare the contents */                                               \
      auto oracle_it = oracle.begin();                                         \
      auto c_queue_it = pw_VariableLengthEntryQueue_Begin(c_queue);            \
      const auto c_queue_end = pw_VariableLengthEntryQueue_End(c_queue);       \
      uint32_t entries_compared = 0;                                           \
                                                                               \
      for (auto entry : cpp_queue) {                                           \
        entries_compared += 1;                                                 \
                                                                               \
        ASSERT_EQ(*oracle_it, ReadEntry(c_queue_it));                          \
        ASSERT_EQ(*oracle_it,                                                  \
                  std::vector<std::byte>(entry.begin(), entry.end()));         \
                                                                               \
        ASSERT_NE(oracle_it, oracle.end());                                    \
        ASSERT_FALSE(pw_VariableLengthEntryQueue_Iterator_Equal(               \
            &c_queue_it, &c_queue_end));                                       \
                                                                               \
        ++oracle_it;                                                           \
        pw_VariableLengthEntryQueue_Iterator_Advance(&c_queue_it);             \
      }                                                                        \
      ASSERT_EQ(entries_compared, oracle.size());                              \
      ASSERT_TRUE(pw_VariableLengthEntryQueue_Iterator_Equal(&c_queue_it,      \
                                                             &c_queue_end));   \
      ASSERT_EQ(oracle_it, oracle.end());                                      \
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

constexpr const char* kStrings[] = {"Haart", "Sandro", "", "Gelu", "Solmyr"};

TEST(VariableLengthEntryQueueClass, Iterate) {
  pw::BasicVariableLengthEntryQueue<char, 32> queue;

  for (const char* string : kStrings) {
    queue.push(std::string_view(string));
  }

  uint32_t i = 0;
  for (auto entry : queue) {
    char value[8]{};
    entry.copy(value, sizeof(value));
    EXPECT_STREQ(value, kStrings[i++]);
  }
  ASSERT_EQ(i, 5u);
}

TEST(VariableLengthEntryQueueClass, IterateOverwrittenElements) {
  pw::BasicVariableLengthEntryQueue<char, 6> queue;

  for (const char* string : kStrings) {
    queue.push_overwrite(std::string_view(string));
  }

  ASSERT_EQ(queue.size(), 1u);

  for (auto entry : queue) {
    char value[8]{};
    EXPECT_EQ(6u, entry.copy(value, sizeof(value)));
    EXPECT_STREQ(value, "Solmyr");
  }
}

TEST(VariableLengthEntryQueueClass, InitializeExistingBuffer) {
  constexpr size_t kArraySize =
      10 + PW_VARIABLE_LENGTH_ENTRY_QUEUE_HEADER_SIZE_UINT32;
  uint32_t queue_array[kArraySize]{50, 50, 99};
  pw::VariableLengthEntryQueue<>& queue =
      pw::VariableLengthEntryQueue<>::Init(queue_array, kArraySize);

  EXPECT_EQ(queue.raw_storage().data(),
            reinterpret_cast<const std::byte*>(queue_array));
  EXPECT_EQ(queue.raw_storage().size_bytes(), sizeof(queue_array));
  EXPECT_EQ(queue.max_size_bytes(),
            sizeof(uint32_t) * 10u - 1 /*prefix*/ - 1 /*end*/);
  EXPECT_EQ(queue.size_bytes(), 0u);
  EXPECT_EQ(queue.size(), 0u);
  EXPECT_TRUE(queue.empty());
}

TEST(VariableLengthEntryQueueClass, Entry) {
  pw::BasicVariableLengthEntryQueue<char, 5> queue;
  queue.push("12");  // Split the next entry across the end.
  queue.push_overwrite(std::string_view("ABCDE"));

  decltype(queue)::Entry front = queue.front();

  ASSERT_EQ(front.size(), 5u);
  EXPECT_EQ(front[0], 'A');
  EXPECT_EQ(front[1], 'B');
  EXPECT_EQ(front[2], 'C');
  EXPECT_EQ(front[3], 'D');
  EXPECT_EQ(front[4], 'E');

  EXPECT_EQ(front.at(0), 'A');
  EXPECT_EQ(front.at(1), 'B');
  EXPECT_EQ(front.at(2), 'C');
  EXPECT_EQ(front.at(3), 'D');
  EXPECT_EQ(front.at(4), 'E');

  const auto [span_1, span_2] = front.contiguous_data();
  EXPECT_EQ(span_1.size(), 2u);
  EXPECT_EQ(std::memcmp(span_1.data(), "AB", 2u), 0);
  EXPECT_EQ(span_2.size(), 3u);
  EXPECT_EQ(std::memcmp(span_2.data(), "CDE", 3u), 0);

  const char* expected_ptr = "ABCDE";
  for (char c : front) {
    EXPECT_EQ(*expected_ptr, c);
    ++expected_ptr;
  }

  // Check the iterators with std::copy and std::equal.
  char value[6] = {};
  std::copy(front.begin(), front.end(), value);
  EXPECT_STREQ(value, "ABCDE");

  EXPECT_TRUE(std::equal(front.begin(), front.end(), "ABCDE"));
}

}  // namespace
