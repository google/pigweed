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

#define DUMP_KVS_STATE_TO_FILE 0
#define USE_MEMORY_BUFFER 1
#define PW_LOG_USE_ULTRA_SHORT_NAMES 1

#include "pw_kvs/key_value_store.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <span>

#if DUMP_KVS_STATE_TO_FILE
#include <vector>
#endif  // DUMP_KVS_STATE_TO_FILE

#include "gtest/gtest.h"
#include "pw_checksum/ccitt_crc16.h"
#include "pw_kvs/crc16_checksum.h"
#include "pw_kvs/fake_flash_memory.h"
#include "pw_kvs/flash_memory.h"
#include "pw_kvs/internal/entry.h"
#include "pw_kvs_private/byte_utils.h"
#include "pw_kvs_private/macros.h"
#include "pw_log/log.h"
#include "pw_status/status.h"
#include "pw_string/string_builder.h"

namespace pw::kvs {
namespace {

using internal::EntryHeader;
using std::byte;

constexpr size_t kMaxEntries = 256;
constexpr size_t kMaxUsableSectors = 256;

// Test the functions in byte_utils.h. Create a byte array with AsBytes and
// ByteStr and check that its contents are correct.
constexpr std::array<char, 2> kTestArray = {'a', 'b'};

constexpr auto kAsBytesTest = AsBytes(
    'a', uint16_t(1), uint8_t(23), kTestArray, ByteStr("c"), uint64_t(-1));

static_assert(kAsBytesTest.size() == 15);
static_assert(kAsBytesTest[0] == std::byte{'a'});
static_assert(kAsBytesTest[1] == std::byte{1});
static_assert(kAsBytesTest[2] == std::byte{0});
static_assert(kAsBytesTest[3] == std::byte{23});
static_assert(kAsBytesTest[4] == std::byte{'a'});
static_assert(kAsBytesTest[5] == std::byte{'b'});
static_assert(kAsBytesTest[6] == std::byte{'c'});
static_assert(kAsBytesTest[7] == std::byte{0xff});
static_assert(kAsBytesTest[8] == std::byte{0xff});
static_assert(kAsBytesTest[9] == std::byte{0xff});
static_assert(kAsBytesTest[10] == std::byte{0xff});
static_assert(kAsBytesTest[11] == std::byte{0xff});
static_assert(kAsBytesTest[12] == std::byte{0xff});
static_assert(kAsBytesTest[13] == std::byte{0xff});
static_assert(kAsBytesTest[14] == std::byte{0xff});

// Test that the ConvertsToSpan trait correctly idenitifies types that convert
// to std::span.
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

static_assert(ConvertsToSpan<bool[1]>());
static_assert(ConvertsToSpan<char[35]>());
static_assert(ConvertsToSpan<const int[35]>());

static_assert(ConvertsToSpan<std::span<int>>());
static_assert(ConvertsToSpan<std::span<byte>>());
static_assert(ConvertsToSpan<std::span<const int*>>());
static_assert(ConvertsToSpan<std::span<bool>&&>());
static_assert(ConvertsToSpan<const std::span<bool>&>());
static_assert(ConvertsToSpan<std::span<bool>&&>());

// This is a self contained flash unit with both memory and a single partition.
template <uint32_t sector_size_bytes, uint16_t sector_count>
struct FlashWithPartitionFake {
  // Default to 16 byte alignment, which is common in practice.
  FlashWithPartitionFake() : FlashWithPartitionFake(16) {}
  FlashWithPartitionFake(size_t alignment_bytes)
      : memory(alignment_bytes), partition(&memory, 0, memory.sector_count()) {}

  FakeFlashMemoryBuffer<sector_size_bytes, sector_count> memory;
  FlashPartition partition;

 public:
#if DUMP_KVS_STATE_TO_FILE
  Status Dump(const char* filename) {
    std::FILE* out_file = std::fopen(filename, "w+");
    if (out_file == nullptr) {
      PW_LOG_ERROR("Failed to dump to %s", filename);
      return Status::DATA_LOSS;
    }
    std::vector<std::byte> out_vec(memory.size_bytes());
    Status status =
        memory.Read(0, std::span<std::byte>(out_vec.data(), out_vec.size()));
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
  Status Dump(const char*) { return Status::OK; }
#endif  // DUMP_KVS_STATE_TO_FILE
};

typedef FlashWithPartitionFake<4 * 128 /*sector size*/, 6 /*sectors*/> Flash;

FakeFlashMemoryBuffer<1024, 60> large_test_flash(8);
FlashPartition large_test_partition(&large_test_flash,
                                    0,
                                    large_test_flash.sector_count());

constexpr std::array<const char*, 3> keys{"TestKey1", "Key2", "TestKey3"};

ChecksumCrc16 checksum;
// For KVS magic value always use a random 32 bit integer rather than a
// human readable 4 bytes. See pw_kvs/format.h for more information.
constexpr EntryFormat default_format{.magic = 0xa6cb3c16,
                                     .checksum = &checksum};

}  // namespace

TEST(InitCheck, TooFewSectors) {
  // Use test flash with 1 x 4k sectors, 16 byte alignment
  FakeFlashMemoryBuffer<4 * 1024, 1> test_flash(16);
  FlashPartition test_partition(&test_flash, 0, test_flash.sector_count());

  // For KVS magic value always use a random 32 bit integer rather than a
  // human readable 4 bytes. See pw_kvs/format.h for more information.
  constexpr EntryFormat format{.magic = 0x89bb14d2, .checksum = nullptr};
  KeyValueStoreBuffer<kMaxEntries, kMaxUsableSectors> kvs(&test_partition,
                                                          format);

  EXPECT_EQ(kvs.Init(), Status::FAILED_PRECONDITION);
}

TEST(InitCheck, ZeroSectors) {
  // Use test flash with 1 x 4k sectors, 16 byte alignment
  FakeFlashMemoryBuffer<4 * 1024, 1> test_flash(16);

  // Set FlashPartition to have 0 sectors.
  FlashPartition test_partition(&test_flash, 0, 0);

  // For KVS magic value always use a random 32 bit integer rather than a
  // human readable 4 bytes. See pw_kvs/format.h for more information.
  constexpr EntryFormat format{.magic = 0xd1da57c1, .checksum = nullptr};
  KeyValueStoreBuffer<kMaxEntries, kMaxUsableSectors> kvs(&test_partition,
                                                          format);

  EXPECT_EQ(kvs.Init(), Status::FAILED_PRECONDITION);
}

TEST(InitCheck, TooManySectors) {
  // Use test flash with 1 x 4k sectors, 16 byte alignment
  FakeFlashMemoryBuffer<4 * 1024, 5> test_flash(16);

  // Set FlashPartition to have 0 sectors.
  FlashPartition test_partition(&test_flash, 0, test_flash.sector_count());

  // For KVS magic value always use a random 32 bit integer rather than a
  // human readable 4 bytes. See pw_kvs/format.h for more information.
  constexpr EntryFormat format{.magic = 0x610f6d17, .checksum = nullptr};
  KeyValueStoreBuffer<kMaxEntries, 2> kvs(&test_partition, format);

  EXPECT_EQ(kvs.Init(), Status::FAILED_PRECONDITION);
}

#define ASSERT_OK(expr) ASSERT_EQ(Status::OK, expr)
#define EXPECT_OK(expr) EXPECT_EQ(Status::OK, expr)

TEST(InMemoryKvs, WriteOneKeyMultipleTimes) {
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

    // Create and initialize the KVS. For KVS magic value always use a random 32
    // bit integer rather than a human readable 4 bytes. See pw_kvs/format.h for
    // more information.
    constexpr EntryFormat format{.magic = 0x83a9257, .checksum = nullptr};
    KeyValueStoreBuffer<kMaxEntries, kMaxUsableSectors> kvs(&flash.partition,
                                                            format);
    ASSERT_OK(kvs.Init());

    // Write the same entry many times.
    const char* key = "abcd";
    const size_t num_writes = 99;
    uint32_t written_value;
    EXPECT_EQ(kvs.size(), (reload == 0) ? 0 : 1u);
    for (uint32_t i = 0; i < num_writes; ++i) {
      DBG("PUT #%zu for key %s with value %zu", size_t(i), key, size_t(i));

      written_value = i + 0xfc;  // Prevent accidental pass with zero.
      EXPECT_OK(kvs.Put(key, written_value));
      EXPECT_EQ(kvs.size(), 1u);
    }

    // Verify that we can read the value back.
    DBG("GET final value for key: %s", key);
    uint32_t actual_value;
    EXPECT_OK(kvs.Get(key, &actual_value));
    EXPECT_EQ(actual_value, written_value);

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

  // Create and initialize the KVS. For KVS magic value always use a random 32
  // bit integer rather than a human readable 4 bytes. See pw_kvs/format.h for
  // more information.
  constexpr EntryFormat format{.magic = 0x2ed3a058, .checksum = nullptr};
  KeyValueStoreBuffer<kMaxEntries, kMaxUsableSectors> kvs(&flash.partition,
                                                          format);
  ASSERT_OK(kvs.Init());

  // Write the same entry many times.
  const size_t num_writes = 10;
  EXPECT_EQ(kvs.size(), 0u);
  for (size_t i = 0; i < num_writes; ++i) {
    StringBuffer<150> key;
    key << "key_" << i;
    DBG("PUT #%zu for key %s with value %zu", i, key.c_str(), i);

    size_t value = i + 77;  // Prevent accidental pass with zero.
    EXPECT_OK(kvs.Put(key.view(), value));
    EXPECT_EQ(kvs.size(), i + 1);
  }
  flash.Dump("WritingMultipleKeysIncreasesSize.bin");
}

TEST(InMemoryKvs, WriteAndReadOneKey) {
  // Create and erase the fake flash.
  Flash flash;
  ASSERT_OK(flash.partition.Erase());

  // Create and initialize the KVS.
  // For KVS magic value always use a random 32 bit integer rather than a
  // human readable 4 bytes. See pw_kvs/format.h for more information.
  constexpr EntryFormat format{.magic = 0x5d70896, .checksum = nullptr};
  KeyValueStoreBuffer<kMaxEntries, kMaxUsableSectors> kvs(&flash.partition,
                                                          format);
  ASSERT_OK(kvs.Init());

  // Add one entry.
  const char* key = "Key1";
  DBG("PUT value for key: %s", key);
  uint8_t written_value = 0xDA;
  ASSERT_OK(kvs.Put(key, written_value));
  EXPECT_EQ(kvs.size(), 1u);

  DBG("GET value for key: %s", key);
  uint8_t actual_value;
  ASSERT_OK(kvs.Get(key, &actual_value));
  EXPECT_EQ(actual_value, written_value);

  EXPECT_EQ(kvs.size(), 1u);
}

TEST(InMemoryKvs, WriteOneKeyValueMultipleTimes) {
  // Create and erase the fake flash.
  Flash flash;
  ASSERT_OK(flash.partition.Erase());

  // Create and initialize the KVS.
  KeyValueStoreBuffer<kMaxEntries, kMaxUsableSectors> kvs(&flash.partition,
                                                          default_format);
  ASSERT_OK(kvs.Init());

  // Add one entry, with the same key and value, multiple times.
  const char* key = "Key1";
  uint8_t written_value = 0xDA;
  for (int i = 0; i < 50; i++) {
    DBG("PUT [%d] value for key: %s", i, key);
    ASSERT_OK(kvs.Put(key, written_value));
    EXPECT_EQ(kvs.size(), 1u);
  }

  DBG("GET value for key: %s", key);
  uint8_t actual_value;
  ASSERT_OK(kvs.Get(key, &actual_value));
  EXPECT_EQ(actual_value, written_value);

  // Verify that only one entry was written to the KVS.
  EXPECT_EQ(kvs.size(), 1u);
  EXPECT_EQ(kvs.transaction_count(), 1u);
  KeyValueStore::StorageStats stats = kvs.GetStorageStats();
  EXPECT_EQ(stats.reclaimable_bytes, 0u);
}

TEST(InMemoryKvs, Basic) {
  const char* key1 = "Key1";
  const char* key2 = "Key2";

  // Create and erase the fake flash.
  Flash flash;
  ASSERT_EQ(Status::OK, flash.partition.Erase());

  // Create and initialize the KVS.
  // For KVS magic value always use a random 32 bit integer rather than a
  // human readable 4 bytes. See pw_kvs/format.h for more information.
  constexpr EntryFormat format{.magic = 0x7bf19895, .checksum = nullptr};
  KeyValueStoreBuffer<kMaxEntries, kMaxUsableSectors> kvs(&flash.partition,
                                                          format);
  ASSERT_OK(kvs.Init());

  // Add two entries with different keys and values.
  uint8_t value1 = 0xDA;
  ASSERT_OK(kvs.Put(key1, std::as_bytes(std::span(&value1, sizeof(value1)))));
  EXPECT_EQ(kvs.size(), 1u);

  uint32_t value2 = 0xBAD0301f;
  ASSERT_OK(kvs.Put(key2, value2));
  EXPECT_EQ(kvs.size(), 2u);

  // Verify data
  uint32_t test2;
  EXPECT_OK(kvs.Get(key2, &test2));

  uint8_t test1;
  ASSERT_OK(kvs.Get(key1, &test1));

  EXPECT_EQ(test1, value1);
  EXPECT_EQ(test2, value2);

  EXPECT_EQ(kvs.size(), 2u);
}

TEST(InMemoryKvs, CallingEraseTwice_NothingWrittenToFlash) {
  // Create and erase the fake flash.
  Flash flash;
  ASSERT_EQ(Status::OK, flash.partition.Erase());

  // Create and initialize the KVS.
  KeyValueStoreBuffer<kMaxEntries, kMaxUsableSectors> kvs(&flash.partition,
                                                          default_format);
  ASSERT_OK(kvs.Init());

  const uint8_t kValue = 0xDA;
  ASSERT_EQ(Status::OK, kvs.Put(keys[0], kValue));
  ASSERT_EQ(Status::OK, kvs.Delete(keys[0]));

  // Compare before / after checksums to verify that nothing was written.
  const uint16_t crc = checksum::CcittCrc16(flash.memory.buffer());

  EXPECT_EQ(kvs.Delete(keys[0]), Status::NOT_FOUND);

  EXPECT_EQ(crc, checksum::CcittCrc16(flash.memory.buffer()));
}

class LargeEmptyInitializedKvs : public ::testing::Test {
 protected:
  LargeEmptyInitializedKvs() : kvs_(&large_test_partition, default_format) {
    ASSERT_EQ(Status::OK, large_test_partition.Erase());
    ASSERT_EQ(Status::OK, kvs_.Init());
  }

  KeyValueStoreBuffer<kMaxEntries, kMaxUsableSectors> kvs_;
};

TEST_F(LargeEmptyInitializedKvs, Basic) {
  const uint8_t kValue1 = 0xDA;
  const uint8_t kValue2 = 0x12;
  uint8_t value;
  ASSERT_EQ(Status::OK, kvs_.Put(keys[0], kValue1));
  EXPECT_EQ(kvs_.size(), 1u);
  ASSERT_EQ(Status::OK, kvs_.Delete(keys[0]));
  EXPECT_EQ(kvs_.Get(keys[0], &value), Status::NOT_FOUND);
  ASSERT_EQ(Status::OK, kvs_.Put(keys[1], kValue1));
  ASSERT_EQ(Status::OK, kvs_.Put(keys[2], kValue2));
  ASSERT_EQ(Status::OK, kvs_.Delete(keys[1]));
  EXPECT_EQ(Status::OK, kvs_.Get(keys[2], &value));
  EXPECT_EQ(kValue2, value);
  ASSERT_EQ(kvs_.Get(keys[1], &value), Status::NOT_FOUND);
  EXPECT_EQ(kvs_.size(), 1u);
}

TEST(InMemoryKvs, Put_MaxValueSize) {
  // Create and erase the fake flash.
  Flash flash;
  ASSERT_EQ(Status::OK, flash.partition.Erase());

  // Create and initialize the KVS.
  KeyValueStoreBuffer<kMaxEntries, kMaxUsableSectors> kvs(&flash.partition,
                                                          default_format);
  ASSERT_OK(kvs.Init());

  size_t max_key_value_size = kvs.max_key_value_size_bytes();
  EXPECT_EQ(max_key_value_size,
            KeyValueStore::max_key_value_size_bytes(
                flash.partition.sector_size_bytes()));

  size_t max_value_size =
      flash.partition.sector_size_bytes() - sizeof(EntryHeader) - 1;
  EXPECT_EQ(max_key_value_size, (max_value_size + 1));

  // Use the large_test_flash as a big chunk of data for the Put statement.
  ASSERT_GT(sizeof(large_test_flash), max_value_size + 2 * sizeof(EntryHeader));
  auto big_data = std::as_bytes(std::span(&large_test_flash, 1));

  EXPECT_EQ(Status::OK, kvs.Put("K", big_data.subspan(0, max_value_size)));

  // Larger than maximum is rejected.
  EXPECT_EQ(Status::INVALID_ARGUMENT,
            kvs.Put("K", big_data.subspan(0, max_value_size + 1)));
  EXPECT_EQ(Status::INVALID_ARGUMENT, kvs.Put("K", big_data));
}

}  // namespace pw::kvs
