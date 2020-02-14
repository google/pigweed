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
#include "pw_kvs/alignment.h"
#include "pw_kvs/flash_memory.h"
#include "pw_kvs/in_memory_fake_flash.h"

namespace pw::kvs {
namespace {

// TODO(hepler): expand these tests

InMemoryFakeFlash<128, 4> test_flash(16);
FlashPartition test_partition(&test_flash, 0, test_flash.sector_count());

TEST(Entry, Size_RoundsUpToAlignment) {
  for (size_t alignment_bytes = 1; alignment_bytes <= 4096; ++alignment_bytes) {
    const size_t align = AlignUp(alignment_bytes, Entry::kMinAlignmentBytes);

    for (size_t value : {size_t(0), align - 1, align, align + 1, 2 * align}) {
      Entry entry = Entry::Valid(test_partition,
                                 0,
                                 9,
                                 nullptr,
                                 "k",
                                 {nullptr, value},
                                 alignment_bytes,
                                 0);
      ASSERT_EQ(AlignUp(sizeof(EntryHeader) + 1 /* key */ + value, align),
                entry.size());
    }

    Entry entry = Entry::Tombstone(
        test_partition, 0, 9, nullptr, "k", alignment_bytes, 0);
    ASSERT_EQ(AlignUp(sizeof(EntryHeader) + 1 /* key */, align), entry.size());
  }
}

TEST(Entry, Construct_ValidEntry) {
  auto entry = Entry::Valid(
      test_partition, 1, 9, nullptr, "k", as_bytes(span("123")), 1, 9876);

  EXPECT_FALSE(entry.deleted());
  EXPECT_EQ(entry.magic(), 9u);
  EXPECT_EQ(entry.value_size(), sizeof("123"));
  EXPECT_EQ(entry.key_version(), 9876u);
}

TEST(Entry, Construct_Tombstone) {
  auto entry = Entry::Tombstone(test_partition, 1, 99, nullptr, "key", 1, 123);

  EXPECT_TRUE(entry.deleted());
  EXPECT_EQ(entry.magic(), 99u);
  EXPECT_EQ(entry.value_size(), 0u);
  EXPECT_EQ(entry.key_version(), 123u);
}

}  // namespace
}  // namespace pw::kvs
