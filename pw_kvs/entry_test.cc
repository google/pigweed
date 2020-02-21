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

#include "pw_kvs/internal/entry.h"

#include <string_view>

#include "gtest/gtest.h"
#include "pw_kvs/alignment.h"
#include "pw_kvs/crc16_checksum.h"
#include "pw_kvs/flash_memory.h"
#include "pw_kvs/format.h"
#include "pw_kvs/in_memory_fake_flash.h"
#include "pw_kvs_private/byte_utils.h"
#include "pw_span/span.h"

namespace pw::kvs::internal {
namespace {

using std::byte;
using std::string_view;

constexpr EntryFormat kFormat{0xbeef, nullptr};

TEST(Entry, Size_RoundsUpToAlignment) {
  FakeFlashBuffer<64, 2> flash(16);
  FlashPartition partition(&flash, 0, flash.sector_count());

  for (size_t alignment_bytes = 1; alignment_bytes <= 4096; ++alignment_bytes) {
    const size_t align = AlignUp(alignment_bytes, Entry::kMinAlignmentBytes);

    for (size_t value : {size_t(0), align - 1, align, align + 1, 2 * align}) {
      Entry entry = Entry::Valid(
          partition, 0, kFormat, "k", {nullptr, value}, alignment_bytes, 0);
      ASSERT_EQ(AlignUp(sizeof(EntryHeader) + 1 /* key */ + value, align),
                entry.size());
    }

    Entry entry =
        Entry::Tombstone(partition, 0, kFormat, "k", alignment_bytes, 0);
    ASSERT_EQ(AlignUp(sizeof(EntryHeader) + 1 /* key */, align), entry.size());
  }
}

TEST(Entry, Construct_ValidEntry) {
  FakeFlashBuffer<64, 2> flash(16);
  FlashPartition partition(&flash, 0, flash.sector_count());

  auto entry =
      Entry::Valid(partition, 1, kFormat, "k", as_bytes(span("123")), 1, 9876);

  EXPECT_FALSE(entry.deleted());
  EXPECT_EQ(entry.magic(), kFormat.magic);
  EXPECT_EQ(entry.value_size(), sizeof("123"));
  EXPECT_EQ(entry.transaction_id(), 9876u);
}

TEST(Entry, Construct_Tombstone) {
  FakeFlashBuffer<64, 2> flash(16);
  FlashPartition partition(&flash, 0, flash.sector_count());

  auto entry = Entry::Tombstone(partition, 1, kFormat, "key", 1, 123);

  EXPECT_TRUE(entry.deleted());
  EXPECT_EQ(entry.magic(), kFormat.magic);
  EXPECT_EQ(entry.value_size(), 0u);
  EXPECT_EQ(entry.transaction_id(), 123u);
}

constexpr auto kHeader1 = ByteStr(
    "\x0d\xf0\x0d\x60"  // magic
    "\xC5\x65\x00\x00"  // checksum (CRC16)
    "\x01"              // alignment
    "\x05"              // key length
    "\x06\x00"          // value size
    "\x99\x98\x97\x96"  // version
);

constexpr auto kKey1 = ByteStr("key45");
constexpr auto kValue1 = ByteStr("VALUE!");
constexpr auto kPadding1 = ByteStr("\0\0\0\0\0");

constexpr auto kEntry1 = AsBytes(kHeader1, kKey1, kValue1, kPadding1);

class ValidEntryInFlash : public ::testing::Test {
 protected:
  ValidEntryInFlash() : flash_(kEntry1), partition_(&flash_) {
    EXPECT_EQ(Status::OK, Entry::Read(partition_, 0, &entry_));
  }

  FakeFlashBuffer<1024, 4> flash_;
  FlashPartition partition_;
  Entry entry_;
};

TEST_F(ValidEntryInFlash, PassesChecksumVerification) {
  ChecksumCrc16 checksum;
  EXPECT_EQ(Status::OK, entry_.VerifyChecksumInFlash(&checksum));
  EXPECT_EQ(Status::OK, entry_.VerifyChecksum(&checksum, "key45", kValue1));
}

TEST_F(ValidEntryInFlash, HeaderContents) {
  EXPECT_EQ(entry_.magic(), 0x600DF00Du);
  EXPECT_EQ(entry_.key_length(), 5u);
  EXPECT_EQ(entry_.value_size(), 6u);
  EXPECT_EQ(entry_.transaction_id(), 0x96979899u);
  EXPECT_FALSE(entry_.deleted());
}

TEST_F(ValidEntryInFlash, ReadKey) {
  Entry::KeyBuffer key = {};
  auto result = entry_.ReadKey(key);

  ASSERT_EQ(Status::OK, result.status());
  EXPECT_EQ(result.size(), entry_.key_length());
  EXPECT_STREQ(key.data(), "key45");
}

TEST_F(ValidEntryInFlash, ReadValue) {
  char value[32] = {};
  auto result = entry_.ReadValue(as_writable_bytes(span(value)));

  ASSERT_EQ(Status::OK, result.status());
  EXPECT_EQ(result.size(), entry_.value_size());
  EXPECT_STREQ(value, "VALUE!");
}

TEST_F(ValidEntryInFlash, ReadValue_BufferTooSmall) {
  char value[3] = {};
  auto result = entry_.ReadValue(as_writable_bytes(span(value)));

  ASSERT_EQ(Status::RESOURCE_EXHAUSTED, result.status());
  EXPECT_EQ(3u, result.size());
  EXPECT_EQ(value[0], 'V');
  EXPECT_EQ(value[1], 'A');
  EXPECT_EQ(value[2], 'L');
}

TEST_F(ValidEntryInFlash, ReadValue_WithOffset) {
  char value[3] = {};
  auto result = entry_.ReadValue(as_writable_bytes(span(value)), 3);

  ASSERT_EQ(Status::OK, result.status());
  EXPECT_EQ(3u, result.size());
  EXPECT_EQ(value[0], 'U');
  EXPECT_EQ(value[1], 'E');
  EXPECT_EQ(value[2], '!');
}

TEST_F(ValidEntryInFlash, ReadValue_WithOffset_BufferTooSmall) {
  char value[1] = {};
  auto result = entry_.ReadValue(as_writable_bytes(span(value)), 4);

  ASSERT_EQ(Status::RESOURCE_EXHAUSTED, result.status());
  EXPECT_EQ(1u, result.size());
  EXPECT_EQ(value[0], 'E');
}

TEST_F(ValidEntryInFlash, ReadValue_WithOffset_EmptyRead) {
  char value[16] = {'?'};
  auto result = entry_.ReadValue(as_writable_bytes(span(value)), 6);

  ASSERT_EQ(Status::OK, result.status());
  EXPECT_EQ(0u, result.size());
  EXPECT_EQ(value[0], '?');
}

TEST_F(ValidEntryInFlash, ReadValue_WithOffset_PastEnd) {
  char value[16] = {};
  auto result = entry_.ReadValue(as_writable_bytes(span(value)), 7);

  EXPECT_EQ(Status::OUT_OF_RANGE, result.status());
  EXPECT_EQ(0u, result.size());
}

TEST(ValidEntry, Write) {
  FakeFlashBuffer<1024, 4> flash;
  FlashPartition partition(&flash);
  ChecksumCrc16 checksum;
  const EntryFormat format{0x600DF00Du, &checksum};

  Entry entry =
      Entry::Valid(partition, 53, format, "key45", kValue1, 32, 0x96979899u);

  auto result = entry.Write("key45", kValue1);
  EXPECT_EQ(Status::OK, result.status());
  EXPECT_EQ(32u, result.size());
  EXPECT_EQ(std::memcmp(&flash.buffer()[53], kEntry1.data(), kEntry1.size()),
            0);
}

constexpr auto kHeader2 = ByteStr(
    "\x0d\xf0\x0d\x60"  // magic
    "\xd5\xf5\x00\x00"  // checksum (CRC16)
    "\x00"              // alignment
    "\x01"              // key length
    "\xff\xff"          // value size
    "\x00\x01\x02\x03"  // key version
);

constexpr auto kKeyAndPadding2 = ByteStr("K\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0");

class TombstoneEntryInFlash : public ::testing::Test {
 protected:
  TombstoneEntryInFlash()
      : flash_(AsBytes(kHeader2, kKeyAndPadding2)), partition_(&flash_) {
    EXPECT_EQ(Status::OK, Entry::Read(partition_, 0, &entry_));
  }

  FakeFlashBuffer<1024, 4> flash_;
  FlashPartition partition_;
  Entry entry_;
};

TEST_F(TombstoneEntryInFlash, PassesChecksumVerification) {
  ChecksumCrc16 checksum;
  EXPECT_EQ(Status::OK, entry_.VerifyChecksumInFlash(&checksum));
  EXPECT_EQ(Status::OK, entry_.VerifyChecksum(&checksum, "K", {}));
}

TEST_F(TombstoneEntryInFlash, HeaderContents) {
  EXPECT_EQ(entry_.magic(), 0x600DF00Du);
  EXPECT_EQ(entry_.key_length(), 1u);
  EXPECT_EQ(entry_.value_size(), 0u);
  EXPECT_EQ(entry_.transaction_id(), 0x03020100u);
  EXPECT_TRUE(entry_.deleted());
}

TEST_F(TombstoneEntryInFlash, ReadKey) {
  Entry::KeyBuffer key = {};
  auto result = entry_.ReadKey(key);

  ASSERT_EQ(Status::OK, result.status());
  EXPECT_EQ(result.size(), entry_.key_length());
  EXPECT_STREQ(key.data(), "K");
}

TEST_F(TombstoneEntryInFlash, ReadValue) {
  char value[32] = {};
  auto result = entry_.ReadValue(as_writable_bytes(span(value)));

  ASSERT_EQ(Status::OK, result.status());
  EXPECT_EQ(0u, result.size());
}

TEST(TombstoneEntry, Write) {
  FakeFlashBuffer<1024, 4> flash;
  FlashPartition partition(&flash);
  ChecksumCrc16 checksum;
  const EntryFormat format{0x600DF00Du, &checksum};

  Entry entry = Entry::Tombstone(partition, 16, format, "K", 16, 0x03020100);

  auto result = entry.Write("K", {});
  EXPECT_EQ(Status::OK, result.status());
  EXPECT_EQ(32u, result.size());
  EXPECT_EQ(std::memcmp(&flash.buffer()[16],
                        AsBytes(kHeader2, kKeyAndPadding2).data(),
                        kEntry1.size()),
            0);
}

TEST(Entry, Checksum_NoChecksumRequiresZero) {
  FakeFlashBuffer<1024, 4> flash(kEntry1);
  FlashPartition partition(&flash);
  Entry entry;
  ASSERT_EQ(Status::OK, Entry::Read(partition, 0, &entry));

  EXPECT_EQ(Status::DATA_LOSS, entry.VerifyChecksumInFlash(nullptr));
  EXPECT_EQ(Status::DATA_LOSS, entry.VerifyChecksum(nullptr, {}, {}));

  std::memset(&flash.buffer()[4], 0, 4);  // set the checksum field to 0
  ASSERT_EQ(Status::OK, Entry::Read(partition, 0, &entry));
  EXPECT_EQ(Status::OK, entry.VerifyChecksumInFlash(nullptr));
  EXPECT_EQ(Status::OK, entry.VerifyChecksum(nullptr, {}, {}));
}

TEST(Entry, Checksum_ChecksPadding) {
  FakeFlashBuffer<1024, 4> flash(
      AsBytes(kHeader1, kKey1, kValue1, ByteStr("\0\0\0\0\1")));
  FlashPartition partition(&flash);
  Entry entry;
  ASSERT_EQ(Status::OK, Entry::Read(partition, 0, &entry));

  // Last byte in padding is a 1; should fail.
  ChecksumCrc16 checksum;
  EXPECT_EQ(Status::DATA_LOSS, entry.VerifyChecksumInFlash(&checksum));

  // The in-memory verification fills in 0s for the padding.
  EXPECT_EQ(Status::OK, entry.VerifyChecksum(&checksum, "key45", kValue1));

  flash.buffer()[kEntry1.size() - 1] = byte{0};
  EXPECT_EQ(Status::OK, entry.VerifyChecksumInFlash(&checksum));
}
}  // namespace
}  // namespace pw::kvs::internal
