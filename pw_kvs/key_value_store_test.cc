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

#include <array>
#include <cstdio>
#if defined(__linux__)
#include <vector>
#endif  // defined(__linux__)
#include <cstring>
#include <type_traits>

#include "pw_span/span.h"

#define PW_LOG_USE_ULTRA_SHORT_NAMES 1
#include "pw_log/log.h"

#define USE_MEMORY_BUFFER 1

#include "gtest/gtest.h"
#include "pw_checksum/ccitt_crc16.h"
#include "pw_kvs/crc16_checksum.h"
#include "pw_kvs/flash_memory.h"
#include "pw_kvs_private/format.h"
#include "pw_kvs_private/macros.h"
#include "pw_log/log.h"
#include "pw_status/status.h"
#include "pw_string/string_builder.h"

#if USE_MEMORY_BUFFER
#include "pw_kvs/in_memory_fake_flash.h"
#endif  // USE_MEMORY_BUFFER

namespace pw::kvs {
namespace {

using std::byte;

template <typename... Args>
constexpr auto ByteArray(Args... args) {
  return std::array<byte, sizeof...(args)>{static_cast<byte>(args)...};
}

// Test that the ConvertsToSpan trait correctly idenitifies types that convert
// to span.
static_assert(!ConvertsToSpan<int>());
static_assert(!ConvertsToSpan<void>());
static_assert(!ConvertsToSpan<std::byte>());
static_assert(!ConvertsToSpan<std::byte*>());

static_assert(ConvertsToSpan<std::array<int, 5>>());
static_assert(ConvertsToSpan<decltype("Hello!")>());

static_assert(ConvertsToSpan<std::string_view>());
static_assert(ConvertsToSpan<std::string_view&>());
static_assert(ConvertsToSpan<std::string_view&&>());

static_assert(ConvertsToSpan<const std::string_view>());
static_assert(ConvertsToSpan<const std::string_view&>());
static_assert(ConvertsToSpan<const std::string_view&&>());

static_assert(ConvertsToSpan<span<int>>());
static_assert(ConvertsToSpan<span<byte>>());
static_assert(ConvertsToSpan<span<const int*>>());
static_assert(ConvertsToSpan<span<bool>&&>());
static_assert(ConvertsToSpan<const span<bool>&>());
static_assert(ConvertsToSpan<span<bool>&&>());

// This is a self contained flash unit with both memory and a single partition.
template <uint32_t sector_size_bytes, uint16_t sector_count>
struct FlashWithPartitionFake {
  // Default to 16 byte alignment, which is common in practice.
  FlashWithPartitionFake() : FlashWithPartitionFake(16) {}
  FlashWithPartitionFake(size_t alignment_bytes)
      : memory(alignment_bytes), partition(&memory, 0, memory.sector_count()) {}

  InMemoryFakeFlash<sector_size_bytes, sector_count> memory;
  FlashPartition partition;

 public:
#if defined(__linux__)
  Status Dump(const char* filename) {
    std::FILE* out_file = std::fopen(filename, "w+");
    if (out_file == nullptr) {
      PW_LOG_ERROR("Failed to dump to %s", filename);
      return Status::DATA_LOSS;
    }
    std::vector<std::byte> out_vec(memory.size_bytes());
    Status status =
        memory.Read(0, pw::span<std::byte>(out_vec.data(), out_vec.size()));
    if (status != Status::OK) {
      fclose(out_file);
      return status;
    }

    size_t written =
        std::fwrite(out_vec.data(), 1, memory.size_bytes(), out_file);
    if (written != memory.size_bytes()) {
      PW_LOG_ERROR("Failed to dump to %s, written=%u",
                   filename,
                   static_cast<unsigned>(written));
      status = Status::DATA_LOSS;
    } else {
      PW_LOG_INFO("Dumped to %s", filename);
      status = Status::OK;
    }

    fclose(out_file);
    return status;
  }
#else
  Status Dump(const char* filename) {
    (void)(filename);
    return Status::OK;
  }
#endif  // defined(__linux__)
};

typedef FlashWithPartitionFake<4 * 128 /*sector size*/, 6 /*sectors*/> Flash;

#if USE_MEMORY_BUFFER
// Although it might be useful to test other configurations, some tests require
// at least 3 sectors; therfore it should have this when checked in.
InMemoryFakeFlash<4 * 1024, 4> test_flash(
    16);  // 4 x 4k sectors, 16 byte alignment
FlashPartition test_partition(&test_flash, 0, test_flash.sector_count());
InMemoryFakeFlash<1024, 60> large_test_flash(8);
FlashPartition large_test_partition(&large_test_flash,
                                    0,
                                    large_test_flash.sector_count());
#else   // TODO: Test with real flash
FlashPartition& test_partition = FlashExternalTestPartition();
#endif  // USE_MEMORY_BUFFER

ChecksumCrc16 checksum;
constexpr EntryHeaderFormat format{.magic = 0xBAD'C0D3, .checksum = &checksum};

KeyValueStore kvs(&test_partition, format);

// Use test fixture for logging support
class KeyValueStoreTest : public ::testing::Test {
 protected:
  KeyValueStoreTest() : kvs_local_(&test_partition, format) {}

  KeyValueStore kvs_local_;
};

std::array<byte, 512> buffer;
constexpr std::array<const char*, 3> keys{"TestKey1", "Key2", "TestKey3"};

Status PaddedWrite(FlashPartition* partition,
                   FlashPartition::Address address,
                   const std::byte* buf,
                   size_t size) {
  static constexpr size_t kMaxAlignmentBytes = 128;
  byte alignment_buffer[kMaxAlignmentBytes] = {};

  size_t aligned_bytes = size - (size % partition->alignment_bytes());
  TRY(partition->Write(address, span(buf, aligned_bytes)));

  uint16_t remaining_bytes = size - aligned_bytes;
  if (remaining_bytes > 0) {
    std::memcpy(alignment_buffer, &buf[aligned_bytes], remaining_bytes);
    if (Status status = partition->Write(
            address + aligned_bytes,
            span(alignment_buffer, partition->alignment_bytes()));
        !status.ok()) {
      return status;
    }
  }
  return Status::OK;
}

size_t RoundUpForAlignment(size_t size) {
  // TODO: THIS IS SO PADDEDWRITE APPEARS USED
  (void)PaddedWrite;

  uint16_t alignment = test_partition.alignment_bytes();
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
            RoundUpForAlignment(sizeof(EntryHeader))),  // TODO: not correct
        sector_header_clean_size_(
            RoundUpForAlignment(sizeof(EntryHeader))),  // TODO: not correct
        chunk_header_size_(RoundUpForAlignment(sizeof(EntryHeader))),
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

  ASSERT_GE(size_to_fill, kvs_attr.MinPutSize() + kvs_attr.EraseSize());

  // Saving enough space to perform erase after loop
  size_to_fill -= kvs_attr.EraseSize();
  // We start with possible small chunk to prevent too small of a Kvs.Put() at
  // the end.
  size_t chunk_len =
      std::max(kvs_attr.MinPutSize(), size_to_fill % buffer.size());
  std::memset(buffer.data(), 0, buffer.size());
  while (size_to_fill > 0) {
    // Changing buffer value so put actually does something
    buffer[0] = static_cast<byte>(static_cast<uint8_t>(buffer[0]) + 1);
    ASSERT_EQ(Status::OK,
              kvs.Put(key,
                      span(buffer.data(),
                           chunk_len - kvs_attr.ChunkHeaderSize() -
                               kvs_attr.KeySize())));
    size_to_fill -= chunk_len;
    chunk_len = std::min(size_to_fill, kMaxPutSize);
  }
  ASSERT_EQ(Status::OK, kvs.Delete(key));
}

uint16_t CalcKvsCrc(const char* key, const void* data, size_t data_len) {
  uint16_t crc = checksum::CcittCrc16(as_bytes(span(key, std::strlen(key))));
  return checksum::CcittCrc16(span(static_cast<const byte*>(data), data_len),
                              crc);
}

uint16_t CalcTestPartitionCrc() {
  byte buf[16];  // Read as 16 byte chunks
  EXPECT_EQ(sizeof(buf) % test_partition.alignment_bytes(), 0u);
  EXPECT_EQ(test_partition.size_bytes() % sizeof(buf), 0u);
  uint16_t crc = checksum::kCcittCrc16DefaultInitialValue;
  for (size_t i = 0; i < test_partition.size_bytes(); i += sizeof(buf)) {
    test_partition.Read(i, buf);
    crc = checksum::CcittCrc16(as_bytes(span(buf)), crc);
  }
  return crc;
}

}  // namespace

TEST_F(KeyValueStoreTest, Iteration_Empty_ByReference) {
  kvs.Init();
  for (const KeyValueStore::Entry& entry : kvs) {
    FAIL();  // The KVS is empty; this shouldn't execute.
    static_cast<void>(entry);
  }
}

TEST_F(KeyValueStoreTest, Iteration_Empty_ByValue) {
  kvs.Init();
  for (KeyValueStore::Entry entry : kvs) {
    FAIL();  // The KVS is empty; this shouldn't execute.
    static_cast<void>(entry);
  }
}

TEST_F(KeyValueStoreTest, DISABLED_FuzzTest) {
  if (test_partition.sector_size_bytes() < 4 * 1024 ||
      test_partition.sector_count() < 4) {
    PW_LOG_INFO("Sectors too small, skipping test.");
    return;  // TODO: Test could be generalized
  }
  // Erase
  ASSERT_EQ(Status::OK, test_partition.Erase(0, test_partition.sector_count()));

  // Reset KVS
  ASSERT_EQ(Status::OK, kvs.Init());
  const char* key1 = "Buf1";
  const char* key2 = "Buf2";
  const size_t kLargestBufSize = 3 * 1024;
  static byte buf1[kLargestBufSize];
  static byte buf2[kLargestBufSize];
  std::memset(buf1, 1, sizeof(buf1));
  std::memset(buf2, 2, sizeof(buf2));

  // Start with things in KVS
  ASSERT_EQ(Status::OK, kvs.Put(key1, buf1));
  ASSERT_EQ(Status::OK, kvs.Put(key2, buf2));
  for (size_t j = 0; j < keys.size(); j++) {
    ASSERT_EQ(Status::OK, kvs.Put(keys[j], j));
  }

  for (size_t i = 0; i < 100; i++) {
    // Vary two sizes
    size_t size1 = (kLargestBufSize) / (i + 1);
    size_t size2 = (kLargestBufSize) / (100 - i);
    for (size_t j = 0; j < 50; j++) {
      // Rewrite a single key many times, can fill up a sector
      ASSERT_EQ(Status::OK, kvs.Put("some_data", j));
    }
    // Delete and re-add everything
    ASSERT_EQ(Status::OK, kvs.Delete(key1));
    ASSERT_EQ(Status::OK, kvs.Put(key1, span(buf1, size1)));
    ASSERT_EQ(Status::OK, kvs.Delete(key2));
    ASSERT_EQ(Status::OK, kvs.Put(key2, span(buf2, size2)));
    for (size_t j = 0; j < keys.size(); j++) {
      ASSERT_EQ(Status::OK, kvs.Delete(keys[j]));
      ASSERT_EQ(Status::OK, kvs.Put(keys[j], j));
    }

    // Re-enable and verify
    ASSERT_EQ(Status::OK, kvs.Init());
    static byte buf[4 * 1024];
    ASSERT_EQ(Status::OK, kvs.Get(key1, span(buf, size1)).status());
    ASSERT_EQ(std::memcmp(buf, buf1, size1), 0);
    ASSERT_EQ(Status::OK, kvs.Get(key2, span(buf, size2)).status());
    ASSERT_EQ(std::memcmp(buf2, buf2, size2), 0);
    for (size_t j = 0; j < keys.size(); j++) {
      size_t ret = 1000;
      ASSERT_EQ(Status::OK, kvs.Get(keys[j], &ret));
      ASSERT_EQ(ret, j);
    }
  }
}

TEST_F(KeyValueStoreTest, DISABLED_Basic) {
  // Erase
  ASSERT_EQ(Status::OK, test_partition.Erase(0, test_partition.sector_count()));

  // Reset KVS
  ASSERT_EQ(Status::OK, kvs.Init());

  // Add some data
  uint8_t value1 = 0xDA;
  ASSERT_EQ(Status::OK,
            kvs.Put(keys[0], as_bytes(span(&value1, sizeof(value1)))));

  uint32_t value2 = 0xBAD0301f;
  ASSERT_EQ(Status::OK, kvs.Put(keys[1], value2));

  // Verify data
  uint32_t test2;
  EXPECT_EQ(Status::OK, kvs.Get(keys[1], &test2));
  uint8_t test1;
  ASSERT_EQ(Status::OK, kvs.Get(keys[0], &test1));

  EXPECT_EQ(test1, value1);
  EXPECT_EQ(test2, value2);

  // Erase a key
  EXPECT_EQ(Status::OK, kvs.Delete(keys[0]));

  // Verify it was erased
  EXPECT_EQ(kvs.Get(keys[0], &test1), Status::NOT_FOUND);
  test2 = 0;
  ASSERT_EQ(
      Status::OK,
      kvs.Get(keys[1], span(reinterpret_cast<byte*>(&test2), sizeof(test2)))
          .status());
  EXPECT_EQ(test2, value2);

  // Erase other key
  kvs.Delete(keys[1]);

  // Verify it was erased
  EXPECT_EQ(kvs.size(), 0u);
}

#define ASSERT_OK(expr) ASSERT_EQ(Status::OK, expr)
#define EXPECT_OK(expr) EXPECT_EQ(Status::OK, expr)

#define AS_SIZE(x) static_cast<size_t>(x)

TEST(InMemoryKvs, DISABLED_WriteOneKeyMultipleTimes) {
  // Create and erase the fake flash. It will persist across reloads.
  Flash flash;
  ASSERT_OK(flash.partition.Erase());

  int num_reloads = 2;
  for (int reload = 0; reload < num_reloads; ++reload) {
    DBG("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");
    DBG("xxx                                      xxxx");
    DBG("xxx               Reload %2d              xxxx", reload);
    DBG("xxx                                      xxxx");
    DBG("xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx");

    // Create and initialize the KVS.
    constexpr EntryHeaderFormat format{.magic = 0xBAD'C0D3,
                                       .checksum = nullptr};
    KeyValueStore kvs(&flash.partition, format);
    ASSERT_OK(kvs.Init());

    // Write the same entry many times.
    const char* key = "abcd";
    const size_t num_writes = 1;  // TODO: Make this > 1 when things work.
    uint32_t written_value;
    EXPECT_EQ(kvs.size(), (reload == 0) ? 0 : 1u);
    for (uint32_t i = 0; i < num_writes; ++i) {
      INF("PUT #%zu for key %s with value %zu", AS_SIZE(i), key, AS_SIZE(i));

      written_value = i + 0xfc;  // Prevent accidental pass with zero.
      EXPECT_OK(kvs.Put(key, written_value));
      EXPECT_EQ(kvs.size(), 1u);
    }

    // Verify that we can read the value back.
    INF("GET final value for key: %s", key);
    uint32_t actual_value;
    EXPECT_OK(kvs.Get(key, &actual_value));
    EXPECT_EQ(actual_value, written_value);

    kvs.LogDebugInfo();

    char fname_buf[64] = {'\0'};
    snprintf(&fname_buf[0],
             sizeof(fname_buf),
             "WriteOneKeyMultipleTimes_%d.bin",
             reload);
    flash.Dump(fname_buf);
  }
}

TEST(InMemoryKvs, WritingMultipleKeysIncreasesSize) {
  // Create and erase the fake flash.
  Flash flash;
  ASSERT_OK(flash.partition.Erase());

  // Create and initialize the KVS.
  constexpr EntryHeaderFormat format{.magic = 0xBAD'C0D3, .checksum = nullptr};
  KeyValueStore kvs(&flash.partition, format);
  ASSERT_OK(kvs.Init());

  // Write the same entry many times.
  const size_t num_writes = 10;
  EXPECT_EQ(kvs.size(), 0u);
  for (size_t i = 0; i < num_writes; ++i) {
    StringBuffer<150> key;
    key << "key_" << i;
    INF("PUT #%zu for key %s with value %zu", i, key.c_str(), i);

    size_t value = i + 77;  // Prevent accidental pass with zero.
    EXPECT_OK(kvs.Put(key.view(), value));
    EXPECT_EQ(kvs.size(), i + 1);
  }
  kvs.LogDebugInfo();
  flash.Dump("WritingMultipleKeysIncreasesSize.bin");
}

TEST(InMemoryKvs, WriteAndReadOneKey) {
  // Create and erase the fake flash.
  Flash flash;
  ASSERT_OK(flash.partition.Erase());

  // Create and initialize the KVS.
  constexpr EntryHeaderFormat format{.magic = 0xBAD'C0D3, .checksum = nullptr};
  KeyValueStore kvs(&flash.partition, format);
  ASSERT_OK(kvs.Init());

  // Add two entries with different keys and values.
  const char* key = "Key1";
  INF("PUT value for key: %s", key);
  uint8_t written_value = 0xDA;
  ASSERT_OK(kvs.Put(key, written_value));
  EXPECT_EQ(kvs.size(), 1u);

  INF("GET value for key: %s", key);
  uint8_t actual_value;
  ASSERT_OK(kvs.Get(key, &actual_value));
  EXPECT_EQ(actual_value, written_value);

  EXPECT_EQ(kvs.size(), 1u);
}

TEST(InMemoryKvs, Basic) {
  const char* key1 = "Key1";
  const char* key2 = "Key2";

  // Create and erase the fake flash.
  Flash flash;
  ASSERT_EQ(Status::OK, flash.partition.Erase());

  // Create and initialize the KVS.
  constexpr EntryHeaderFormat format{.magic = 0xBAD'C0D3, .checksum = nullptr};
  KeyValueStore kvs(&flash.partition, format);
  ASSERT_OK(kvs.Init());

  // Add two entries with different keys and values.
  INF("PUT first value");
  uint8_t value1 = 0xDA;
  ASSERT_OK(kvs.Put(key1, as_bytes(span(&value1, sizeof(value1)))));
  EXPECT_EQ(kvs.size(), 1u);

  INF("PUT second value");
  uint32_t value2 = 0xBAD0301f;
  ASSERT_OK(kvs.Put(key2, value2));
  EXPECT_EQ(kvs.size(), 2u);

  INF("--------------------------------");
  INF("GET second value");
  // Verify data
  uint32_t test2;
  EXPECT_OK(kvs.Get(key2, &test2));

  INF("GET first value");
  uint8_t test1;
  ASSERT_OK(kvs.Get(key1, &test1));

  EXPECT_EQ(test1, value1);
  EXPECT_EQ(test2, value2);

  EXPECT_EQ(kvs.size(), 2u);
}

TEST_F(KeyValueStoreTest, DISABLED_MaxKeyLength) {
  // Erase
  ASSERT_EQ(Status::OK, test_partition.Erase(0, test_partition.sector_count()));

  // Reset KVS
  ASSERT_EQ(Status::OK, kvs.Init());

  // Add some data
  char key[16] = "123456789abcdef";  // key length 15 (without \0)
  int value = 1;
  ASSERT_EQ(Status::OK, kvs.Put(key, value));

  // Verify data
  int test = 0;
  ASSERT_EQ(Status::OK, kvs.Get(key, &test));
  EXPECT_EQ(test, value);

  // Erase a key
  kvs.Delete(key);

  // Verify it was erased
  EXPECT_EQ(kvs.Get(key, &test), Status::NOT_FOUND);
}

TEST_F(KeyValueStoreTest, DISABLED_LargeBuffers) {
  test_partition.Erase(0, test_partition.sector_count());

  // Note this assumes that no other keys larger then key0
  static_assert(sizeof(keys[0]) >= sizeof(keys[1]) &&
                sizeof(keys[0]) >= sizeof(keys[2]));
  KvsAttributes kvs_attr(std::strlen(keys[0]), buffer.size());

  // Verify the data will fit in this test partition. This checks that all the
  // keys chunks will fit and a header for each sector will fit. It requires 1
  // empty sector also.
  const size_t kAllChunkSize = kvs_attr.MinPutSize() * keys.size();
  const size_t kAllSectorHeaderSizes =
      kvs_attr.SectorHeaderSize() * (test_partition.sector_count() - 1);
  const size_t kMinSize = kAllChunkSize + kAllSectorHeaderSizes;
  const size_t kAvailSectorSpace =
      test_partition.sector_size_bytes() * (test_partition.sector_count() - 1);
  if (kAvailSectorSpace < kMinSize) {
    PW_LOG_INFO("KVS too small, skipping test.");
    return;
  }
  // Reset KVS
  ASSERT_EQ(Status::OK, kvs.Init());

  // Add and verify
  for (unsigned add_idx = 0; add_idx < keys.size(); add_idx++) {
    std::memset(buffer.data(), add_idx, buffer.size());
    ASSERT_EQ(Status::OK, kvs.Put(keys[add_idx], buffer));
    EXPECT_EQ(kvs.size(), add_idx + 1);
    for (unsigned verify_idx = 0; verify_idx <= add_idx; verify_idx++) {
      std::memset(buffer.data(), 0, buffer.size());
      ASSERT_EQ(Status::OK, kvs.Get(keys[verify_idx], buffer).status());
      for (unsigned i = 0; i < buffer.size(); i++) {
        EXPECT_EQ(static_cast<unsigned>(buffer[i]), verify_idx);
      }
    }
  }

  // Erase and verify
  for (unsigned erase_idx = 0; erase_idx < keys.size(); erase_idx++) {
    ASSERT_EQ(Status::OK, kvs.Delete(keys[erase_idx]));
    EXPECT_EQ(kvs.size(), keys.size() - erase_idx - 1);
    for (unsigned verify_idx = 0; verify_idx < keys.size(); verify_idx++) {
      std::memset(buffer.data(), 0, buffer.size());
      if (verify_idx <= erase_idx) {
        ASSERT_EQ(kvs.Get(keys[verify_idx], buffer).status(),
                  Status::NOT_FOUND);
      } else {
        ASSERT_EQ(Status::OK, kvs.Get(keys[verify_idx], buffer).status());
        for (uint32_t i = 0; i < buffer.size(); i++) {
          EXPECT_EQ(buffer[i], static_cast<byte>(verify_idx));
        }
      }
    }
  }
}

TEST_F(KeyValueStoreTest, DISABLED_Enable) {
  test_partition.Erase(0, test_partition.sector_count());

  KvsAttributes kvs_attr(std::strlen(keys[0]), buffer.size());

  // Verify the data will fit in this test partition. This checks that all the
  // keys chunks will fit and a header for each sector will fit. It requires 1
  // empty sector also.
  const size_t kAllChunkSize = kvs_attr.MinPutSize() * keys.size();
  const size_t kAllSectorHeaderSizes =
      kvs_attr.SectorHeaderSize() * (test_partition.sector_count() - 1);
  const size_t kMinSize = kAllChunkSize + kAllSectorHeaderSizes;
  const size_t kAvailSectorSpace =
      test_partition.sector_size_bytes() * (test_partition.sector_count() - 1);
  if (kAvailSectorSpace < kMinSize) {
    PW_LOG_INFO("KVS too small, skipping test.");
    return;
  }

  // Reset KVS
  ASSERT_EQ(Status::OK, kvs.Init());

  // Add some items
  for (unsigned add_idx = 0; add_idx < keys.size(); add_idx++) {
    std::memset(buffer.data(), add_idx, buffer.size());
    ASSERT_EQ(Status::OK, kvs.Put(keys[add_idx], buffer));
    EXPECT_EQ(kvs.size(), add_idx + 1);
  }

  // Enable different KVS which should be able to properly setup the same map
  // from what is stored in flash.
  ASSERT_EQ(Status::OK, kvs_local_.Init());
  EXPECT_EQ(kvs_local_.size(), keys.size());

  // Ensure adding to new KVS works
  uint8_t value = 0xDA;
  const char* key = "new_key";
  ASSERT_EQ(Status::OK, kvs_local_.Put(key, value));
  uint8_t test;
  ASSERT_EQ(Status::OK, kvs_local_.Get(key, &test));
  EXPECT_EQ(value, test);
  EXPECT_EQ(kvs_local_.size(), keys.size() + 1);

  // Verify previous data
  for (unsigned verify_idx = 0; verify_idx < keys.size(); verify_idx++) {
    std::memset(buffer.data(), 0, buffer.size());
    ASSERT_EQ(Status::OK, kvs_local_.Get(keys[verify_idx], buffer).status());
    for (uint32_t i = 0; i < buffer.size(); i++) {
      EXPECT_EQ(static_cast<unsigned>(buffer[i]), verify_idx);
    }
  }
}

TEST_F(KeyValueStoreTest, DISABLED_MultiSector) {
  test_partition.Erase(0, test_partition.sector_count());

  // Reset KVS
  ASSERT_EQ(Status::OK, kvs.Init());

  // Calculate number of elements to ensure multiple sectors are required.
  uint16_t add_count = (test_partition.sector_size_bytes() / buffer.size()) + 1;

  if (kvs.max_size() < add_count) {
    PW_LOG_INFO("Sector size too large, skipping test.");
    return;  // this chip has very large sectors, test won't work
  }
  if (test_partition.sector_count() < 3) {
    PW_LOG_INFO("Not enough sectors, skipping test.");
    return;  // need at least 3 sectors for multi-sector test
  }

  char key[20];
  for (unsigned add_idx = 0; add_idx < add_count; add_idx++) {
    std::memset(buffer.data(), add_idx, buffer.size());
    snprintf(key, sizeof(key), "key_%u", add_idx);
    ASSERT_EQ(Status::OK, kvs.Put(key, buffer));
    EXPECT_EQ(kvs.size(), add_idx + 1);
  }

  for (unsigned verify_idx = 0; verify_idx < add_count; verify_idx++) {
    std::memset(buffer.data(), 0, buffer.size());
    snprintf(key, sizeof(key), "key_%u", verify_idx);
    ASSERT_EQ(Status::OK, kvs.Get(key, buffer).status());
    for (uint32_t i = 0; i < buffer.size(); i++) {
      EXPECT_EQ(static_cast<unsigned>(buffer[i]), verify_idx);
    }
  }

  // Check erase
  for (unsigned erase_idx = 0; erase_idx < add_count; erase_idx++) {
    snprintf(key, sizeof(key), "key_%u", erase_idx);
    ASSERT_EQ(Status::OK, kvs.Delete(key));
    EXPECT_EQ(kvs.size(), add_count - erase_idx - 1);
  }
}

TEST_F(KeyValueStoreTest, DISABLED_RewriteValue) {
  test_partition.Erase(0, test_partition.sector_count());

  // Reset KVS
  ASSERT_EQ(Status::OK, kvs.Init());

  // Write first value
  const uint8_t kValue1 = 0xDA;
  const uint8_t kValue2 = 0x12;
  const char* key = "the_key";
  ASSERT_EQ(Status::OK, kvs.Put(key, as_bytes(span(&kValue1, 1))));

  // Verify
  uint8_t value;
  ASSERT_EQ(Status::OK,
            kvs.Get(key, as_writable_bytes(span(&value, 1))).status());
  EXPECT_EQ(kValue1, value);

  // Write new value for key
  ASSERT_EQ(Status::OK, kvs.Put(key, as_bytes(span(&kValue2, 1))));

  // Verify
  ASSERT_EQ(Status::OK,
            kvs.Get(key, as_writable_bytes(span(&value, 1))).status());
  EXPECT_EQ(kValue2, value);

  // Verify only 1 element exists
  EXPECT_EQ(kvs.size(), 1u);
}

#if 0  // Offset reads are not yet supported

TEST_F(KeyValueStoreTest, OffsetRead) {
  test_partition.Erase(0, test_partition.sector_count());

  // Reset KVS
  ASSERT_EQ(Status::OK, kvs.Init());

  const char* key = "the_key";
  constexpr size_t kReadSize = 16;  // needs to be a multiple of alignment
  constexpr size_t kTestBufferSize = kReadSize * 10;
  ASSERT_GT(buffer.size(), kTestBufferSize);
  ASSERT_LE(kTestBufferSize, 0xFF);

  // Write the entire buffer
  for (size_t i = 0; i < kTestBufferSize; i++) {
    buffer[i] = byte(i);
  }
  ASSERT_EQ(Status::OK, kvs.Put(key, span(buffer.data(), kTestBufferSize)));
  EXPECT_EQ(kvs.size(), 1u);

  // Read in small chunks and verify
  for (unsigned i = 0; i < kTestBufferSize / kReadSize; i++) {
    std::memset(buffer.data(), 0, buffer.size());
    ASSERT_EQ(
        Status::OK,
        kvs.Get(key, span(buffer.data(), kReadSize), i * kReadSize).status());
    for (unsigned j = 0; j < kReadSize; j++) {
      ASSERT_EQ(static_cast<unsigned>(buffer[j]), j + i * kReadSize);
    }
  }
}
#endif

TEST_F(KeyValueStoreTest, DISABLED_MultipleRewrite) {
  // Write many large buffers to force moving to new sector.
  test_partition.Erase(0, test_partition.sector_count());

  // Reset KVS
  ASSERT_EQ(Status::OK, kvs.Init());

  // Calculate number of elements to ensure multiple sectors are required.
  unsigned add_count = (test_partition.sector_size_bytes() / buffer.size()) + 1;

  const char* key = "the_key";
  constexpr uint8_t kGoodVal = 0x60;
  constexpr uint8_t kBadVal = 0xBA;
  std::memset(buffer.data(), kBadVal, buffer.size());
  for (unsigned add_idx = 0; add_idx < add_count; add_idx++) {
    if (add_idx == add_count - 1) {  // last value
      std::memset(buffer.data(), kGoodVal, buffer.size());
    }
    ASSERT_EQ(Status::OK, kvs.Put(key, buffer));
    EXPECT_EQ(kvs.size(), 1u);
  }

  // Verify
  std::memset(buffer.data(), 0, buffer.size());
  ASSERT_EQ(Status::OK, kvs.Get(key, buffer).status());
  for (uint32_t i = 0; i < buffer.size(); i++) {
    ASSERT_EQ(buffer[i], static_cast<byte>(kGoodVal));
  }
}

TEST_F(KeyValueStoreTest, DISABLED_FillSector) {
  // Write key[0], Write/erase Key[2] multiple times to fill sector check
  // everything makes sense after.
  test_partition.Erase(0, test_partition.sector_count());

  // Reset KVS
  ASSERT_EQ(Status::OK, kvs.Init());

  ASSERT_EQ(std::strlen(keys[0]), 8U);  // Easier for alignment
  ASSERT_EQ(std::strlen(keys[2]), 8U);  // Easier for alignment
  constexpr size_t kTestDataSize = 8;
  KvsAttributes kvs_attr(std::strlen(keys[2]), kTestDataSize);
  int bytes_remaining =
      test_partition.sector_size_bytes() - kvs_attr.SectorHeaderSize();
  constexpr byte kKey0Pattern = byte{0xBA};

  std::memset(
      buffer.data(), static_cast<int>(kKey0Pattern), kvs_attr.DataSize());
  ASSERT_EQ(Status::OK,
            kvs.Put(keys[0], span(buffer.data(), kvs_attr.DataSize())));
  bytes_remaining -= kvs_attr.MinPutSize();
  std::memset(buffer.data(), 1, kvs_attr.DataSize());
  ASSERT_EQ(Status::OK,
            kvs.Put(keys[2], span(buffer.data(), kvs_attr.DataSize())));
  bytes_remaining -= kvs_attr.MinPutSize();
  EXPECT_EQ(kvs.size(), 2u);
  ASSERT_EQ(Status::OK, kvs.Delete(keys[2]));
  bytes_remaining -= kvs_attr.EraseSize();
  EXPECT_EQ(kvs.size(), 1u);

  // Intentionally adding erase size to trigger sector cleanup
  bytes_remaining += kvs_attr.EraseSize();
  FillKvs(keys[2], bytes_remaining);

  // Verify key[0]
  std::memset(buffer.data(), 0, kvs_attr.DataSize());
  ASSERT_EQ(
      Status::OK,
      kvs.Get(keys[0], span(buffer.data(), kvs_attr.DataSize())).status());
  for (uint32_t i = 0; i < kvs_attr.DataSize(); i++) {
    EXPECT_EQ(buffer[i], kKey0Pattern);
  }
}

TEST_F(KeyValueStoreTest, DISABLED_Interleaved) {
  test_partition.Erase(0, test_partition.sector_count());

  // Reset KVS
  ASSERT_EQ(Status::OK, kvs.Init());

  const uint8_t kValue1 = 0xDA;
  const uint8_t kValue2 = 0x12;
  uint8_t value;
  ASSERT_EQ(Status::OK, kvs.Put(keys[0], kValue1));
  EXPECT_EQ(kvs.size(), 1u);
  ASSERT_EQ(Status::OK, kvs.Delete(keys[0]));
  EXPECT_EQ(kvs.Get(keys[0], &value), Status::NOT_FOUND);
  ASSERT_EQ(Status::OK, kvs.Put(keys[1], as_bytes(span(&kValue1, 1))));
  ASSERT_EQ(Status::OK, kvs.Put(keys[2], kValue2));
  ASSERT_EQ(Status::OK, kvs.Delete(keys[1]));
  EXPECT_EQ(Status::OK, kvs.Get(keys[2], &value));
  EXPECT_EQ(kValue2, value);

  EXPECT_EQ(kvs.size(), 1u);
}

TEST_F(KeyValueStoreTest, DISABLED_BadCrc) {
  static constexpr uint32_t kTestPattern = 0xBAD0301f;
  // clang-format off
  // There is a top and bottom because for each because we don't want to write
  // the erase 0xFF, especially on encrypted flash.
  static constexpr auto kKvsTestDataAligned1Top = ByteArray(
      0xCD, 0xAB, 0x03, 0x00, 0x01, 0x00, 0xFF, 0xFF   // Sector Header
  );
  static constexpr auto kKvsTestDataAligned1Bottom = ByteArray(
      0xAA, 0x55, 0xBA, 0xDD, 0x00, 0x00, 0x18, 0x00,  // header (BAD CRC)
      0x54, 0x65, 0x73, 0x74, 0x4B, 0x65, 0x79, 0x31,  // Key (keys[0])
      0xDA,                                            // Value
      0xAA, 0x55, 0xB5, 0x87, 0x00, 0x00, 0x44, 0x00,  // Header (GOOD CRC)
      0x4B, 0x65, 0x79, 0x32,                          // Key (keys[1])
      0x1F, 0x30, 0xD0, 0xBA);                         // Value
  static constexpr auto kKvsTestDataAligned2Top = ByteArray(
      0xCD, 0xAB, 0x03, 0x00, 0x02, 0x00, 0xFF, 0xFF   // Sector Header
  );
  static constexpr auto kKvsTestDataAligned2Bottom = ByteArray(
      0xAA, 0x55, 0xBA, 0xDD, 0x00, 0x00, 0x18, 0x00,  // header (BAD CRC)
      0x54, 0x65, 0x73, 0x74, 0x4B, 0x65, 0x79, 0x31,  // Key (keys[0])
      0xDA, 0x00,                                      // Value + padding
      0xAA, 0x55, 0xB5, 0x87, 0x00, 0x00, 0x44, 0x00,  // Header (GOOD CRC)
      0x4B, 0x65, 0x79, 0x32,                          // Key (keys[1])
      0x1F, 0x30, 0xD0, 0xBA                           // Value
  );
  static constexpr auto kKvsTestDataAligned8Top = ByteArray(
      0xCD, 0xAB, 0x03, 0x00, 0x08, 0x00, 0xFF, 0xFF   // Sector Header
  );
  static constexpr auto kKvsTestDataAligned8Bottom = ByteArray(
      0xAA, 0x55, 0xBA, 0xDD, 0x00, 0x00, 0x18, 0x00,  // header (BAD CRC)
      0x54, 0x65, 0x73, 0x74, 0x4B, 0x65, 0x79, 0x31,  // Key (keys[0])
      0xDA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Value + padding
      0xAA, 0x55, 0xB5, 0x87, 0x00, 0x00, 0x44, 0x00,  // header (GOOD CRC)
      0x4B, 0x65, 0x79, 0x32, 0x00, 0x00, 0x00, 0x00,  // Key (keys[1])
      0x1F, 0x30, 0xD0, 0xBA, 0x00, 0x00, 0x00, 0x00   // Value + padding
  );
  static constexpr auto kKvsTestDataAligned16Top = ByteArray(
      0xCD, 0xAB, 0x03, 0x00, 0x10, 0x00, 0xFF, 0xFF,  // Sector Header
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00   // Alignment to 16
  );
  static constexpr auto kKvsTestDataAligned16Bottom = ByteArray(
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
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00   // Alignment to 16
  );
  // clang-format on
  ASSERT_EQ(Status::OK, test_partition.Erase(0, test_partition.sector_count()));

  // We don't actually care about the size values provided, since we are only
  // using kvs_attr to get Sector Size
  KvsAttributes kvs_attr(8, 8);
  if (test_partition.alignment_bytes() == 1) {
    ASSERT_EQ(Status::OK,
              test_partition.Write(0, kKvsTestDataAligned1Top).status());
    ASSERT_EQ(
        Status::OK,
        test_partition
            .Write(kvs_attr.SectorHeaderSize(), kKvsTestDataAligned1Bottom)
            .status());
  } else if (test_partition.alignment_bytes() == 2) {
    ASSERT_EQ(Status::OK,
              test_partition.Write(0, kKvsTestDataAligned2Top).status());
    ASSERT_EQ(
        Status::OK,
        test_partition
            .Write(kvs_attr.SectorHeaderSize(), kKvsTestDataAligned2Bottom)
            .status());
  } else if (test_partition.alignment_bytes() == 8) {
    ASSERT_EQ(Status::OK,
              test_partition.Write(0, kKvsTestDataAligned8Top).status());
    ASSERT_EQ(
        Status::OK,
        test_partition
            .Write(kvs_attr.SectorHeaderSize(), kKvsTestDataAligned8Bottom)
            .status());
  } else if (test_partition.alignment_bytes() == 16) {
    ASSERT_EQ(Status::OK,
              test_partition.Write(0, kKvsTestDataAligned16Top).status());
    ASSERT_EQ(
        Status::OK,
        test_partition
            .Write(kvs_attr.SectorHeaderSize(), kKvsTestDataAligned16Bottom)
            .status());
  } else {
    PW_LOG_ERROR("Test only supports 1, 2, 8 and 16 byte alignments.");
    ASSERT_EQ(Status::OK, false);
  }

  EXPECT_EQ(kvs_local_.Init(), Status::OK);
  EXPECT_TRUE(kvs_local_.initialized());

  EXPECT_EQ(Status::DATA_LOSS,
            kvs_local_.Get(keys[0], span(buffer.data(), 1)).status());

  // Value with correct CRC should still be available.
  uint32_t test2 = 0;
  ASSERT_EQ(Status::OK, kvs_local_.Get(keys[1], &test2));
  EXPECT_EQ(kTestPattern, test2);

  // Test rewriting over corrupted data.
  ASSERT_EQ(Status::OK, kvs_local_.Put(keys[0], kTestPattern));
  test2 = 0;
  EXPECT_EQ(Status::OK, kvs_local_.Get(keys[0], &test2));
  EXPECT_EQ(kTestPattern, test2);

  // Check correct when re-enabled
  EXPECT_EQ(kvs_local_.Init(), Status::OK);
  test2 = 0;
  EXPECT_EQ(Status::OK, kvs_local_.Get(keys[0], &test2));
  EXPECT_EQ(kTestPattern, test2);
}

TEST_F(KeyValueStoreTest, DISABLED_TestVersion2) {
  static constexpr uint32_t kTestPattern = 0xBAD0301f;
  // Since this test is not run on encypted flash, we can write the clean
  // pending flag for just this test.
  static constexpr uint8_t kKvsTestDataAligned1[] = {
      0xCD, 0xAB, 0x02, 0x00, 0x00, 0x00, 0xFF, 0xFF,  // Sector Header
      0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // Clean pending flag
      0xAA, 0x55, 0xB5, 0x87, 0x00, 0x00, 0x44, 0x00,  // Header (GOOD CRC)
      0x4B, 0x65, 0x79, 0x32,                          // Key (keys[1])
      0x1F, 0x30, 0xD0, 0xBA};                         // Value

  if (test_partition.alignment_bytes() == 1) {
    // Test only runs on 1 byte alignment partitions
    test_partition.Erase(0, test_partition.sector_count());
    test_partition.Write(0, as_bytes(span(kKvsTestDataAligned1)));
    EXPECT_EQ(Status::OK, kvs_local_.Init());
    uint32_t test2 = 0;
    ASSERT_EQ(
        Status::OK,
        kvs_local_.Get(keys[1], as_writable_bytes(span(&test2, 1))).status());
    EXPECT_EQ(kTestPattern, test2);
  }
}

TEST_F(KeyValueStoreTest, DISABLED_ReEnable) {
  test_partition.Erase(0, test_partition.sector_count());

  // Reset KVS
  ASSERT_EQ(Status::OK, kvs.Init());
  EXPECT_EQ(Status::OK, kvs_local_.Init());
  // Write value
  const uint8_t kValue = 0xDA;
  ASSERT_EQ(Status::OK, kvs_local_.Put(keys[0], kValue));
  uint8_t value;
  ASSERT_EQ(Status::OK, kvs_local_.Get(keys[0], &value));

  // Verify
  EXPECT_EQ(kValue, value);
}

TEST_F(KeyValueStoreTest, DISABLED_Erase) {
  test_partition.Erase(0, test_partition.sector_count());

  // Reset KVS
  ASSERT_EQ(Status::OK, kvs.Init());

  // Write value
  const uint8_t kValue = 0xDA;
  ASSERT_EQ(Status::OK, kvs.Put(keys[0], kValue));

  ASSERT_EQ(Status::OK, kvs.Delete(keys[0]));
  uint8_t value;
  ASSERT_EQ(kvs.Get(keys[0], &value), Status::NOT_FOUND);

  // Reset KVS, ensure captured at enable
  ASSERT_EQ(Status::OK, kvs.Init());

  ASSERT_EQ(kvs.Get(keys[0], &value), Status::NOT_FOUND);
}

TEST_F(KeyValueStoreTest, DISABLED_TemplatedPutAndGet) {
  test_partition.Erase(0, test_partition.sector_count());
  // Reset KVS
  ASSERT_EQ(Status::OK, kvs.Init());
  // Store a value with the convenience method.
  const uint32_t kValue = 0x12345678;
  ASSERT_EQ(Status::OK, kvs.Put(keys[0], kValue));

  // Read it back with the other convenience method.
  uint32_t value;
  ASSERT_EQ(Status::OK, kvs.Get(keys[0], &value));
  ASSERT_EQ(kValue, value);

  // Make sure we cannot get something where size isn't what we expect
  const uint8_t kSmallValue = 0xBA;
  uint8_t small_value = kSmallValue;
  ASSERT_EQ(kvs.Get(keys[0], &small_value), Status::INVALID_ARGUMENT);
  ASSERT_EQ(small_value, kSmallValue);
}

TEST_F(KeyValueStoreTest, DISABLED_SameValueRewrite) {
  static constexpr uint32_t kTestPattern = 0xBAD0301f;
  // clang-format off
  static constexpr auto kKvsTestDataAligned1Top = ByteArray(
      0xCD, 0xAB, 0x02, 0x00, 0x00, 0x00, 0xFF, 0xFF  // Sector Header
  );
  static constexpr auto kKvsTestDataAligned1Bottom = ByteArray(
      0xAA, 0x55, 0xB5, 0x87, 0x00, 0x00, 0x44, 0x00, // Header (GOOD CRC)
      0x4B, 0x65, 0x79, 0x32,                          // Key (keys[1])
      0x1F, 0x30, 0xD0, 0xBA  // Value
      );
  static constexpr auto kKvsTestDataAligned2Top = ByteArray(
      0xCD, 0xAB, 0x03, 0x00, 0x02, 0x00, 0xFF, 0xFF   // Sector Header
  );
  static constexpr auto kKvsTestDataAligned2Bottom = ByteArray(
      0xAA, 0x55, 0xB5, 0x87, 0x00, 0x00, 0x44, 0x00,  // Header (GOOD CRC)
      0x4B, 0x65, 0x79, 0x32,                          // Key (keys[1])
      0x1F, 0x30, 0xD0, 0xBA                           // Value
  );
  static constexpr auto kKvsTestDataAligned8Top = ByteArray(
      0xCD, 0xAB, 0x03, 0x00, 0x08, 0x00, 0xFF, 0xFF   // Sector Header
  );
  static constexpr auto kKvsTestDataAligned8Bottom = ByteArray(
      0xAA, 0x55, 0xB5, 0x87, 0x00, 0x00, 0x44, 0x00,  // header (GOOD CRC)
      0x4B, 0x65, 0x79, 0x32, 0x00, 0x00, 0x00, 0x00,  // Key (keys[1])
      0x1F, 0x30, 0xD0, 0xBA, 0x00, 0x00, 0x00, 0x00   // Value + padding
  );
  static constexpr auto kKvsTestDataAligned16Top = ByteArray(
      0xCD, 0xAB, 0x03, 0x00, 0x10, 0x00, 0xFF, 0xFF,  // Sector Header
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00   // Alignment to 16
  );
  static constexpr auto kKvsTestDataAligned16Bottom = ByteArray(
      0xAA, 0x55, 0xB5, 0x87, 0x00, 0x00, 0x44, 0x00,  // header (GOOD CRC)
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Alignment to 16
      0x4B, 0x65, 0x79, 0x32, 0x00, 0x00, 0x00, 0x00,  // Key (keys[1])
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // Alignment to 16
      0x1F, 0x30, 0xD0, 0xBA, 0x00, 0x00, 0x00, 0x00,  // Value + padding
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00   // Alignment to 16
  );
  // clang-format on

  ASSERT_EQ(Status::OK, test_partition.Erase(0, test_partition.sector_count()));
  // We don't actually care about the size values provided, since we are only
  // using kvs_attr to get Sector Size
  KvsAttributes kvs_attr(8, 8);
  FlashPartition::Address address = kvs_attr.SectorHeaderSize();
  if (test_partition.alignment_bytes() == 1) {
    ASSERT_EQ(Status::OK,
              test_partition.Write(0, kKvsTestDataAligned1Top).status());
    ASSERT_EQ(
        Status::OK,
        test_partition.Write(address, kKvsTestDataAligned1Bottom).status());
    address += sizeof(kKvsTestDataAligned1Bottom);
  } else if (test_partition.alignment_bytes() == 2) {
    ASSERT_EQ(Status::OK,
              test_partition.Write(0, kKvsTestDataAligned2Top).status());
    ASSERT_EQ(
        Status::OK,
        test_partition.Write(address, kKvsTestDataAligned2Bottom).status());
    address += sizeof(kKvsTestDataAligned2Bottom);
  } else if (test_partition.alignment_bytes() == 8) {
    ASSERT_EQ(Status::OK,
              test_partition.Write(0, kKvsTestDataAligned8Top).status());
    ASSERT_EQ(
        Status::OK,
        test_partition.Write(address, kKvsTestDataAligned8Bottom).status());
    address += sizeof(kKvsTestDataAligned8Bottom);
  } else if (test_partition.alignment_bytes() == 16) {
    ASSERT_EQ(Status::OK,
              test_partition.Write(0, kKvsTestDataAligned16Top).status());
    ASSERT_EQ(
        Status::OK,
        test_partition.Write(address, kKvsTestDataAligned16Bottom).status());
    address += sizeof(kKvsTestDataAligned16Bottom);
  } else {
    PW_LOG_ERROR("Test only supports 1, 2, 8 and 16 byte alignments.");
    ASSERT_EQ(true, false);
  }

  ASSERT_EQ(Status::OK, kvs_local_.Init());
  EXPECT_TRUE(kvs_local_.initialized());

  // Put in same key/value pair
  ASSERT_EQ(Status::OK, kvs_local_.Put(keys[1], kTestPattern));

  bool is_erased = false;
  ASSERT_EQ(Status::OK,
            test_partition.IsRegionErased(
                address, test_partition.alignment_bytes(), &is_erased));
  EXPECT_EQ(is_erased, true);
}

// This test is derived from bug that was discovered. Testing this corner case
// relies on creating a new key-value just under the size that is left over in
// the sector.
TEST_F(KeyValueStoreTest, DISABLED_FillSector2) {
  if (test_partition.sector_count() < 3) {
    PW_LOG_INFO("Not enough sectors, skipping test.");
    return;  // need at least 3 sectors
  }

  // Reset KVS
  test_partition.Erase(0, test_partition.sector_count());
  ASSERT_EQ(Status::OK, kvs.Init());

  // Start of by filling flash sector to near full
  constexpr int kHalfBufferSize = buffer.size() / 2;
  const int kSizeToFill = test_partition.sector_size_bytes() - kHalfBufferSize;
  constexpr size_t kTestDataSize = 8;
  KvsAttributes kvs_attr(std::strlen(keys[2]), kTestDataSize);

  FillKvs(keys[2], kSizeToFill);

  // Find out how much space is remaining for new key-value and confirm it
  // makes sense.
  size_t new_keyvalue_size = 0;
  size_t alignment = test_partition.alignment_bytes();
  // Starts on second sector since it will try to keep first sector free
  FlashPartition::Address read_address =
      2 * test_partition.sector_size_bytes() - alignment;
  for (; read_address > 0; read_address -= alignment) {
    bool is_erased = false;
    ASSERT_EQ(
        Status::OK,
        test_partition.IsRegionErased(read_address, alignment, &is_erased));
    if (is_erased) {
      new_keyvalue_size += alignment;
    } else {
      break;
    }
  }

  size_t expected_remaining = test_partition.sector_size_bytes() -
                              kvs_attr.SectorHeaderSize() - kSizeToFill;
  ASSERT_EQ(new_keyvalue_size, expected_remaining);

  const char* kNewKey = "NewKey";
  constexpr size_t kValueLessThanChunkHeaderSize = 2;
  constexpr auto kTestPattern = byte{0xBA};
  new_keyvalue_size -= kValueLessThanChunkHeaderSize;
  std::memset(buffer.data(), static_cast<int>(kTestPattern), new_keyvalue_size);
  ASSERT_EQ(Status::OK,
            kvs.Put(kNewKey, span(buffer.data(), new_keyvalue_size)));

  // In failed corner case, adding new key is deceptively successful. It isn't
  // until KVS is disabled and reenabled that issue can be detected.
  ASSERT_EQ(Status::OK, kvs.Init());

  // Might as well check that new key-value is what we expect it to be
  ASSERT_EQ(Status::OK,
            kvs.Get(kNewKey, span(buffer.data(), new_keyvalue_size)).status());
  for (size_t i = 0; i < new_keyvalue_size; i++) {
    EXPECT_EQ(buffer[i], kTestPattern);
  }
}

TEST_F(KeyValueStoreTest, DISABLED_GetValueSizeTests) {
  constexpr uint16_t kSizeOfValueToFill = 20U;
  constexpr uint8_t kKey0Pattern = 0xBA;
  // Start off with disabled KVS
  // kvs.Disable();

  // Try getting value when KVS is disabled, expect failure
  EXPECT_EQ(kvs.ValueSize(keys[0]).status(), Status::FAILED_PRECONDITION);

  // Reset KVS
  test_partition.Erase(0, test_partition.sector_count());
  ASSERT_EQ(Status::OK, kvs.Init());

  // Try some case that are expected to fail
  ASSERT_EQ(kvs.ValueSize(keys[0]).status(), Status::NOT_FOUND);
  ASSERT_EQ(kvs.ValueSize("").status(), Status::INVALID_ARGUMENT);

  // Add key[0] and test we get the right value size for it.
  std::memset(buffer.data(), kKey0Pattern, kSizeOfValueToFill);
  ASSERT_EQ(Status::OK,
            kvs.Put(keys[0], span(buffer.data(), kSizeOfValueToFill)));
  ASSERT_EQ(kSizeOfValueToFill, kvs.ValueSize(keys[0]).size());

  // Verify after erase key is not found
  ASSERT_EQ(Status::OK, kvs.Delete(keys[0]));
  ASSERT_EQ(kvs.ValueSize(keys[0]).status(), Status::NOT_FOUND);
}

#if 0  // TODO: not CanFitEntry function yet
TEST_F(KeyValueStoreTest, DISABLED_CanFitEntryTests) {
  // Reset KVS
  test_partition.Erase(0, test_partition.sector_count());
  ASSERT_EQ(Status::OK, kvs.Init());

  // Get exactly the number of bytes that can fit in the space remaining for
  // a large value, accounting for alignment.
  constexpr uint16_t kTestKeySize = 2;
  size_t space_remaining =
      test_partition.sector_size_bytes()          //
      - RoundUpForAlignment(sizeof(EntryHeader))  // TODO: Sector Header
      - RoundUpForAlignment(sizeof(EntryHeader))  // Cleaning Header
      - RoundUpForAlignment(sizeof(EntryHeader))  // TODO: Chunk Header
      - RoundUpForAlignment(kTestKeySize);
  space_remaining -= test_partition.alignment_bytes() / 2;
  space_remaining = RoundUpForAlignment(space_remaining);

  EXPECT_TRUE(kvs.CanFitEntry(kTestKeySize, space_remaining));
  EXPECT_FALSE(kvs.CanFitEntry(kTestKeySize, space_remaining + 1));
}
#endif

TEST_F(KeyValueStoreTest, DISABLED_DifferentValueSameCrc16) {
  test_partition.Erase(0, test_partition.sector_count());
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
  ASSERT_EQ(Status::OK, kvs.Init());
  ASSERT_EQ(Status::OK, kvs.Put(kKey, kValue1));

  // Now try to rewrite with the similar value.
  ASSERT_EQ(Status::OK, kvs.Put(kKey, kValue2));

  // Read it back and check it is correct
  char value[3];
  ASSERT_EQ(Status::OK, kvs.Get(kKey, &value));
  ASSERT_EQ(std::memcmp(value, kValue2, sizeof(value)), 0);
}

TEST_F(KeyValueStoreTest, DISABLED_CallingEraseTwice) {
  test_partition.Erase(0, test_partition.sector_count());

  // Reset KVS
  ASSERT_EQ(Status::OK, kvs.Init());

  const uint8_t kValue = 0xDA;
  ASSERT_EQ(Status::OK, kvs.Put(keys[0], kValue));
  ASSERT_EQ(Status::OK, kvs.Delete(keys[0]));
  uint16_t crc = CalcTestPartitionCrc();
  EXPECT_EQ(kvs.Delete(keys[0]), Status::NOT_FOUND);
  // Verify the flash has not changed
  EXPECT_EQ(crc, CalcTestPartitionCrc());
}

void __attribute__((noinline)) StackHeavyPartialClean() {
#if 0  // TODO: No FlashSubPartition

  ASSERT_GE(test_partition.sector_count(), 2);
  FlashSubPartition test_partition_sector1(&test_partition, 0, 1);
  FlashSubPartition test_partition_sector2(&test_partition, 1, 1);

  KeyValueStore kvs1(&test_partition_sector1);
  KeyValueStore kvs2(&test_partition_sector2);

  test_partition.Erase(0, test_partition.sector_count());

  ASSERT_EQ(Status::OK, kvs1.Enable());
  ASSERT_EQ(Status::OK, kvs2.Enable());

  int values1[3] = {100, 101, 102};
  ASSERT_EQ(Status::OK, kvs1.Put(keys[0], values1[0]));
  ASSERT_EQ(Status::OK, kvs1.Put(keys[1], values1[1]));
  ASSERT_EQ(Status::OK, kvs1.Put(keys[2], values1[2]));

  int values2[3] = {200, 201, 202};
  ASSERT_EQ(Status::OK, kvs2.Put(keys[0], values2[0]));
  ASSERT_EQ(Status::OK, kvs2.Put(keys[1], values2[1]));
  ASSERT_EQ(Status::OK, kvs2.Delete(keys[1]));

  kvs1.Disable();
  kvs2.Disable();

  // Key 0 is value1 in first sector, value 2 in second
  // Key 1 is value1 in first sector, erased in second
  // key 2 is only in first sector

  uint64_t mark_clean_count = 5;
  ASSERT_EQ(Status::OK,
            PaddedWrite(&test_partition_sector1,
                        RoundUpForAlignment(KeyValueStore::kHeaderSize),
                        reinterpret_cast<uint8_t*>(&mark_clean_count),
                        sizeof(uint64_t)));

  // Reset KVS
  kvs.Disable();
  ASSERT_EQ(Status::OK, kvs.Init());
  int value;
  ASSERT_EQ(Status::OK, kvs.Get(keys[0], &value));
  ASSERT_EQ(values2[0], value);
  ASSERT_EQ(kvs.Get(keys[1], &value), Status::NOT_FOUND);
  ASSERT_EQ(Status::OK, kvs.Get(keys[2], &value));
  ASSERT_EQ(values1[2], value);

  if (test_partition.sector_count() == 2) {
    EXPECT_EQ(kvs.PendingCleanCount(), 0u);
    // Has forced a clean, mark again for next test
    return;  // Not enough sectors to test 2 partial cleans.
  } else {
    EXPECT_EQ(kvs.PendingCleanCount(), 1u);
  }

  mark_clean_count--;
  ASSERT_EQ(Status::OK,
            PaddedWrite(&test_partition_sector2,
                        RoundUpForAlignment(KeyValueStore::kHeaderSize),
                        reinterpret_cast<uint8_t*>(&mark_clean_count),
                        sizeof(uint64_t)));
  // Reset KVS
  kvs.Disable();
  ASSERT_EQ(Status::OK, kvs.Init());
  EXPECT_EQ(kvs.PendingCleanCount(), 2u);
  ASSERT_EQ(Status::OK, kvs.Get(keys[0], &value));
  ASSERT_EQ(values1[0], value);
  ASSERT_EQ(Status::OK, kvs.Get(keys[1], &value));
  ASSERT_EQ(values1[1], value);
  ASSERT_EQ(Status::OK, kvs.Get(keys[2], &value));
  ASSERT_EQ(values1[2], value);
#endif
}

// TODO: This doesn't do anything, and would be unreliable anyway.
size_t CurrentTaskStackFree() { return -1; }

TEST_F(KeyValueStoreTest, DISABLED_PartialClean) {
  if (CurrentTaskStackFree() < sizeof(KeyValueStore) * 2) {
    PW_LOG_ERROR("Not enough stack for test, skipping");
    return;
  }
  StackHeavyPartialClean();
}

void __attribute__((noinline)) StackHeavyCleanAll() {
#if 0  // TODO: no FlashSubPartition
  ASSERT_GE(test_partition.sector_count(), 2);
  FlashSubPartition test_partition_sector1(&test_partition, 0, 1);

  KeyValueStore kvs1(&test_partition_sector1);
  test_partition.Erase(0, test_partition.sector_count());

  ASSERT_EQ(Status::OK, kvs1.Enable());

  int values1[3] = {100, 101, 102};
  ASSERT_EQ(Status::OK, kvs1.Put(keys[0], values1[0]));
  ASSERT_EQ(Status::OK, kvs1.Put(keys[1], values1[1]));
  ASSERT_EQ(Status::OK,
            kvs1.Put(keys[2], values1[2] - 100));  //  force a rewrite
  ASSERT_EQ(Status::OK, kvs1.Put(keys[2], values1[2]));

  kvs1.Disable();

  uint64_t mark_clean_count = 5;
  ASSERT_EQ(Status::OK,
            PaddedWrite(&test_partition_sector1,
                        RoundUpForAlignment(KeyValueStore::kHeaderSize),
                        reinterpret_cast<uint8_t*>(&mark_clean_count),
                        sizeof(uint64_t)));

  // Reset KVS
  kvs.Disable();
  ASSERT_EQ(Status::OK, kvs.Init());
  int value;
  EXPECT_EQ(kvs.PendingCleanCount(), 1u);
  ASSERT_EQ(Status::OK, kvs.CleanAll());
  EXPECT_EQ(kvs.PendingCleanCount(), 0u);
  ASSERT_EQ(Status::OK, kvs.Get(keys[0], &value));
  ASSERT_EQ(values1[0], value);
  ASSERT_EQ(Status::OK, kvs.Get(keys[1], &value));
  ASSERT_EQ(values1[1], value);
  ASSERT_EQ(Status::OK, kvs.Get(keys[2], &value));
  ASSERT_EQ(values1[2], value);
#endif
}

TEST_F(KeyValueStoreTest, DISABLED_CleanAll) {
  if (CurrentTaskStackFree() < sizeof(KeyValueStore) * 1) {
    PW_LOG_ERROR("Not enough stack for test, skipping");
    return;
  }
  StackHeavyCleanAll();
}

void __attribute__((noinline)) StackHeavyPartialCleanLargeCounts() {
#if 0
  ASSERT_GE(test_partition.sector_count(), 2);
  FlashSubPartition test_partition_sector1(&test_partition, 0, 1);
  FlashSubPartition test_partition_sector2(&test_partition, 1, 1);

  KeyValueStore kvs1(&test_partition_sector1);
  KeyValueStore kvs2(&test_partition_sector2);

  test_partition.Erase(0, test_partition.sector_count());

  ASSERT_EQ(Status::OK, kvs1.Enable());
  ASSERT_EQ(Status::OK, kvs2.Enable());

  int values1[3] = {100, 101, 102};
  ASSERT_EQ(Status::OK, kvs1.Put(keys[0], values1[0]));
  ASSERT_EQ(Status::OK, kvs1.Put(keys[1], values1[1]));
  ASSERT_EQ(Status::OK, kvs1.Put(keys[2], values1[2]));

  int values2[3] = {200, 201, 202};
  ASSERT_EQ(Status::OK, kvs2.Put(keys[0], values2[0]));
  ASSERT_EQ(Status::OK, kvs2.Put(keys[1], values2[1]));
  ASSERT_EQ(Status::OK, kvs2.Delete(keys[1]));

  kvs1.Disable();
  kvs2.Disable();
  kvs.Disable();

  // Key 0 is value1 in first sector, value 2 in second
  // Key 1 is value1 in first sector, erased in second
  // key 2 is only in first sector

  uint64_t mark_clean_count = 4569877515;
  ASSERT_EQ(Status::OK,
            PaddedWrite(&test_partition_sector1,
                        RoundUpForAlignment(KeyValueStore::kHeaderSize),
                        reinterpret_cast<uint8_t*>(&mark_clean_count),
                        sizeof(uint64_t)));

  // Reset KVS
  ASSERT_EQ(Status::OK, kvs.Init());
  int value;
  ASSERT_EQ(Status::OK, kvs.Get(keys[0], &value));
  ASSERT_EQ(values2[0], value);
  ASSERT_EQ(kvs.Get(keys[1], &value), Status::NOT_FOUND);
  ASSERT_EQ(Status::OK, kvs.Get(keys[2], &value));
  ASSERT_EQ(values1[2], value);

  if (test_partition.sector_count() == 2) {
    EXPECT_EQ(kvs.PendingCleanCount(), 0u);
    // Has forced a clean, mark again for next test
    // Has forced a clean, mark again for next test
    return;  // Not enough sectors to test 2 partial cleans.
  } else {
    EXPECT_EQ(kvs.PendingCleanCount(), 1u);
  }
  kvs.Disable();

  mark_clean_count--;
  ASSERT_EQ(Status::OK,
            PaddedWrite(&test_partition_sector2,
                        RoundUpForAlignment(KeyValueStore::kHeaderSize),
                        reinterpret_cast<uint8_t*>(&mark_clean_count),
                        sizeof(uint64_t)));
  // Reset KVS
  ASSERT_EQ(Status::OK, kvs.Init());
  EXPECT_EQ(kvs.PendingCleanCount(), 2u);
  ASSERT_EQ(Status::OK, kvs.Get(keys[0], &value));
  ASSERT_EQ(values1[0], value);
  ASSERT_EQ(Status::OK, kvs.Get(keys[1], &value));
  ASSERT_EQ(values1[1], value);
  ASSERT_EQ(Status::OK, kvs.Get(keys[2], &value));
  ASSERT_EQ(values1[2], value);
#endif
}

TEST_F(KeyValueStoreTest, DISABLED_PartialCleanLargeCounts) {
  if (CurrentTaskStackFree() < sizeof(KeyValueStore) * 2) {
    PW_LOG_ERROR("Not enough stack for test, skipping");
    return;
  }
  StackHeavyPartialCleanLargeCounts();
}

void __attribute__((noinline)) StackHeavyRecoverNoFreeSectors() {
#if 0  // TODO: no FlashSubPartition
  ASSERT_GE(test_partition.sector_count(), 2);
  FlashSubPartition test_partition_sector1(&test_partition, 0, 1);
  FlashSubPartition test_partition_sector2(&test_partition, 1, 1);
  FlashSubPartition test_partition_both(&test_partition, 0, 2);

  KeyValueStore kvs1(&test_partition_sector1);
  KeyValueStore kvs2(&test_partition_sector2);
  KeyValueStore kvs_both(&test_partition_both);

  test_partition.Erase(0, test_partition.sector_count());

  ASSERT_EQ(Status::OK, kvs1.Enable());
  ASSERT_EQ(Status::OK, kvs2.Enable());

  int values[3] = {100, 101};
  ASSERT_EQ(Status::OK, kvs1.Put(keys[0], values[0]));
  ASSERT_FALSE(kvs1.HasEmptySector());
  ASSERT_EQ(Status::OK, kvs2.Put(keys[1], values[1]));
  ASSERT_FALSE(kvs2.HasEmptySector());

  kvs1.Disable();
  kvs2.Disable();

  // Reset KVS
  ASSERT_EQ(Status::OK, kvs_both.Enable());
  ASSERT_TRUE(kvs_both.HasEmptySector());
  int value;
  ASSERT_EQ(Status::OK, kvs_both.Get(keys[0], &value));
  ASSERT_EQ(values[0], value);
  ASSERT_EQ(Status::OK, kvs_both.Get(keys[1], &value));
  ASSERT_EQ(values[1], value);
#endif
}

TEST_F(KeyValueStoreTest, RecoverNoFreeSectors) {
  if (CurrentTaskStackFree() < sizeof(KeyValueStore) * 3) {
    PW_LOG_ERROR("Not enough stack for test, skipping");
    return;
  }
  StackHeavyRecoverNoFreeSectors();
}

void __attribute__((noinline)) StackHeavyCleanOneSector() {
#if 0  // TODO: no FlashSubPartition
  ASSERT_GE(test_partition.sector_count(), 2);
  FlashSubPartition test_partition_sector1(&test_partition, 0, 1);
  FlashSubPartition test_partition_sector2(&test_partition, 1, 1);

  KeyValueStore kvs1(&test_partition_sector1);

  test_partition.Erase(0, test_partition.sector_count());

  ASSERT_EQ(Status::OK, kvs1.Enable());

  int values[3] = {100, 101, 102};
  ASSERT_EQ(Status::OK, kvs1.Put(keys[0], values[0]));
  ASSERT_EQ(Status::OK, kvs1.Put(keys[1], values[1]));
  ASSERT_EQ(Status::OK, kvs1.Put(keys[2], values[2]));

  kvs1.Disable();
  kvs.Disable();

  uint64_t mark_clean_count = 1;
  ASSERT_EQ(Status::OK,
            PaddedWrite(&test_partition_sector1,
                        RoundUpForAlignment(KeyValueStore::kHeaderSize),
                        reinterpret_cast<uint8_t*>(&mark_clean_count),
                        sizeof(uint64_t)));

  // Reset KVS
  ASSERT_EQ(Status::OK, kvs.Init());

  EXPECT_EQ(kvs.PendingCleanCount(), 1u);

  bool all_sectors_have_been_cleaned = false;
  ASSERT_EQ(Status::OK, kvs.CleanOneSector(&all_sectors_have_been_cleaned));
  EXPECT_EQ(all_sectors_have_been_cleaned, true);
  EXPECT_EQ(kvs.PendingCleanCount(), 0u);
  ASSERT_EQ(Status::OK, kvs.CleanOneSector(&all_sectors_have_been_cleaned));
  EXPECT_EQ(all_sectors_have_been_cleaned, true);
  int value;
  ASSERT_EQ(Status::OK, kvs.Get(keys[0], &value));
  ASSERT_EQ(values[0], value);
  ASSERT_EQ(Status::OK, kvs.Get(keys[1], &value));
  ASSERT_EQ(values[1], value);
  ASSERT_EQ(Status::OK, kvs.Get(keys[2], &value));
  ASSERT_EQ(values[2], value);
#endif
}

TEST_F(KeyValueStoreTest, DISABLED_CleanOneSector) {
  if (CurrentTaskStackFree() < sizeof(KeyValueStore)) {
    PW_LOG_ERROR("Not enough stack for test, skipping");
    return;
  }
  StackHeavyCleanOneSector();
}

#if USE_MEMORY_BUFFER

TEST_F(KeyValueStoreTest, DISABLED_LargePartition) {
  if (CurrentTaskStackFree() < sizeof(KeyValueStore)) {
    PW_LOG_ERROR("Not enough stack for test, skipping");
    return;
  }
  large_test_partition.Erase(0, large_test_partition.sector_count());
  KeyValueStore large_kvs(&large_test_partition, format);
  // Reset KVS
  ASSERT_EQ(Status::OK, large_kvs.Init());

  const uint8_t kValue1 = 0xDA;
  const uint8_t kValue2 = 0x12;
  uint8_t value;
  ASSERT_EQ(Status::OK, large_kvs.Put(keys[0], kValue1));
  EXPECT_EQ(large_kvs.size(), 1u);
  ASSERT_EQ(Status::OK, large_kvs.Delete(keys[0]));
  EXPECT_EQ(large_kvs.Get(keys[0], &value), Status::NOT_FOUND);
  ASSERT_EQ(Status::OK, large_kvs.Put(keys[1], kValue1));
  ASSERT_EQ(Status::OK, large_kvs.Put(keys[2], kValue2));
  ASSERT_EQ(Status::OK, large_kvs.Delete(keys[1]));
  EXPECT_EQ(Status::OK, large_kvs.Get(keys[2], &value));
  EXPECT_EQ(kValue2, value);
  ASSERT_EQ(large_kvs.Get(keys[1], &value), Status::NOT_FOUND);
  EXPECT_EQ(large_kvs.size(), 1u);
}
#endif  // USE_MEMORY_BUFFER

TEST(KeyValueStoreEntryHeader, KeyValueSizes) {
  EntryHeader header;

  header.set_key_length(9u);
  EXPECT_EQ(header.key_length(), 9u);

  header.set_value_length(11u);
  EXPECT_EQ(header.value_length(), 11u);

  header.set_key_length(6u);
  header.set_value_length(100u);
  EXPECT_EQ(header.key_length(), 6u);
  EXPECT_EQ(header.value_length(), 100u);

  header.set_value_length(10u);
  EXPECT_EQ(header.key_length(), 6u);
  EXPECT_EQ(header.value_length(), 10u);

  header.set_key_length(3u);
  header.set_value_length(4000u);
  EXPECT_EQ(header.key_length(), 3u);
  EXPECT_EQ(header.value_length(), 4000u);
}

}  // namespace pw::kvs
