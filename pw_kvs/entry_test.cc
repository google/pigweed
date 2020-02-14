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

#include "pw_kvs_private/entry.h"

#include "gtest/gtest.h"
#include "pw_kvs/flash_memory.h"

namespace pw::kvs {
namespace {

TEST(Entry, Alignment) {
  for (size_t alignment_bytes = 1; alignment_bytes <= 4096; ++alignment_bytes) {
    ASSERT_EQ(AlignUp(alignment_bytes, Entry::kMinAlignmentBytes),
              Entry::Valid(9, nullptr, "k", {}, alignment_bytes, 0)
                  .alignment_bytes());
    ASSERT_EQ(AlignUp(alignment_bytes, Entry::kMinAlignmentBytes),
              Entry::Tombstone(9, nullptr, "k", alignment_bytes, 0)
                  .alignment_bytes());
  }
}

TEST(Entry, ValidEntry) {
  Entry entry = Entry::Valid(9, nullptr, "k", as_bytes(span("123")), 1, 9876);

  EXPECT_FALSE(entry.deleted());
  EXPECT_EQ(entry.magic(), 9u);
  EXPECT_EQ(entry.checksum(), 0u);  // kNoChecksum
  EXPECT_EQ(entry.key_length(), 1u);
  EXPECT_EQ(entry.value_length(), sizeof("123"));
  EXPECT_EQ(entry.alignment_bytes(), 16u);
  EXPECT_EQ(entry.key_version(), 9876u);
}

TEST(Entry, Tombstone) {
  Entry entry = Entry::Tombstone(99, nullptr, "key", 1, 123);

  EXPECT_TRUE(entry.deleted());
  EXPECT_EQ(entry.magic(), 99u);
  EXPECT_EQ(entry.checksum(), 0u);  // kNoChecksum
  EXPECT_EQ(entry.key_length(), 3u);
  EXPECT_EQ(entry.value_length(), 0u);
  EXPECT_EQ(entry.alignment_bytes(), 16u);
  EXPECT_EQ(entry.key_version(), 123u);
}

}  // namespace
}  // namespace pw::kvs
