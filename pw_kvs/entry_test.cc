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
#include "pw_kvs/checksum.h"
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

  for (size_t alignment_bytes = 1; alignment_bytes <= 4096; ++alignment_bytes) {
    FlashPartition partition(&flash, 0, flash.sector_count(), alignment_bytes);
    const size_t align = AlignUp(alignment_bytes, Entry::kMinAlignmentBytes);

    for (size_t value : {size_t(0), align - 1, align, align + 1, 2 * align}) {
      Entry entry =
          Entry::Valid(partition, 0, kFormat, "k", {nullptr, value}, 0);

      ASSERT_EQ(AlignUp(sizeof(EntryHeader) + 1 /* key */ + value, align),
                entry.size());
    }

    Entry entry = Entry::Tombstone(partition, 0, kFormat, "k", 0);
    ASSERT_EQ(AlignUp(sizeof(EntryHeader) + 1 /* key */, align), entry.size());
  }
}

TEST(Entry, Construct_ValidEntry) {
  FakeFlashBuffer<64, 2> flash(16);
  FlashPartition partition(&flash, 0, flash.sector_count());

  auto entry =
      Entry::Valid(partition, 1, kFormat, "k", as_bytes(span("123")), 9876);

  EXPECT_FALSE(entry.deleted());
  EXPECT_EQ(entry.magic(), kFormat.magic);
  EXPECT_EQ(entry.value_size(), sizeof("123"));
  EXPECT_EQ(entry.transaction_id(), 9876u);
}

TEST(Entry, Construct_Tombstone) {
  FakeFlashBuffer<64, 2> flash(16);
  FlashPartition partition(&flash, 0, flash.sector_count());

  auto entry = Entry::Tombstone(partition, 1, kFormat, "key", 123);

  EXPECT_TRUE(entry.deleted());
  EXPECT_EQ(entry.magic(), kFormat.magic);
  EXPECT_EQ(entry.value_size(), 0u);
  EXPECT_EQ(entry.transaction_id(), 123u);
}

constexpr uint32_t kMagicWithChecksum = 0x600df00d;
constexpr uint32_t kTransactionId1 = 0x96979899;

constexpr auto kKey1 = ByteStr("key45");
constexpr auto kValue1 = ByteStr("VALUE!");
constexpr auto kPadding1 = ByteStr("\0\0\0\0\0");

constexpr auto kHeader1 = AsBytes(kMagicWithChecksum,
                                  uint32_t(0x65c5),          // checksum (CRC16)
                                  uint8_t(1),                // alignment (32 B)
                                  uint8_t(kKey1.size()),     // key length
                                  uint16_t(kValue1.size()),  // value size
                                  kTransactionId1            // transaction ID
);

constexpr auto kEntry1 = AsBytes(kHeader1, kKey1, kValue1, kPadding1);
static_assert(kEntry1.size() == 32);

ChecksumCrc16 checksum;
constexpr EntryFormat kFormatWithChecksum{kMagicWithChecksum, &checksum};
constexpr internal::EntryFormats kFormats(kFormatWithChecksum);

class ValidEntryInFlash : public ::testing::Test {
 protected:
  ValidEntryInFlash() : flash_(kEntry1), partition_(&flash_) {
    EXPECT_EQ(Status::OK, Entry::Read(partition_, 0, kFormats, &entry_));
  }

  FakeFlashBuffer<1024, 4> flash_;
  FlashPartition partition_;
  Entry entry_;
};

TEST_F(ValidEntryInFlash, PassesChecksumVerification) {
  EXPECT_EQ(Status::OK, entry_.VerifyChecksumInFlash());
  EXPECT_EQ(Status::OK, entry_.VerifyChecksum("key45", kValue1));
}

TEST_F(ValidEntryInFlash, HeaderContents) {
  EXPECT_EQ(entry_.magic(), kMagicWithChecksum);
  EXPECT_EQ(entry_.key_length(), 5u);
  EXPECT_EQ(entry_.value_size(), 6u);
  EXPECT_EQ(entry_.transaction_id(), kTransactionId1);
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
  FlashPartition partition(&flash, 0, flash.sector_count(), 32);

  Entry entry = Entry::Valid(
      partition, 53, kFormatWithChecksum, "key45", kValue1, kTransactionId1);

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
    "\x00\x01\x02\x03"  // transaction ID
);

constexpr auto kKeyAndPadding2 = ByteStr("K\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0");

class TombstoneEntryInFlash : public ::testing::Test {
 protected:
  TombstoneEntryInFlash()
      : flash_(AsBytes(kHeader2, kKeyAndPadding2)), partition_(&flash_) {
    EXPECT_EQ(Status::OK, Entry::Read(partition_, 0, kFormats, &entry_));
  }

  FakeFlashBuffer<1024, 4> flash_;
  FlashPartition partition_;
  Entry entry_;
};

TEST_F(TombstoneEntryInFlash, PassesChecksumVerification) {
  EXPECT_EQ(Status::OK, entry_.VerifyChecksumInFlash());
  EXPECT_EQ(Status::OK, entry_.VerifyChecksum("K", {}));
}

TEST_F(TombstoneEntryInFlash, HeaderContents) {
  EXPECT_EQ(entry_.magic(), kMagicWithChecksum);
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

  Entry entry =
      Entry::Tombstone(partition, 16, kFormatWithChecksum, "K", 0x03020100);

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

  const EntryFormat format{kMagicWithChecksum, nullptr};
  const internal::EntryFormats formats(format);

  ASSERT_EQ(Status::OK, Entry::Read(partition, 0, formats, &entry));

  EXPECT_EQ(Status::DATA_LOSS, entry.VerifyChecksumInFlash());
  EXPECT_EQ(Status::DATA_LOSS, entry.VerifyChecksum({}, {}));

  std::memset(&flash.buffer()[4], 0, 4);  // set the checksum field to 0
  ASSERT_EQ(Status::OK, Entry::Read(partition, 0, formats, &entry));
  EXPECT_EQ(Status::OK, entry.VerifyChecksumInFlash());
  EXPECT_EQ(Status::OK, entry.VerifyChecksum({}, {}));
}

TEST(Entry, Checksum_ChecksPadding) {
  FakeFlashBuffer<1024, 4> flash(
      AsBytes(kHeader1, kKey1, kValue1, ByteStr("\0\0\0\0\1")));
  FlashPartition partition(&flash);
  Entry entry;
  ASSERT_EQ(Status::OK, Entry::Read(partition, 0, kFormats, &entry));

  // Last byte in padding is a 1; should fail.
  EXPECT_EQ(Status::DATA_LOSS, entry.VerifyChecksumInFlash());

  // The in-memory verification fills in 0s for the padding.
  EXPECT_EQ(Status::OK, entry.VerifyChecksum("key45", kValue1));

  flash.buffer()[kEntry1.size() - 1] = byte{0};
  EXPECT_EQ(Status::OK, entry.VerifyChecksumInFlash());
}

class EntryInFlash : public ::testing::Test {
 protected:
  static constexpr EntryFormat kNoChecksum{.magic = 0xf000000d,
                                           .checksum = nullptr};

  EntryInFlash() : flash_(AsBytes(kEntry1)), partition_(&flash_) {
    ASSERT_EQ(Status::OK, Entry::Read(partition_, 0, kFormats, &entry_));
  }

  FakeFlashBuffer<1024, 4> flash_;
  FlashPartition partition_;
  Entry entry_;
};

TEST_F(EntryInFlash, Update_SameFormat_TransactionIdIsUpdated) {
  ASSERT_EQ(Status::OK,
            entry_.Update(kFormatWithChecksum, kTransactionId1 + 3));

  EXPECT_EQ(kFormatWithChecksum.magic, entry_.magic());
  EXPECT_EQ(0u, entry_.address());
  EXPECT_EQ(kTransactionId1 + 3, entry_.transaction_id());
  EXPECT_FALSE(entry_.deleted());
}

TEST_F(EntryInFlash, Update_DifferentFormat_MagicAndTransactionIdAreUpdated) {
  ASSERT_EQ(Status::OK, entry_.Update(kFormat, kTransactionId1 + 6));

  EXPECT_EQ(kFormat.magic, entry_.magic());
  EXPECT_EQ(0u, entry_.address());
  EXPECT_EQ(kTransactionId1 + 6, entry_.transaction_id());
  EXPECT_FALSE(entry_.deleted());
}

TEST_F(EntryInFlash, Update_ReadError_NoChecksumIsOkay) {
  flash_.InjectReadError(FlashError::Unconditional(Status::ABORTED));

  EXPECT_EQ(Status::ABORTED,
            entry_.Update(kFormatWithChecksum, kTransactionId1 + 1));
}

TEST_F(EntryInFlash, Update_ReadError_WithChecksumIsError) {
  flash_.InjectReadError(FlashError::Unconditional(Status::ABORTED));

  EXPECT_EQ(Status::OK, entry_.Update(kNoChecksum, kTransactionId1 + 1));
}

TEST_F(EntryInFlash, Copy) {
  auto result = entry_.Copy(123);

  EXPECT_EQ(Status::OK, result.status());
  EXPECT_EQ(entry_.size(), result.size());
  EXPECT_EQ(0,
            std::memcmp(
                &flash_.buffer().data()[123], kEntry1.data(), kEntry1.size()));
}

TEST_F(EntryInFlash, Copy_ReadError) {
  flash_.InjectReadError(FlashError::Unconditional(Status::UNIMPLEMENTED));
  auto result = entry_.Copy(kEntry1.size());
  EXPECT_EQ(Status::UNIMPLEMENTED, result.status());
  EXPECT_EQ(0u, result.size());
}

constexpr uint32_t ByteSum(span<const byte> bytes, uint32_t value = 0) {
  for (byte b : bytes) {
    value += unsigned(b);
  }
  return value;
}

TEST_F(EntryInFlash, UpdateAndCopy_DifferentChecksum_UpdatesToNewFormat) {
  static class Sum final : public ChecksumAlgorithm {
   public:
    Sum() : ChecksumAlgorithm(as_bytes(span(&state_, 1))), state_(0) {}

    void Update(span<const byte> data) override {
      state_ = ByteSum(data, state_);
    }

    void Reset() override { state_ = 0; }

   private:
    uint32_t state_;
  } sum_checksum;

  constexpr EntryFormat sum_format{.magic = 0x12345678,
                                   .checksum = &sum_checksum};

  ASSERT_EQ(Status::OK, entry_.Update(sum_format, kTransactionId1 + 9));

  auto result = entry_.Copy(kEntry1.size());
  ASSERT_EQ(Status::OK, result.status());
  EXPECT_EQ(kEntry1.size(), result.size());

  constexpr uint32_t checksum =
      ByteSum(AsBytes(sum_format.magic)) + 0 /* checksum */ +
      0 /* alignment */ + kKey1.size() + kValue1.size() +
      ByteSum(AsBytes(kTransactionId1 + 9)) + ByteSum(kKey1) + ByteSum(kValue1);

  constexpr auto kNewHeader1 =
      AsBytes(sum_format.magic,          // magic
              checksum,                  // checksum (byte sum)
              uint8_t(0),                // alignment (changed to 16 B from 32)
              uint8_t(kKey1.size()),     // key length
              uint16_t(kValue1.size()),  // value size
              kTransactionId1 + 9);      // transaction ID
  constexpr auto kNewEntry1 = AsBytes(kNewHeader1, kKey1, kValue1, kPadding1);

  EXPECT_EQ(0,
            std::memcmp(&flash_.buffer()[kEntry1.size()],
                        kNewEntry1.data(),
                        kNewEntry1.size()));
}

TEST_F(EntryInFlash, UpdateAndCopy_NoChecksum_UpdatesToNewFormat) {
  constexpr EntryFormat no_checksum{.magic = 0xf000000d, .checksum = nullptr};

  ASSERT_EQ(Status::OK, entry_.Update(no_checksum, kTransactionId1 + 1));

  auto result = entry_.Copy(kEntry1.size());
  ASSERT_EQ(Status::OK, result.status());
  EXPECT_EQ(kEntry1.size(), result.size());

  constexpr auto kNewHeader1 =
      AsBytes(no_checksum.magic,         // magic
              uint32_t(0),               // checksum (none)
              uint8_t(0),                // alignment (changed to 16 B from 32)
              uint8_t(kKey1.size()),     // key length
              uint16_t(kValue1.size()),  // value size
              kTransactionId1 + 1);      // transaction ID
  constexpr auto kNewEntry1 = AsBytes(kNewHeader1, kKey1, kValue1, kPadding1);

  EXPECT_EQ(0,
            std::memcmp(&flash_.buffer()[kEntry1.size()],
                        kNewEntry1.data(),
                        kNewEntry1.size()));
}

TEST_F(EntryInFlash, UpdateAndCopy_WriteError) {
  flash_.InjectWriteError(FlashError::Unconditional(Status::CANCELLED));

  ASSERT_EQ(Status::OK, entry_.Update(kNoChecksum, kTransactionId1 + 1));

  auto result = entry_.Copy(kEntry1.size());
  EXPECT_EQ(Status::CANCELLED, result.status());
  EXPECT_EQ(kEntry1.size(), result.size());
}

}  // namespace
}  // namespace pw::kvs::internal
