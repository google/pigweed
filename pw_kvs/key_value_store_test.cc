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

#include "pw_kvs/key_value_store.h"

#include "pw_kvs/assert.h"
#include "pw_kvs/devices/flash_memory.h"
#include "pw_kvs/os/stack_checks.h"
#include "pw_kvs/platform/board.h"
#include "pw_kvs/status.h"
#include "pw_kvs/test/fixture.h"
#include "pw_kvs/test/framework.h"
#include "pw_kvs/test/status_macros.h"
#include "pw_kvs/util/ccitt_crc16.h"

#if USE_MEMORY_BUFFER
#include "pw_kvs/test/fakes/in_memory_fake_flash.h"
#endif  // USE_MEMORY_BUFFER

namespace pw {
namespace {

#if USE_MEMORY_BUFFER
// Although it might be useful to test other configurations, some tests require
// at least 3 sectors; therfore it should have this when checked in.
InMemoryFakeFlash<4 * 1024, 4> test_flash(
    16);  // 4 x 4k sectors, 16 byte alignment
FlashPartition test_partition(&test_flash, 0, test_flash.GetSectorCount());
InMemoryFakeFlash<1024, 60> large_test_flash(8);
FlashPartition large_test_partition(&large_test_flash,
                                    0,
                                    large_test_flash.GetSectorCount());
#else   // Device test
FlashPartition& test_partition = Board::Instance().FlashExternalTestPartition();
#endif  // USE_MEMORY_BUFFER

KeyValueStore kvs(&test_partition);

// Use test fixture for logging support
class KeyValueStoreTest : public ::testing::Test {
 protected:
  KeyValueStoreTest() : kvs_local_(&test_partition) {}

  KeyValueStore kvs_local_;
};

std::array<uint8_t, 512> buffer;
std::array<const char*, 3> keys{"TestKey1", "Key2", "TestKey3"};

Status PaddedWrite(FlashPartition* partition,
                   FlashPartition::Address address,
                   const uint8_t* buf,
                   uint16_t size) {
  static constexpr uint16_t kMaxAlignmentBytes = 128;
  uint8_t alignment_buffer[kMaxAlignmentBytes] = {0};
  uint16_t aligned_bytes = size - (size % partition->GetAlignmentBytes());
  RETURN_IF_ERROR(partition->Write(address, buf, aligned_bytes));
  uint16_t remaining_bytes = size - aligned_bytes;
  if (remaining_bytes > 0) {
    memcpy(alignment_buffer, &buf[aligned_bytes], remaining_bytes);
    RETURN_IF_ERROR(partition->Write(address + aligned_bytes,
                                     alignment_buffer,
                                     partition->GetAlignmentBytes()));
  }
  return Status::OK;
}

size_t RoundUpForAlignment(size_t size) {
  uint16_t alignment = test_partition.GetAlignmentBytes();
  if (size % alignment != 0) {
    return size + alignment - size % alignment;
  }
  return size;
}

// This class gives attributes of KVS that we are testing against
class KvsAttributes {
 public:
  KvsAttributes(size_t key_size, size_t data_size)
      : sector_header_meta_size_(
            RoundUpForAlignment(KeyValueStore::kHeaderSize)),
        sector_header_clean_size_(
            RoundUpForAlignment(KeyValueStore::kHeaderSize)),
        chunk_header_size_(RoundUpForAlignment(KeyValueStore::kHeaderSize)),
        data_size_(RoundUpForAlignment(data_size)),
        key_size_(RoundUpForAlignment(key_size)),
        erase_size_(chunk_header_size_ + key_size_),
        min_put_size_(chunk_header_size_ + key_size_ + data_size_) {}

  size_t SectorHeaderSize() {
    return sector_header_meta_size_ + sector_header_clean_size_;
  }
  size_t SectorHeaderMetaSize() { return sector_header_meta_size_; }
  size_t ChunkHeaderSize() { return chunk_header_size_; }
  size_t DataSize() { return data_size_; }
  size_t KeySize() { return key_size_; }
  size_t EraseSize() { return erase_size_; }
  size_t MinPutSize() { return min_put_size_; }

 private:
  const size_t sector_header_meta_size_;
  const size_t sector_header_clean_size_;
  const size_t chunk_header_size_;
  const size_t data_size_;
  const size_t key_size_;
  const size_t erase_size_;
  const size_t min_put_size_;
};

// Intention of this is to put and erase key-val to fill up sectors. It's a
// helper function in testing how KVS handles cases where flash sector is full
// or near full.
void FillKvs(const char* key, size_t size_to_fill) {
  constexpr size_t kTestDataSize = 8;
  KvsAttributes kvs_attr(std::strlen(key), kTestDataSize);
  const size_t kMaxPutSize =
      buffer.size() + kvs_attr.ChunkHeaderSize() + kvs_attr.KeySize();

  CHECK(size_to_fill >= kvs_attr.MinPutSize() + kvs_attr.EraseSize());

  // Saving enough space to perform erase after loop
  size_to_fill -= kvs_attr.EraseSize();
  // We start with possible small chunk to prevent too small of a Kvs.Put() at
  // the end.
  size_t chunk_len =
      std::max(kvs_attr.MinPutSize(), size_to_fill % buffer.size());
  memset(buffer.data(), 0, buffer.size());
  while (size_to_fill > 0) {
    buffer[0]++;  // Changing buffer value so put actually does something
    ASSERT_OK(
        kvs.Put(key,
                buffer.data(),
                chunk_len - kvs_attr.ChunkHeaderSize() - kvs_attr.KeySize()));
    size_to_fill -= chunk_len;
    chunk_len = std::min(size_to_fill, kMaxPutSize);
  }
  ASSERT_OK(kvs.Erase(key));
}

uint16_t CalcKvsCrc(const char* key, const void* data, size_t data_len) {
  CcittCrc16 crc;
  static constexpr size_t kChunkKeyLengthMax = 15;
  crc.AppendBytes(
      ConstBuffer(reinterpret_cast<const uint8_t*>(key),
                  pw::util::StringLength(key, kChunkKeyLengthMax)));
  return crc.AppendBytes(
      ConstBuffer(reinterpret_cast<const uint8_t*>(data), data_len));
}

uint16_t CalcTestPartitionCrc() {
  uint8_t buf[16];  // Read as 16 byte chunks
  CHECK_EQ(sizeof(buf) % test_partition.GetAlignmentBytes(), 0);
  CHECK_EQ(test_partition.GetSizeBytes() % sizeof(buf), 0);
  CcittCrc16 crc;
  for (size_t i = 0; i < test_partition.GetSizeBytes(); i += sizeof(buf)) {
    test_partition.Read(buf, i, sizeof(buf));
    crc.AppendBytes(ConstBuffer(buf, sizeof(buf)));
  }
  return crc.CurrentValue();
}
}  // namespace

TEST_F(KeyValueStoreTest, FuzzTest) {
  if (test_partition.GetSectorSizeBytes() < 4 * 1024 ||
      test_partition.GetSectorCount() < 4) {
    LOG_INFO("Sectors too small, skipping test.");
    return;  // TODO: Test could be generalized
  }
  // Erase
  ASSERT_OK(test_partition.Erase(0, test_partition.GetSectorCount()));

  // Reset KVS
  kvs.Disable();
  ASSERT_OK(kvs.Enable());
  const char* key1 = "Buf1";
  const char* key2 = "Buf2";
  const size_t kLargestBufSize = 3 * 1024;
  static uint8_t buf1[kLargestBufSize];
  static uint8_t buf2[kLargestBufSize];
  memset(buf1, 1, sizeof(buf1));
  memset(buf2, 2, sizeof(buf2));

  // Start with things in KVS
  ASSERT_OK(kvs.Put(key1, buf1, sizeof(buf1)));
  ASSERT_OK(kvs.Put(key2, buf2, sizeof(buf2)));
  for (size_t j = 0; j < keys.size(); j++) {
    ASSERT_OK(kvs.Put(keys[j], j));
  }

  for (size_t i = 0; i < 100; i++) {
    // Vary two sizes
    size_t size1 = (kLargestBufSize) / (i + 1);
    size_t size2 = (kLargestBufSize) / (100 - i);
    for (size_t j = 0; j < 50; j++) {
      // Rewrite a single key many times, can fill up a sector
      ASSERT_OK(kvs.Put("some_data", j));
    }
    // Delete and re-add everything
    ASSERT_OK(kvs.Erase(key1));
    ASSERT_OK(kvs.Put(key1, buf1, size1));
    ASSERT_OK(kvs.Erase(key2));
    ASSERT_OK(kvs.Put(key2, buf2, size2));
    for (size_t j = 0; j < keys.size(); j++) {
      ASSERT_OK(kvs.Erase(keys[j]));
      ASSERT_OK(kvs.Put(keys[j], j));
    }

    // Re-enable and verify
    kvs.Disable();
    ASSERT_OK(kvs.Enable());
    static uint8_t buf[4 * 1024];
    ASSERT_OK(kvs.Get(key1, buf, size1));
    ASSERT_EQ(memcmp(buf, buf1, size1), 0);
    ASSERT_OK(kvs.Get(key2, buf, size2));
    ASSERT_EQ(memcmp(buf2, buf2, size2), 0);
    for (size_t j = 0; j < keys.size(); j++) {
      size_t ret = 1000;
      ASSERT_OK(kvs.Get(keys[j], &ret));
      ASSERT_EQ(ret, j);
    }
  }
}

TEST_F(KeyValueStoreTest, Basic) {
  // Erase
  ASSERT_OK(test_partition.Erase(0, test_partition.GetSectorCount()));

  // Reset KVS
  kvs.Disable();
  ASSERT_OK(kvs.Enable());

  // Add some data
  uint8_t value1 = 0xDA;
  ASSERT_OK(kvs.Put(keys[0], &value1, sizeof(value1)));

  uint32_t value2 = 0xBAD0301f;
  ASSERT_OK(
      kvs.Put(keys[1], reinterpret_cast<uint8_t*>(&value2), sizeof(value2)));

  // Verify data
  uint32_t test2;
  EXPECT_OK(
      kvs.Get(keys[1], reinterpret_cast<uint8_t*>(&test2), sizeof(test2)));
  uint8_t test1;
  ASSERT_OK(
      kvs.Get(keys[0], reinterpret_cast<uint8_t*>(&test1), sizeof(test1)));

  EXPECT_EQ(test1, value1);
  EXPECT_EQ(test2, value2);

  // Erase a key
  EXPECT_OK(kvs.Erase(keys[0]));

  // Verify it was erased
  EXPECT_EQ(kvs.Get(keys[0], reinterpret_cast<uint8_t*>(&test1), sizeof(test1))
                .code(),
            Status::NOT_FOUND);
  test2 = 0;
  ASSERT_OK(
      kvs.Get(keys[1], reinterpret_cast<uint8_t*>(&test2), sizeof(test2)));
  EXPECT_EQ(test2, value2);

  // Erase other key
  kvs.Erase(keys[1]);

  // Verify it was erased
  EXPECT_EQ(kvs.KeyCount(), 0);
}

TEST_F(KeyValueStoreTest, MaxKeyLength) {
  // Erase
  ASSERT_OK(test_partition.Erase(0, test_partition.GetSectorCount()));

  // Reset KVS
  kvs.Disable();
  ASSERT_OK(kvs.Enable());

  // Add some data
  char key[16] = "123456789abcdef";  // key length 15 (without \0)
  int value = 1;
  ASSERT_OK(kvs.Put(key, value));

  // Verify data
  int test = 0;
  ASSERT_OK(kvs.Get(key, &test));
  EXPECT_EQ(test, value);

  // Erase a key
  kvs.Erase(key);

  // Verify it was erased
  EXPECT_EQ(kvs.Get(key, &test).code(), Status::NOT_FOUND);
}

TEST_F(KeyValueStoreTest, LargeBuffers) {
  test_partition.Erase(0, test_partition.GetSectorCount());

  // Note this assumes that no other keys larger then key0
  static_assert(sizeof(keys[0]) >= sizeof(keys[1]) &&
                sizeof(keys[0]) >= sizeof(keys[2]));
  KvsAttributes kvs_attr(std::strlen(keys[0]), buffer.size());

  // Verify the data will fit in this test partition. This checks that all the
  // keys chunks will fit and a header for each sector will fit. It requires 1
  // empty sector also.
  const size_t kAllChunkSize = kvs_attr.MinPutSize() * keys.size();
  const size_t kAllSectorHeaderSizes =
      kvs_attr.SectorHeaderSize() * (test_partition.GetSectorCount() - 1);
  const size_t kMinSize = kAllChunkSize + kAllSectorHeaderSizes;
  const size_t kAvailSectorSpace = test_partition.GetSectorSizeBytes() *
                                   (test_partition.GetSectorCount() - 1);
  if (kAvailSectorSpace < kMinSize) {
    LOG_INFO("KVS too small, skipping test.");
    return;
  }
  // Reset KVS
  kvs.Disable();
  ASSERT_OK(kvs.Enable());

  // Add and verify
  for (unsigned add_idx = 0; add_idx < keys.size(); add_idx++) {
    memset(buffer.data(), add_idx, buffer.size());
    ASSERT_OK(kvs.Put(keys[add_idx], buffer.data(), buffer.size()));
    EXPECT_EQ(kvs.KeyCount(), add_idx + 1);
    for (unsigned verify_idx = 0; verify_idx <= add_idx; verify_idx++) {
      memset(buffer.data(), 0, buffer.size());
      ASSERT_OK(kvs.Get(keys[verify_idx], buffer.data(), buffer.size()));
      for (unsigned i = 0; i < buffer.size(); i++) {
        EXPECT_EQ(buffer[i], verify_idx);
      }
    }
  }

  // Erase and verify
  for (unsigned erase_idx = 0; erase_idx < keys.size(); erase_idx++) {
    ASSERT_OK(kvs.Erase(keys[erase_idx]));
    EXPECT_EQ(kvs.KeyCount(), keys.size() - erase_idx - 1);
    for (unsigned verify_idx = 0; verify_idx < keys.size(); verify_idx++) {
      memset(buffer.data(), 0, buffer.size());
      if (verify_idx <= erase_idx) {
        ASSERT_EQ(
            kvs.Get(keys[verify_idx], buffer.data(), buffer.size()).code(),
            Status::NOT_FOUND);
      } else {
        ASSERT_OK(kvs.Get(keys[verify_idx], buffer.data(), buffer.size()));
        for (uint32_t i = 0; i < buffer.size(); i++) {
          EXPECT_EQ(buffer[i], verify_idx);
        }
      }
    }
  }
}

TEST_F(KeyValueStoreTest, Enable) {
  test_partition.Erase(0, test_partition.GetSectorCount());

  KvsAttributes kvs_attr(std::strlen(keys[0]), buffer.size());

  // Verify the data will fit in this test partition. This checks that all the
  // keys chunks will fit and a header for each sector will fit. It requires 1
  // empty sector also.
  const size_t kAllChunkSize = kvs_attr.MinPutSize() * keys.size();
  const size_t kAllSectorHeaderSizes =
      kvs_attr.SectorHeaderSize() * (test_partition.GetSectorCount() - 1);
  const size_t kMinSize = kAllChunkSize + kAllSectorHeaderSizes;
  const size_t kAvailSectorSpace = test_partition.GetSectorSizeBytes() *
                                   (test_partition.GetSectorCount() - 1);
  if (kAvailSectorSpace < kMinSize) {
    LOG_INFO("KVS too small, skipping test.");
    return;
  }

  // Reset KVS
  kvs.Disable();
  ASSERT_OK(kvs.Enable());

  // Add some items
  for (unsigned add_idx = 0; add_idx < keys.size(); add_idx++) {
    memset(buffer.data(), add_idx, buffer.size());
    ASSERT_OK(kvs.Put(keys[add_idx], buffer.data(), buffer.size()));
    EXPECT_EQ(kvs.KeyCount(), add_idx + 1);
  }

  // Enable different KVS which should be able to properly setup the same map
  // from what is stored in flash.
  ASSERT_OK(kvs_local_.Enable());
  EXPECT_EQ(kvs_local_.KeyCount(), keys.size());

  // Ensure adding to new KVS works
  uint8_t value = 0xDA;
  const char* key = "new_key";
  ASSERT_OK(kvs_local_.Put(key, &value, sizeof(value)));
  uint8_t test;
  ASSERT_OK(kvs_local_.Get(key, &test, sizeof(test)));
  EXPECT_EQ(value, test);
  EXPECT_EQ(kvs_local_.KeyCount(), keys.size() + 1);

  // Verify previous data
  for (unsigned verify_idx = 0; verify_idx < keys.size(); verify_idx++) {
    memset(buffer.data(), 0, buffer.size());
    ASSERT_OK(kvs_local_.Get(keys[verify_idx], buffer.data(), buffer.size()));
    for (uint32_t i = 0; i < buffer.size(); i++) {
      EXPECT_EQ(buffer[i], verify_idx);
    }
  }
}

TEST_F(KeyValueStoreTest, MultiSector) {
  test_partition.Erase(0, test_partition.GetSectorCount());

  // Reset KVS
  kvs.Disable();
  ASSERT_OK(kvs.Enable());

  // Calculate number of elements to ensure multiple sectors are required.
  uint16_t add_count =
      (test_partition.GetSectorSizeBytes() / buffer.size()) + 1;

  if (kvs.GetMaxKeys() < add_count) {
    LOG_INFO("Sector size too large, skipping test.");
    return;  // this chip has very large sectors, test won't work
  }
  if (test_partition.GetSectorCount() < 3) {
    LOG_INFO("Not enough sectors, skipping test.");
    return;  // need at least 3 sectors for multi-sector test
  }

  char key[20];
  for (unsigned add_idx = 0; add_idx < add_count; add_idx++) {
    memset(buffer.data(), add_idx, buffer.size());
    snprintf(key, sizeof(key), "key_%u", add_idx);
    ASSERT_OK(kvs.Put(key, buffer.data(), buffer.size()));
    EXPECT_EQ(kvs.KeyCount(), add_idx + 1);
  }

  for (unsigned verify_idx = 0; verify_idx < add_count; verify_idx++) {
    memset(buffer.data(), 0, buffer.size());
    snprintf(key, sizeof(key), "key_%u", verify_idx);
    ASSERT_OK(kvs.Get(key, buffer.data(), buffer.size()));
    for (uint32_t i = 0; i < buffer.size(); i++) {
      EXPECT_EQ(buffer[i], verify_idx);
    }
  }

  // Check erase
  for (unsigned erase_idx = 0; erase_idx < add_count; erase_idx++) {
    snprintf(key, sizeof(key), "key_%u", erase_idx);
    ASSERT_OK(kvs.Erase(key));
    EXPECT_EQ(kvs.KeyCount(), add_count - erase_idx - 1);
  }
}

TEST_F(KeyValueStoreTest, RewriteValue) {
  test_partition.Erase(0, test_partition.GetSectorCount());

  // Reset KVS
  kvs.Disable();
  ASSERT_OK(kvs.Enable());

  // Write first value
  const uint8_t kValue1 = 0xDA;
  const uint8_t kValue2 = 0x12;
  const char* key = "the_key";
  ASSERT_OK(kvs.Put(key, &kValue1, sizeof(kValue1)));

  // Verify
  uint8_t value;
  ASSERT_OK(kvs.Get(key, reinterpret_cast<uint8_t*>(&value), sizeof(value)));
  EXPECT_EQ(kValue1, value);

  // Write new value for key
  ASSERT_OK(kvs.Put(key, &kValue2, sizeof(kValue2)));

  // Verify
  ASSERT_OK(kvs.Get(key, reinterpret_cast<uint8_t*>(&value), sizeof(value)));
  EXPECT_EQ(kValue2, value);

  // Verify only 1 element exists
  EXPECT_EQ(kvs.KeyCount(), 1);
}

TEST_F(KeyValueStoreTest, OffsetRead) {
  test_partition.Erase(0, test_partition.GetSectorCount());

  // Reset KVS
  kvs.Disable();
  ASSERT_OK(kvs.Enable());

  const char* key = "the_key";
  constexpr uint8_t kReadSize = 16;  // needs to be a multiple of alignment
  constexpr uint8_t kTestBufferSize = kReadSize * 10;
  CHECK_GT(buffer.size(), kTestBufferSize);
  CHECK_LE(kTestBufferSize, 0xFF);

  // Write the entire buffer
  for (uint8_t i = 0; i < kTestBufferSize; i++) {
    buffer[i] = i;
  }
  ASSERT_OK(kvs.Put(key, buffer.data(), kTestBufferSize));
  EXPECT_EQ(kvs.KeyCount(), 1);

  // Read in small chunks and verify
  for (int i = 0; i < kTestBufferSize / kReadSize; i++) {
    memset(buffer.data(), 0, buffer.size());
    ASSERT_OK(kvs.Get(key, buffer.data(), kReadSize, i * kReadSize));
    for (unsigned j = 0; j < kReadSize; j++) {
      ASSERT_EQ(buffer[j], j + i * kReadSize);
    }
  }
}

TEST_F(KeyValueStoreTest, MultipleRewrite) {
  // Write many large buffers to force moving to new sector.
  test_partition.Erase(0, test_partition.GetSectorCount());

  // Reset KVS
  kvs.Disable();
  ASSERT_OK(kvs.Enable());

  // Calculate number of elements to ensure multiple sectors are required.
  unsigned add_count =
      (test_partition.GetSectorSizeBytes() / buffer.size()) + 1;

  const char* key = "the_key";
  constexpr uint8_t kGoodVal = 0x60;
  constexpr uint8_t kBadVal = 0xBA;
  memset(buffer.data(), kBadVal, buffer.size());
  for (unsigned add_idx = 0; add_idx < add_count; add_idx++) {
    if (add_idx == add_count - 1) {  // last value
      memset(buffer.data(), kGoodVal, buffer.size());
    }
    ASSERT_OK(kvs.Put(key, buffer.data(), buffer.size()));
    EXPECT_EQ(kvs.KeyCount(), 1);
  }

  // Verify
  memset(buffer.data(), 0, buffer.size());
  ASSERT_OK(kvs.Get(key, buffer.data(), buffer.size()));
  for (uint32_t i = 0; i < buffer.size(); i++) {
    ASSERT_EQ(buffer[i], kGoodVal);
  }
}

TEST_F(KeyValueStoreTest, FillSector) {
  // Write key[0], Write/erase Key[2] multiple times to fill sector check
  // everything makes sense after.
  test_partition.Erase(0, test_partition.GetSectorCount());

  // Reset KVS
  kvs.Disable();
  ASSERT_OK(kvs.Enable());

  ASSERT_EQ(std::strlen(keys[0]), 8U);  // Easier for alignment
  ASSERT_EQ(std::strlen(keys[2]), 8U);  // Easier for alignment
  constexpr size_t kTestDataSize = 8;
  KvsAttributes kvs_attr(std::strlen(keys[2]), kTestDataSize);
  int bytes_remaining =
      test_partition.GetSectorSizeBytes() - kvs_attr.SectorHeaderSize();
  constexpr uint8_t kKey0Pattern = 0xBA;

  memset(buffer.data(), kKey0Pattern, kvs_attr.DataSize());
  ASSERT_OK(kvs.Put(keys[0], buffer.data(), kvs_attr.DataSize()));
  bytes_remaining -= kvs_attr.MinPutSize();
  memset(buffer.data(), 1, kvs_attr.DataSize());
  ASSERT_OK(kvs.Put(keys[2], buffer.data(), kvs_attr.DataSize()));
  bytes_remaining -= kvs_attr.MinPutSize();
  EXPECT_EQ(kvs.KeyCount(), 2);
  ASSERT_OK(kvs.Erase(keys[2]));
  bytes_remaining -= kvs_attr.EraseSize();
  EXPECT_EQ(kvs.KeyCount(), 1);

  // Intentionally adding erase size to trigger sector cleanup
  bytes_remaining += kvs_attr.EraseSize();
  FillKvs(keys[2], bytes_remaining);

  // Verify key[0]
  memset(buffer.data(), 0, kvs_attr.DataSize());
  ASSERT_OK(kvs.Get(keys[0], buffer.data(), kvs_attr.DataSize()));
  for (uint32_t i = 0; i < kvs_attr.DataSize(); i++) {
    EXPECT_EQ(buffer[i], kKey0Pattern);
  }
}

TEST_F(KeyValueStoreTest, Interleaved) {
  test_partition.Erase(0, test_partition.GetSectorCount());

  // Reset KVS
  kvs.Disable();
  ASSERT_OK(kvs.Enable());

  const uint8_t kValue1 = 0xDA;
  const uint8_t kValue2 = 0x12;
  uint8_t value[1];
  ASSERT_OK(kvs.Put(keys[0], &kValue1, sizeof(kValue1)));
  EXPECT_EQ(kvs.KeyCount(), 1);
  ASSERT_OK(kvs.Erase(keys[0]));
  EXPECT_EQ(kvs.Get(keys[0], value, sizeof(value)), Status::NOT_FOUND);
  ASSERT_OK(kvs.Put(keys[1], &kValue1, sizeof(kValue1)));
  ASSERT_OK(kvs.Put(keys[2], &kValue2, sizeof(kValue2)));
  ASSERT_OK(kvs.Erase(keys[1]));
  EXPECT_OK(kvs.Get(keys[2], value, sizeof(value)));
  EXPECT_EQ(kValue2, value[0]);

  EXPECT_EQ(kvs.KeyCount(), 1);
}

TEST_F(KeyValueStoreTest, BadCrc) {
  static constexpr uint32_t kTestPattern = 0xBAD0301f;
  // clang-format off
  // There is a top and bottom because for each because we don't want to write
  // the erase 0xFF, especially on encrypted flash.
  static constexpr uint8_t kKvsTestDataAligned1Top[] = {
      0xCD, 0xAB, 0x03, 0x00, 0x01, 0x00, 0xFF, 0xFF,  // Sector Header
  };
  static constexpr uint8_t kKvsTestDataAligned1Bottom[] = {
      0xAA, 0x55, 0xBA, 0xDD, 0x00, 0x00, 0x18, 0x00,  // header (BAD CRC)
      0x54, 0x65, 0x73, 0x74, 0x4B, 0x65, 0x79, 0x31,  // Key (keys[0])
      0xDA,                                            // Value
      0xAA, 0x55, 0xB5, 0x87, 0x00, 0x00, 0x44, 0x00,  // Header (GOOD CRC)
      0x4B, 0x65, 0x79, 0x32,                          // Key (keys[1])
      0x1F, 0x30, 0xD0, 0xBA};                         // Value
  static constexpr uint8_t kKvsTestDataAligned2Top[] = {
      0xCD, 0xAB, 0x03, 0x00, 0x02, 0x00, 0xFF, 0xFF,  // Sector Header
  };
  static constexpr uint8_t kKvsTestDataAligned2Bottom[] = {
      0xAA, 0x55, 0xBA, 0xDD, 0x00, 0x00, 0x18, 0x00,  // header (BAD CRC)
      0x54, 0x65, 0x73, 0x74, 0x4B, 0x65, 0x79, 0x31,  // Key (keys[0])
      0xDA, 0x00,                                      // Value + padding
      0xAA, 0x55, 0xB5, 0x87, 0x00, 0x00, 0x44, 0x00,  // Header (GOOD CRC)
      0x4B, 0x65, 0x79, 0x32,                          // Key (keys[1])
      0x1F, 0x30, 0xD0, 0xBA};                         // Value
  static constexpr uint8_t kKvsTestDataAligned8Top[] = {
      0xCD, 0xAB, 0x03, 0x00, 0x08, 0x00, 0xFF, 0xFF,  // Sector Header
  };
  static constexpr uint8_t kKvsTestDataAligned8Bottom[] = {
      0xAA, 0x55, 0xBA, 0xDD, 0x00, 0x00, 0x18, 0x00,  // header (BAD CRC)
      0x54, 0x65, 0x73, 0x74, 0x4B, 0x65, 0x79, 0x31,  // Key (keys[0])
      0xDA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Value + padding
      0xAA, 0x55, 0xB5, 0x87, 0x00, 0x00, 0x44, 0x00,  // header (GOOD CRC)
      0x4B, 0x65, 0x79, 0x32, 0x00, 0x00, 0x00, 0x00,  // Key (keys[1])
      0x1F, 0x30, 0xD0, 0xBA, 0x00, 0x00, 0x00, 0x00,  // Value + padding
  };
  static constexpr uint8_t kKvsTestDataAligned16Top[] = {
      0xCD, 0xAB, 0x03, 0x00, 0x10, 0x00, 0xFF, 0xFF,  // Sector Header
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Alignment to 16
  };
  static constexpr uint8_t kKvsTestDataAligned16Bottom[] = {
      0xAA, 0x55, 0xBA, 0xDD, 0x00, 0x00, 0x18, 0x00,  // header (BAD CRC)
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Alignment to 16
      0x54, 0x65, 0x73, 0x74, 0x4B, 0x65, 0x79, 0x31,  // Key (keys[0])
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Alignment to 16
      0xDA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Value + padding
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Alignment to 16
      0xAA, 0x55, 0xB5, 0x87, 0x00, 0x00, 0x44, 0x00,  // header (GOOD CRC)
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Alignment to 16
      0x4B, 0x65, 0x79, 0x32, 0x00, 0x00, 0x00, 0x00,  // Key (keys[1])
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Alignment to 16
      0x1F, 0x30, 0xD0, 0xBA, 0x00, 0x00, 0x00, 0x00,  // Value + padding
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Alignment to 16
  };
  // clang-format on
  ASSERT_OK(test_partition.Erase(0, test_partition.GetSectorCount()));

  // We don't actually care about the size values provided, since we are only
  // using kvs_attr to get Sector Size
  KvsAttributes kvs_attr(8, 8);
  if (test_partition.GetAlignmentBytes() == 1) {
    ASSERT_OK(test_partition.Write(
        0, kKvsTestDataAligned1Top, sizeof(kKvsTestDataAligned1Top)));
    ASSERT_OK(test_partition.Write(kvs_attr.SectorHeaderSize(),
                                   kKvsTestDataAligned1Bottom,
                                   sizeof(kKvsTestDataAligned1Bottom)));
  } else if (test_partition.GetAlignmentBytes() == 2) {
    ASSERT_OK(test_partition.Write(
        0, kKvsTestDataAligned2Top, sizeof(kKvsTestDataAligned2Top)));
    ASSERT_OK(test_partition.Write(kvs_attr.SectorHeaderSize(),
                                   kKvsTestDataAligned2Bottom,
                                   sizeof(kKvsTestDataAligned2Bottom)));
  } else if (test_partition.GetAlignmentBytes() == 8) {
    ASSERT_OK(test_partition.Write(
        0, kKvsTestDataAligned8Top, sizeof(kKvsTestDataAligned8Top)));
    ASSERT_OK(test_partition.Write(kvs_attr.SectorHeaderSize(),
                                   kKvsTestDataAligned8Bottom,
                                   sizeof(kKvsTestDataAligned8Bottom)));
  } else if (test_partition.GetAlignmentBytes() == 16) {
    ASSERT_OK(test_partition.Write(
        0, kKvsTestDataAligned16Top, sizeof(kKvsTestDataAligned16Top)));
    ASSERT_OK(test_partition.Write(kvs_attr.SectorHeaderSize(),
                                   kKvsTestDataAligned16Bottom,
                                   sizeof(kKvsTestDataAligned16Bottom)));
  } else {
    LOG_ERROR("Test only supports 1, 2, 8 and 16 byte alignments.");
    ASSERT_OK(false);
  }

  EXPECT_EQ(kvs_local_.Enable(), Status::OK);
  EXPECT_TRUE(kvs_local_.IsEnabled());

  EXPECT_EQ(kvs_local_.Get(keys[0], buffer.data(), 1), Status::DATA_LOSS);

  // Value with correct CRC should still be available.
  uint32_t test2 = 0;
  ASSERT_OK(kvs_local_.Get(
      keys[1], reinterpret_cast<uint8_t*>(&test2), sizeof(test2)));
  EXPECT_EQ(kTestPattern, test2);

  // Test rewriting over corrupted data.
  ASSERT_OK(kvs_local_.Put(keys[0], kTestPattern));
  test2 = 0;
  EXPECT_OK(kvs_local_.Get(keys[0], &test2));
  EXPECT_EQ(kTestPattern, test2);

  // Check correct when re-enabled
  kvs_local_.Disable();
  EXPECT_EQ(kvs_local_.Enable(), Status::OK);
  test2 = 0;
  EXPECT_OK(kvs_local_.Get(keys[0], &test2));
  EXPECT_EQ(kTestPattern, test2);
}

TEST_F(KeyValueStoreTest, TestVersion2) {
  static constexpr uint32_t kTestPattern = 0xBAD0301f;
  // Since this test is not run on encypted flash, we can write the clean
  // pending flag for just this test.
  static constexpr uint8_t kKvsTestDataAligned1[] = {
      0xCD, 0xAB, 0x02, 0x00, 0x00, 0x00, 0xFF, 0xFF,  // Sector Header
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // Clean pending flag
      0xAA, 0x55, 0xB5, 0x87, 0x00, 0x00, 0x44, 0x00,  // Header (GOOD CRC)
      0x4B, 0x65, 0x79, 0x32,                          // Key (keys[1])
      0x1F, 0x30, 0xD0, 0xBA};                         // Value

  if (test_partition.GetAlignmentBytes() == 1) {
    // Test only runs on 1 byte alignment partitions
    test_partition.Erase(0, test_partition.GetSectorCount());
    test_partition.Write(0, kKvsTestDataAligned1, sizeof(kKvsTestDataAligned1));
    EXPECT_OK(kvs_local_.Enable());
    uint32_t test2 = 0;
    ASSERT_OK(kvs_local_.Get(
        keys[1], reinterpret_cast<uint8_t*>(&test2), sizeof(test2)));
    EXPECT_EQ(kTestPattern, test2);
  }
}

TEST_F(KeyValueStoreTest, ReEnable) {
  test_partition.Erase(0, test_partition.GetSectorCount());

  // Reset KVS
  kvs.Disable();
  ASSERT_OK(kvs.Enable());
  kvs.Disable();

  EXPECT_OK(kvs_local_.Enable());
  // Write value
  const uint8_t kValue = 0xDA;
  ASSERT_OK(kvs_local_.Put(keys[0], &kValue, sizeof(kValue)));
  uint8_t value;
  ASSERT_OK(kvs_local_.Get(
      keys[0], reinterpret_cast<uint8_t*>(&value), sizeof(value)));

  // Verify
  EXPECT_EQ(kValue, value);
}

TEST_F(KeyValueStoreTest, Erase) {
  test_partition.Erase(0, test_partition.GetSectorCount());

  // Reset KVS
  kvs.Disable();
  ASSERT_OK(kvs.Enable());

  // Write value
  const uint8_t kValue = 0xDA;
  ASSERT_OK(kvs.Put(keys[0], &kValue, sizeof(kValue)));

  ASSERT_OK(kvs.Erase(keys[0]));
  uint8_t value[1];
  ASSERT_EQ(kvs.Get(keys[0], value, sizeof(value)), Status::NOT_FOUND);

  // Reset KVS, ensure captured at enable
  kvs.Disable();
  ASSERT_OK(kvs.Enable());

  ASSERT_EQ(kvs.Get(keys[0], value, sizeof(value)), Status::NOT_FOUND);
}

TEST_F(KeyValueStoreTest, TemplatedPutAndGet) {
  test_partition.Erase(0, test_partition.GetSectorCount());
  // Reset KVS
  kvs.Disable();
  ASSERT_OK(kvs.Enable());
  // Store a value with the convenience method.
  const uint32_t kValue = 0x12345678;
  ASSERT_OK(kvs.Put(keys[0], kValue));

  // Read it back with the other convenience method.
  uint32_t value;
  ASSERT_OK(kvs.Get(keys[0], &value));
  ASSERT_EQ(kValue, value);

  // Make sure we cannot get something where size isn't what we expect
  const uint8_t kSmallValue = 0xBA;
  uint8_t small_value = kSmallValue;
  ASSERT_EQ(kvs.Get(keys[0], &small_value), Status::INVALID_ARGUMENT);
  ASSERT_EQ(small_value, kSmallValue);
}

TEST_F(KeyValueStoreTest, SameValueRewrite) {
  static constexpr uint32_t kTestPattern = 0xBAD0301f;
  // clang-format off
  static constexpr uint8_t kKvsTestDataAligned1Top[] = {
      0xCD, 0xAB, 0x02, 0x00, 0x00, 0x00, 0xFF, 0xFF,  // Sector Header
  };
  static constexpr uint8_t kKvsTestDataAligned1Bottom[] = {
      0xAA, 0x55, 0xB5, 0x87, 0x00, 0x00, 0x44, 0x00,  // Header (GOOD CRC)
      0x4B, 0x65, 0x79, 0x32,                          // Key (keys[1])
      0x1F, 0x30, 0xD0, 0xBA};                         // Value
  static constexpr uint8_t kKvsTestDataAligned2Top[] = {
      0xCD, 0xAB, 0x03, 0x00, 0x02, 0x00, 0xFF, 0xFF,  // Sector Header
  };
  static constexpr uint8_t kKvsTestDataAligned2Bottom[] = {
      0xAA, 0x55, 0xB5, 0x87, 0x00, 0x00, 0x44, 0x00,  // Header (GOOD CRC)
      0x4B, 0x65, 0x79, 0x32,                          // Key (keys[1])
      0x1F, 0x30, 0xD0, 0xBA};                         // Value
  static constexpr uint8_t kKvsTestDataAligned8Top[] = {
      0xCD, 0xAB, 0x03, 0x00, 0x08, 0x00, 0xFF, 0xFF,  // Sector Header
  };
  static constexpr uint8_t kKvsTestDataAligned8Bottom[] = {
      0xAA, 0x55, 0xB5, 0x87, 0x00, 0x00, 0x44, 0x00,  // header (GOOD CRC)
      0x4B, 0x65, 0x79, 0x32, 0x00, 0x00, 0x00, 0x00,  // Key (keys[1])
      0x1F, 0x30, 0xD0, 0xBA, 0x00, 0x00, 0x00, 0x00,  // Value + padding
  };
  static constexpr uint8_t kKvsTestDataAligned16Top[] = {
      0xCD, 0xAB, 0x03, 0x00, 0x10, 0x00, 0xFF, 0xFF,  // Sector Header
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Alignment to 16
  };
  static constexpr uint8_t kKvsTestDataAligned16Bottom[] = {
      0xAA, 0x55, 0xB5, 0x87, 0x00, 0x00, 0x44, 0x00,  // header (GOOD CRC)
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Alignment to 16
      0x4B, 0x65, 0x79, 0x32, 0x00, 0x00, 0x00, 0x00,  // Key (keys[1])
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Alignment to 16
      0x1F, 0x30, 0xD0, 0xBA, 0x00, 0x00, 0x00, 0x00,  // Value + padding
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Alignment to 16
  };
  // clang-format on

  ASSERT_OK(test_partition.Erase(0, test_partition.GetSectorCount()));
  // We don't actually care about the size values provided, since we are only
  // using kvs_attr to get Sector Size
  KvsAttributes kvs_attr(8, 8);
  pw::FlashPartition::Address address = kvs_attr.SectorHeaderSize();
  if (test_partition.GetAlignmentBytes() == 1) {
    ASSERT_OK(test_partition.Write(
        0, kKvsTestDataAligned1Top, sizeof(kKvsTestDataAligned1Top)));
    ASSERT_OK(test_partition.Write(address,
                                   kKvsTestDataAligned1Bottom,
                                   sizeof(kKvsTestDataAligned1Bottom)));
    address += sizeof(kKvsTestDataAligned1Bottom);
  } else if (test_partition.GetAlignmentBytes() == 2) {
    ASSERT_OK(test_partition.Write(
        0, kKvsTestDataAligned2Top, sizeof(kKvsTestDataAligned2Top)));
    ASSERT_OK(test_partition.Write(address,
                                   kKvsTestDataAligned2Bottom,
                                   sizeof(kKvsTestDataAligned2Bottom)));
    address += sizeof(kKvsTestDataAligned2Bottom);
  } else if (test_partition.GetAlignmentBytes() == 8) {
    ASSERT_OK(test_partition.Write(
        0, kKvsTestDataAligned8Top, sizeof(kKvsTestDataAligned8Top)));
    ASSERT_OK(test_partition.Write(address,
                                   kKvsTestDataAligned8Bottom,
                                   sizeof(kKvsTestDataAligned8Bottom)));
    address += sizeof(kKvsTestDataAligned8Bottom);
  } else if (test_partition.GetAlignmentBytes() == 16) {
    ASSERT_OK(test_partition.Write(
        0, kKvsTestDataAligned16Top, sizeof(kKvsTestDataAligned16Top)));
    ASSERT_OK(test_partition.Write(address,
                                   kKvsTestDataAligned16Bottom,
                                   sizeof(kKvsTestDataAligned16Bottom)));
    address += sizeof(kKvsTestDataAligned16Bottom);
  } else {
    LOG_ERROR("Test only supports 1, 2, 8 and 16 byte alignments.");
    ASSERT_EQ(true, false);
  }

  ASSERT_OK(kvs_local_.Enable());
  EXPECT_TRUE(kvs_local_.IsEnabled());

  // Put in same key/value pair
  ASSERT_OK(kvs_local_.Put(keys[1], kTestPattern));

  bool is_erased = false;
  ASSERT_OK(test_partition.IsChunkErased(
      address, test_partition.GetAlignmentBytes(), &is_erased));
  EXPECT_EQ(is_erased, true);
}

// This test is derived from bug that was discovered. Testing this corner case
// relies on creating a new key-value just under the size that is left over in
// the sector.
TEST_F(KeyValueStoreTest, FillSector2) {
  if (test_partition.GetSectorCount() < 3) {
    LOG_INFO("Not enough sectors, skipping test.");
    return;  // need at least 3 sectors
  }

  // Reset KVS
  kvs.Disable();
  test_partition.Erase(0, test_partition.GetSectorCount());
  ASSERT_OK(kvs.Enable());

  // Start of by filling flash sector to near full
  constexpr int kHalfBufferSize = buffer.size() / 2;
  const int kSizeToFill = test_partition.GetSectorSizeBytes() - kHalfBufferSize;
  constexpr size_t kTestDataSize = 8;
  KvsAttributes kvs_attr(std::strlen(keys[2]), kTestDataSize);

  FillKvs(keys[2], kSizeToFill);

  // Find out how much space is remaining for new key-value and confirm it
  // makes sense.
  size_t new_keyvalue_size = 0;
  size_t alignment = test_partition.GetAlignmentBytes();
  // Starts on second sector since it will try to keep first sector free
  pw::FlashPartition::Address read_address =
      2 * test_partition.GetSectorSizeBytes() - alignment;
  for (; read_address > 0; read_address -= alignment) {
    bool is_erased = false;
    ASSERT_OK(
        test_partition.IsChunkErased(read_address, alignment, &is_erased));
    if (is_erased) {
      new_keyvalue_size += alignment;
    } else {
      break;
    }
  }

  size_t expected_remaining = test_partition.GetSectorSizeBytes() -
                              kvs_attr.SectorHeaderSize() - kSizeToFill;
  ASSERT_EQ(new_keyvalue_size, expected_remaining);

  const char* kNewKey = "NewKey";
  constexpr size_t kValueLessThanChunkHeaderSize = 2;
  constexpr uint8_t kTestPattern = 0xBA;
  new_keyvalue_size -= kValueLessThanChunkHeaderSize;
  memset(buffer.data(), kTestPattern, new_keyvalue_size);
  ASSERT_OK(kvs.Put(kNewKey, buffer.data(), new_keyvalue_size));

  // In failed corner case, adding new key is deceptively successful. It isn't
  // until KVS is disabled and reenabled that issue can be detected.
  kvs.Disable();
  ASSERT_OK(kvs.Enable());

  // Might as well check that new key-value is what we expect it to be
  ASSERT_OK(kvs.Get(kNewKey, buffer.data(), new_keyvalue_size));
  for (size_t i = 0; i < new_keyvalue_size; i++) {
    EXPECT_EQ(buffer[i], kTestPattern);
  }
}

TEST_F(KeyValueStoreTest, GetValueSizeTests) {
  constexpr uint16_t kSizeOfValueToFill = 20U;
  constexpr uint8_t kKey0Pattern = 0xBA;
  uint16_t value_size = 0;
  // Start off with disabled KVS
  kvs.Disable();

  // Try getting value when KVS is disabled, expect failure
  EXPECT_EQ(kvs.GetValueSize(keys[0], &value_size),
            Status::FAILED_PRECONDITION);

  // Reset KVS
  test_partition.Erase(0, test_partition.GetSectorCount());
  ASSERT_OK(kvs.Enable());

  // Try some case that are expected to fail
  ASSERT_EQ(kvs.GetValueSize(keys[0], &value_size), Status::NOT_FOUND);
  ASSERT_EQ(kvs.GetValueSize(nullptr, &value_size), Status::INVALID_ARGUMENT);
  ASSERT_EQ(kvs.GetValueSize(keys[0], nullptr), Status::INVALID_ARGUMENT);

  // Add key[0] and test we get the right value size for it.
  memset(buffer.data(), kKey0Pattern, kSizeOfValueToFill);
  ASSERT_OK(kvs.Put(keys[0], buffer.data(), kSizeOfValueToFill));
  ASSERT_OK(kvs.GetValueSize(keys[0], &value_size));
  ASSERT_EQ(value_size, kSizeOfValueToFill);

  // Verify after erase key is not found
  ASSERT_OK(kvs.Erase(keys[0]));
  ASSERT_EQ(kvs.GetValueSize(keys[0], &value_size), Status::NOT_FOUND);
}

TEST_F(KeyValueStoreTest, CanFitEntryTests) {
  // Reset KVS
  kvs.Disable();
  test_partition.Erase(0, test_partition.GetSectorCount());
  ASSERT_OK(kvs.Enable());

  // Get exactly the number of bytes that can fit in the space remaining for
  // a large value, accounting for alignment.
  constexpr uint16_t kTestKeySize = 2;
  size_t space_remaining =
      test_partition.GetSectorSizeBytes()                //
      - RoundUpForAlignment(KeyValueStore::kHeaderSize)  // Sector Header
      - RoundUpForAlignment(KeyValueStore::kHeaderSize)  // Cleaning Header
      - RoundUpForAlignment(KeyValueStore::kHeaderSize)  // Chunk Header
      - RoundUpForAlignment(kTestKeySize);
  space_remaining -= test_partition.GetAlignmentBytes() / 2;
  space_remaining = RoundUpForAlignment(space_remaining);

  EXPECT_TRUE(kvs.CanFitEntry(kTestKeySize, space_remaining));
  EXPECT_FALSE(kvs.CanFitEntry(kTestKeySize, space_remaining + 1));
}

TEST_F(KeyValueStoreTest, DifferentValueSameCrc16) {
  test_partition.Erase(0, test_partition.GetSectorCount());
  const char kKey[] = "k";
  // With the key and our CRC16 algorithm these both have CRC of 0x82AE
  // Given they are the same size and same key, the KVS will need to check
  // the actual bits to know they are different.
  const char kValue1[] = {'d', 'a', 't'};
  const char kValue2[] = {'u', 'c', 'd'};

  // Verify the CRC matches
  ASSERT_EQ(CalcKvsCrc(kKey, kValue1, sizeof(kValue1)),
            CalcKvsCrc(kKey, kValue2, sizeof(kValue2)));

  // Reset KVS
  kvs.Disable();
  ASSERT_OK(kvs.Enable());
  ASSERT_OK(kvs.Put(kKey, kValue1));

  // Now try to rewrite with the similar value.
  ASSERT_OK(kvs.Put(kKey, kValue2));

  // Read it back and check it is correct
  char value[3];
  ASSERT_OK(kvs.Get(kKey, value, sizeof(value)));
  ASSERT_EQ(memcmp(value, kValue2, sizeof(value)), 0);
}

TEST_F(KeyValueStoreTest, CallingEraseTwice) {
  test_partition.Erase(0, test_partition.GetSectorCount());

  // Reset KVS
  kvs.Disable();
  ASSERT_OK(kvs.Enable());

  const uint8_t kValue = 0xDA;
  ASSERT_OK(kvs.Put(keys[0], &kValue, sizeof(kValue)));
  ASSERT_OK(kvs.Erase(keys[0]));
  uint16_t crc = CalcTestPartitionCrc();
  EXPECT_EQ(kvs.Erase(keys[0]), Status::NOT_FOUND);
  // Verify the flash has not changed
  EXPECT_EQ(crc, CalcTestPartitionCrc());
}

void __attribute__((noinline)) StackHeavyPartialClean() {
  CHECK_GE(test_partition.GetSectorCount(), 2);
  FlashSubPartition test_partition_sector1(&test_partition, 0, 1);
  FlashSubPartition test_partition_sector2(&test_partition, 1, 1);

  KeyValueStore kvs1(&test_partition_sector1);
  KeyValueStore kvs2(&test_partition_sector2);

  test_partition.Erase(0, test_partition.GetSectorCount());

  ASSERT_OK(kvs1.Enable());
  ASSERT_OK(kvs2.Enable());

  int values1[3] = {100, 101, 102};
  ASSERT_OK(kvs1.Put(keys[0], values1[0]));
  ASSERT_OK(kvs1.Put(keys[1], values1[1]));
  ASSERT_OK(kvs1.Put(keys[2], values1[2]));

  int values2[3] = {200, 201, 202};
  ASSERT_OK(kvs2.Put(keys[0], values2[0]));
  ASSERT_OK(kvs2.Put(keys[1], values2[1]));
  ASSERT_OK(kvs2.Erase(keys[1]));

  kvs1.Disable();
  kvs2.Disable();

  // Key 0 is value1 in first sector, value 2 in second
  // Key 1 is value1 in first sector, erased in second
  // key 2 is only in first sector

  uint64_t mark_clean_count = 5;
  ASSERT_OK(PaddedWrite(&test_partition_sector1,
                        RoundUpForAlignment(KeyValueStore::kHeaderSize),
                        reinterpret_cast<uint8_t*>(&mark_clean_count),
                        sizeof(uint64_t)));

  // Reset KVS
  kvs.Disable();
  ASSERT_OK(kvs.Enable());
  int value;
  ASSERT_OK(kvs.Get(keys[0], &value));
  ASSERT_EQ(values2[0], value);
  ASSERT_EQ(kvs.Get(keys[1], &value), Status::NOT_FOUND);
  ASSERT_OK(kvs.Get(keys[2], &value));
  ASSERT_EQ(values1[2], value);

  if (test_partition.GetSectorCount() == 2) {
    EXPECT_EQ(kvs.PendingCleanCount(), 0u);
    // Has forced a clean, mark again for next test
    return;  // Not enough sectors to test 2 partial cleans.
  } else {
    EXPECT_EQ(kvs.PendingCleanCount(), 1u);
  }

  mark_clean_count--;
  ASSERT_OK(PaddedWrite(&test_partition_sector2,
                        RoundUpForAlignment(KeyValueStore::kHeaderSize),
                        reinterpret_cast<uint8_t*>(&mark_clean_count),
                        sizeof(uint64_t)));
  // Reset KVS
  kvs.Disable();
  ASSERT_OK(kvs.Enable());
  EXPECT_EQ(kvs.PendingCleanCount(), 2u);
  ASSERT_OK(kvs.Get(keys[0], &value));
  ASSERT_EQ(values1[0], value);
  ASSERT_OK(kvs.Get(keys[1], &value));
  ASSERT_EQ(values1[1], value);
  ASSERT_OK(kvs.Get(keys[2], &value));
  ASSERT_EQ(values1[2], value);
}

TEST_F(KeyValueStoreTest, PartialClean) {
  if (CurrentTaskStackFree() < sizeof(KeyValueStore) * 2) {
    LOG_ERROR("Not enough stack for test, skipping");
    return;
  }
  StackHeavyPartialClean();
}

void __attribute__((noinline)) StackHeavyCleanAll() {
  CHECK_GE(test_partition.GetSectorCount(), 2);
  FlashSubPartition test_partition_sector1(&test_partition, 0, 1);

  KeyValueStore kvs1(&test_partition_sector1);
  test_partition.Erase(0, test_partition.GetSectorCount());

  ASSERT_OK(kvs1.Enable());

  int values1[3] = {100, 101, 102};
  ASSERT_OK(kvs1.Put(keys[0], values1[0]));
  ASSERT_OK(kvs1.Put(keys[1], values1[1]));
  ASSERT_OK(kvs1.Put(keys[2], values1[2] - 100));  //  force a rewrite
  ASSERT_OK(kvs1.Put(keys[2], values1[2]));

  kvs1.Disable();

  uint64_t mark_clean_count = 5;
  ASSERT_OK(PaddedWrite(&test_partition_sector1,
                        RoundUpForAlignment(KeyValueStore::kHeaderSize),
                        reinterpret_cast<uint8_t*>(&mark_clean_count),
                        sizeof(uint64_t)));

  // Reset KVS
  kvs.Disable();
  ASSERT_OK(kvs.Enable());
  int value;
  EXPECT_EQ(kvs.PendingCleanCount(), 1u);
  ASSERT_OK(kvs.CleanAll());
  EXPECT_EQ(kvs.PendingCleanCount(), 0u);
  ASSERT_OK(kvs.Get(keys[0], &value));
  ASSERT_EQ(values1[0], value);
  ASSERT_OK(kvs.Get(keys[1], &value));
  ASSERT_EQ(values1[1], value);
  ASSERT_OK(kvs.Get(keys[2], &value));
  ASSERT_EQ(values1[2], value);
}

TEST_F(KeyValueStoreTest, CleanAll) {
  if (CurrentTaskStackFree() < sizeof(KeyValueStore) * 1) {
    LOG_ERROR("Not enough stack for test, skipping");
    return;
  }
  StackHeavyCleanAll();
}

void __attribute__((noinline)) StackHeavyPartialCleanLargeCounts() {
  CHECK_GE(test_partition.GetSectorCount(), 2);
  FlashSubPartition test_partition_sector1(&test_partition, 0, 1);
  FlashSubPartition test_partition_sector2(&test_partition, 1, 1);

  KeyValueStore kvs1(&test_partition_sector1);
  KeyValueStore kvs2(&test_partition_sector2);

  test_partition.Erase(0, test_partition.GetSectorCount());

  ASSERT_OK(kvs1.Enable());
  ASSERT_OK(kvs2.Enable());

  int values1[3] = {100, 101, 102};
  ASSERT_OK(kvs1.Put(keys[0], values1[0]));
  ASSERT_OK(kvs1.Put(keys[1], values1[1]));
  ASSERT_OK(kvs1.Put(keys[2], values1[2]));

  int values2[3] = {200, 201, 202};
  ASSERT_OK(kvs2.Put(keys[0], values2[0]));
  ASSERT_OK(kvs2.Put(keys[1], values2[1]));
  ASSERT_OK(kvs2.Erase(keys[1]));

  kvs1.Disable();
  kvs2.Disable();
  kvs.Disable();

  // Key 0 is value1 in first sector, value 2 in second
  // Key 1 is value1 in first sector, erased in second
  // key 2 is only in first sector

  uint64_t mark_clean_count = 4569877515;
  ASSERT_OK(PaddedWrite(&test_partition_sector1,
                        RoundUpForAlignment(KeyValueStore::kHeaderSize),
                        reinterpret_cast<uint8_t*>(&mark_clean_count),
                        sizeof(uint64_t)));

  // Reset KVS
  ASSERT_OK(kvs.Enable());
  int value;
  ASSERT_OK(kvs.Get(keys[0], &value));
  ASSERT_EQ(values2[0], value);
  ASSERT_EQ(kvs.Get(keys[1], &value), Status::NOT_FOUND);
  ASSERT_OK(kvs.Get(keys[2], &value));
  ASSERT_EQ(values1[2], value);

  if (test_partition.GetSectorCount() == 2) {
    EXPECT_EQ(kvs.PendingCleanCount(), 0u);
    // Has forced a clean, mark again for next test
    // Has forced a clean, mark again for next test
    return;  // Not enough sectors to test 2 partial cleans.
  } else {
    EXPECT_EQ(kvs.PendingCleanCount(), 1u);
  }
  kvs.Disable();

  mark_clean_count--;
  ASSERT_OK(PaddedWrite(&test_partition_sector2,
                        RoundUpForAlignment(KeyValueStore::kHeaderSize),
                        reinterpret_cast<uint8_t*>(&mark_clean_count),
                        sizeof(uint64_t)));
  // Reset KVS
  ASSERT_OK(kvs.Enable());
  EXPECT_EQ(kvs.PendingCleanCount(), 2u);
  ASSERT_OK(kvs.Get(keys[0], &value));
  ASSERT_EQ(values1[0], value);
  ASSERT_OK(kvs.Get(keys[1], &value));
  ASSERT_EQ(values1[1], value);
  ASSERT_OK(kvs.Get(keys[2], &value));
  ASSERT_EQ(values1[2], value);
}

TEST_F(KeyValueStoreTest, PartialCleanLargeCounts) {
  if (CurrentTaskStackFree() < sizeof(KeyValueStore) * 2) {
    LOG_ERROR("Not enough stack for test, skipping");
    return;
  }
  StackHeavyPartialCleanLargeCounts();
}

void __attribute__((noinline)) StackHeavyRecoverNoFreeSectors() {
  CHECK_GE(test_partition.GetSectorCount(), 2);
  FlashSubPartition test_partition_sector1(&test_partition, 0, 1);
  FlashSubPartition test_partition_sector2(&test_partition, 1, 1);
  FlashSubPartition test_partition_both(&test_partition, 0, 2);

  KeyValueStore kvs1(&test_partition_sector1);
  KeyValueStore kvs2(&test_partition_sector2);
  KeyValueStore kvs_both(&test_partition_both);

  test_partition.Erase(0, test_partition.GetSectorCount());

  ASSERT_OK(kvs1.Enable());
  ASSERT_OK(kvs2.Enable());

  int values[3] = {100, 101};
  ASSERT_OK(kvs1.Put(keys[0], values[0]));
  ASSERT_FALSE(kvs1.HasEmptySector());
  ASSERT_OK(kvs2.Put(keys[1], values[1]));
  ASSERT_FALSE(kvs2.HasEmptySector());

  kvs1.Disable();
  kvs2.Disable();

  // Reset KVS
  ASSERT_OK(kvs_both.Enable());
  ASSERT_TRUE(kvs_both.HasEmptySector());
  int value;
  ASSERT_OK(kvs_both.Get(keys[0], &value));
  ASSERT_EQ(values[0], value);
  ASSERT_OK(kvs_both.Get(keys[1], &value));
  ASSERT_EQ(values[1], value);
}

TEST_F(KeyValueStoreTest, RecoverNoFreeSectors) {
  if (CurrentTaskStackFree() < sizeof(KeyValueStore) * 3) {
    LOG_ERROR("Not enough stack for test, skipping");
    return;
  }
  StackHeavyRecoverNoFreeSectors();
}

void __attribute__((noinline)) StackHeavyCleanOneSector() {
  CHECK_GE(test_partition.GetSectorCount(), 2);
  FlashSubPartition test_partition_sector1(&test_partition, 0, 1);
  FlashSubPartition test_partition_sector2(&test_partition, 1, 1);

  KeyValueStore kvs1(&test_partition_sector1);

  test_partition.Erase(0, test_partition.GetSectorCount());

  ASSERT_OK(kvs1.Enable());

  int values[3] = {100, 101, 102};
  ASSERT_OK(kvs1.Put(keys[0], values[0]));
  ASSERT_OK(kvs1.Put(keys[1], values[1]));
  ASSERT_OK(kvs1.Put(keys[2], values[2]));

  kvs1.Disable();
  kvs.Disable();

  uint64_t mark_clean_count = 1;
  ASSERT_OK(PaddedWrite(&test_partition_sector1,
                        RoundUpForAlignment(KeyValueStore::kHeaderSize),
                        reinterpret_cast<uint8_t*>(&mark_clean_count),
                        sizeof(uint64_t)));

  // Reset KVS
  ASSERT_OK(kvs.Enable());

  EXPECT_EQ(kvs.PendingCleanCount(), 1u);

  bool all_sectors_have_been_cleaned = false;
  ASSERT_OK(kvs.CleanOneSector(&all_sectors_have_been_cleaned));
  EXPECT_EQ(all_sectors_have_been_cleaned, true);
  EXPECT_EQ(kvs.PendingCleanCount(), 0u);
  ASSERT_OK(kvs.CleanOneSector(&all_sectors_have_been_cleaned));
  EXPECT_EQ(all_sectors_have_been_cleaned, true);
  int value;
  ASSERT_OK(kvs.Get(keys[0], &value));
  ASSERT_EQ(values[0], value);
  ASSERT_OK(kvs.Get(keys[1], &value));
  ASSERT_EQ(values[1], value);
  ASSERT_OK(kvs.Get(keys[2], &value));
  ASSERT_EQ(values[2], value);
}

TEST_F(KeyValueStoreTest, CleanOneSector) {
  if (CurrentTaskStackFree() < sizeof(KeyValueStore)) {
    LOG_ERROR("Not enough stack for test, skipping");
    return;
  }
  StackHeavyCleanOneSector();
}

#if USE_MEMORY_BUFFER

TEST_F(KeyValueStoreTest, LargePartition) {
  if (CurrentTaskStackFree() < sizeof(KeyValueStore)) {
    LOG_ERROR("Not enough stack for test, skipping");
    return;
  }
  large_test_partition.Erase(0, large_test_partition.GetSectorCount());
  KeyValueStore large_kvs(&large_test_partition);
  // Reset KVS
  large_kvs.Disable();
  ASSERT_OK(large_kvs.Enable());

  const uint8_t kValue1 = 0xDA;
  const uint8_t kValue2 = 0x12;
  uint8_t value[1];
  ASSERT_OK(large_kvs.Put(keys[0], &kValue1, sizeof(kValue1)));
  EXPECT_EQ(large_kvs.KeyCount(), 1);
  ASSERT_OK(large_kvs.Erase(keys[0]));
  EXPECT_EQ(large_kvs.Get(keys[0], value, sizeof(value)), Status::NOT_FOUND);
  ASSERT_OK(large_kvs.Put(keys[1], &kValue1, sizeof(kValue1)));
  ASSERT_OK(large_kvs.Put(keys[2], &kValue2, sizeof(kValue2)));
  ASSERT_OK(large_kvs.Erase(keys[1]));
  EXPECT_OK(large_kvs.Get(keys[2], value, sizeof(value)));
  EXPECT_EQ(kValue2, value[0]);
  ASSERT_EQ(large_kvs.Get(keys[1], &value), Status::NOT_FOUND);
  EXPECT_EQ(large_kvs.KeyCount(), 1);
}
#endif  // USE_MEMORY_BUFFER

}  // namespace pw
