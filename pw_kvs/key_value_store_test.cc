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

#if DUMP_KVS_STATE_TO_FILE
#include <vector>
#endif  // DUMP_KVS_STATE_TO_FILE

#include "gtest/gtest.h"
#include "pw_checksum/ccitt_crc16.h"
#include "pw_kvs/crc16_checksum.h"
#include "pw_kvs/flash_memory.h"
#include "pw_kvs/internal/entry.h"
#include "pw_kvs_private/byte_utils.h"
#include "pw_kvs_private/macros.h"
#include "pw_log/log.h"
#include "pw_span/span.h"
#include "pw_status/status.h"
#include "pw_string/string_builder.h"

#if USE_MEMORY_BUFFER
#include "pw_kvs/fake_flash_memory.h"
#endif  // USE_MEMORY_BUFFER

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

static_assert(ConvertsToSpan<bool[1]>());
static_assert(ConvertsToSpan<char[35]>());
static_assert(ConvertsToSpan<const int[35]>());

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
  Status Dump(const char*) { return Status::OK; }
#endif  // DUMP_KVS_STATE_TO_FILE
};

typedef FlashWithPartitionFake<4 * 128 /*sector size*/, 6 /*sectors*/> Flash;

#if USE_MEMORY_BUFFER
// Although it might be useful to test other configurations, some tests require
// at least 3 sectors; therfore it should have this when checked in.
FakeFlashMemoryBuffer<4 * 1024, 6> test_flash(
    16);  // 4 x 4k sectors, 16 byte alignment
FlashPartition test_partition(&test_flash, 0, test_flash.sector_count());
FakeFlashMemoryBuffer<1024, 60> large_test_flash(8);
FlashPartition large_test_partition(&large_test_flash,
                                    0,
                                    large_test_flash.sector_count());
#else  // TODO: Test with real flash
FlashPartition& test_partition = FlashExternalTestPartition();
#endif  // USE_MEMORY_BUFFER

std::array<byte, 512> buffer;
constexpr std::array<const char*, 3> keys{"TestKey1", "Key2", "TestKey3"};

ChecksumCrc16 checksum;
constexpr EntryFormat default_format{.magic = 0xBAD'C0D3,
                                     .checksum = &checksum};

size_t RoundUpForAlignment(size_t size) {
  return AlignUp(size, test_partition.alignment_bytes());
}

// This class gives attributes of KVS that we are testing against
class KvsAttributes {
 public:
  KvsAttributes(size_t key_size, size_t data_size)
      : chunk_header_size_(RoundUpForAlignment(sizeof(EntryHeader))),
        data_size_(RoundUpForAlignment(data_size)),
        key_size_(RoundUpForAlignment(key_size)),
        erase_size_(chunk_header_size_ + key_size_),
        min_put_size_(
            RoundUpForAlignment(chunk_header_size_ + key_size_ + data_size_)) {}

  size_t ChunkHeaderSize() { return chunk_header_size_; }
  size_t DataSize() { return data_size_; }
  size_t KeySize() { return key_size_; }
  size_t EraseSize() { return erase_size_; }
  size_t MinPutSize() { return min_put_size_; }

 private:
  const size_t chunk_header_size_;
  const size_t data_size_;
  const size_t key_size_;
  const size_t erase_size_;
  const size_t min_put_size_;
};

class EmptyInitializedKvs : public ::testing::Test {
 protected:
  EmptyInitializedKvs() : kvs_(&test_partition, default_format) {
    test_partition.Erase();
    ASSERT_EQ(Status::OK, kvs_.Init());
  }

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
                kvs_.Put(key,
                         span(buffer.data(),
                              chunk_len - kvs_attr.ChunkHeaderSize() -
                                  kvs_attr.KeySize())));
      size_to_fill -= chunk_len;
      chunk_len = std::min(size_to_fill, kMaxPutSize);
    }
    ASSERT_EQ(Status::OK, kvs_.Delete(key));
  }

  KeyValueStoreBuffer<kMaxEntries, kMaxUsableSectors> kvs_;
};

}  // namespace

TEST_F(EmptyInitializedKvs, Put_SameKeySameValueRepeatedly_AlignedEntries) {
  std::array<char, 8> value{'v', 'a', 'l', 'u', 'e', '6', '7', '\0'};

  for (int i = 0; i < 1000; ++i) {
    ASSERT_EQ(Status::OK, kvs_.Put("The Key!", as_bytes(span(value))));
  }
}

TEST_F(EmptyInitializedKvs, Put_SameKeySameValueRepeatedly_UnalignedEntries) {
  std::array<char, 7> value{'v', 'a', 'l', 'u', 'e', '6', '\0'};

  for (int i = 0; i < 1000; ++i) {
    ASSERT_EQ(Status::OK, kvs_.Put("The Key!", as_bytes(span(value))));
  }
}

TEST_F(EmptyInitializedKvs, Put_SameKeyDifferentValuesRepeatedly) {
  std::array<char, 10> value{'v', 'a', 'l', 'u', 'e', '6', '7', '8', '9', '\0'};

  for (int i = 0; i < 100; ++i) {
    for (unsigned size = 0; size < value.size(); ++size) {
      ASSERT_EQ(Status::OK, kvs_.Put("The Key!", i));
    }
  }
}

TEST_F(EmptyInitializedKvs, Put_MaxValueSize) {
  size_t max_value_size =
      test_partition.sector_size_bytes() - sizeof(EntryHeader) - 1;

  // Use the large_test_flash as a big chunk of data for the Put statement.
  ASSERT_GT(sizeof(large_test_flash), max_value_size + 2 * sizeof(EntryHeader));
  auto big_data = as_bytes(span(&large_test_flash, 1));

  EXPECT_EQ(Status::OK, kvs_.Put("K", big_data.subspan(0, max_value_size)));

  // Larger than maximum is rejected.
  EXPECT_EQ(Status::INVALID_ARGUMENT,
            kvs_.Put("K", big_data.subspan(0, max_value_size + 1)));
  EXPECT_EQ(Status::INVALID_ARGUMENT, kvs_.Put("K", big_data));
}

TEST_F(EmptyInitializedKvs, PutAndGetByValue_ConvertibleToSpan) {
  constexpr float input[] = {1.0, -3.5};
  ASSERT_EQ(Status::OK, kvs_.Put("key", input));

  float output[2] = {};
  ASSERT_EQ(Status::OK, kvs_.Get("key", &output));
  EXPECT_EQ(input[0], output[0]);
  EXPECT_EQ(input[1], output[1]);
}

TEST_F(EmptyInitializedKvs, PutAndGetByValue_Span) {
  float input[] = {1.0, -3.5};
  ASSERT_EQ(Status::OK, kvs_.Put("key", span(input)));

  float output[2] = {};
  ASSERT_EQ(Status::OK, kvs_.Get("key", &output));
  EXPECT_EQ(input[0], output[0]);
  EXPECT_EQ(input[1], output[1]);
}

TEST_F(EmptyInitializedKvs, PutAndGetByValue_NotConvertibleToSpan) {
  struct TestStruct {
    float a;
    bool b;
  };
  const TestStruct input{-1234.5, true};

  ASSERT_EQ(Status::OK, kvs_.Put("key", input));

  TestStruct output;
  ASSERT_EQ(Status::OK, kvs_.Get("key", &output));
  EXPECT_EQ(input.a, output.a);
  EXPECT_EQ(input.b, output.b);
}

TEST_F(EmptyInitializedKvs, Get_Simple) {
  ASSERT_EQ(Status::OK, kvs_.Put("Charles", as_bytes(span("Mingus"))));

  char value[16];
  auto result = kvs_.Get("Charles", as_writable_bytes(span(value)));
  EXPECT_EQ(Status::OK, result.status());
  EXPECT_EQ(sizeof("Mingus"), result.size());
  EXPECT_STREQ("Mingus", value);
}

TEST_F(EmptyInitializedKvs, Get_WithOffset) {
  ASSERT_EQ(Status::OK, kvs_.Put("Charles", as_bytes(span("Mingus"))));

  char value[16];
  auto result = kvs_.Get("Charles", as_writable_bytes(span(value)), 4);
  EXPECT_EQ(Status::OK, result.status());
  EXPECT_EQ(sizeof("Mingus") - 4, result.size());
  EXPECT_STREQ("us", value);
}

TEST_F(EmptyInitializedKvs, Get_WithOffset_FillBuffer) {
  ASSERT_EQ(Status::OK, kvs_.Put("Charles", as_bytes(span("Mingus"))));

  char value[4] = {};
  auto result = kvs_.Get("Charles", as_writable_bytes(span(value, 3)), 1);
  EXPECT_EQ(Status::RESOURCE_EXHAUSTED, result.status());
  EXPECT_EQ(3u, result.size());
  EXPECT_STREQ("ing", value);
}

TEST_F(EmptyInitializedKvs, Get_WithOffset_PastEnd) {
  ASSERT_EQ(Status::OK, kvs_.Put("Charles", as_bytes(span("Mingus"))));

  char value[16];
  auto result =
      kvs_.Get("Charles", as_writable_bytes(span(value)), sizeof("Mingus") + 1);
  EXPECT_EQ(Status::OUT_OF_RANGE, result.status());
  EXPECT_EQ(0u, result.size());
}

TEST_F(EmptyInitializedKvs, GetValue) {
  ASSERT_EQ(Status::OK, kvs_.Put("key", uint32_t(0xfeedbeef)));

  uint32_t value = 0;
  EXPECT_EQ(Status::OK, kvs_.Get("key", &value));
  EXPECT_EQ(uint32_t(0xfeedbeef), value);
}

TEST_F(EmptyInitializedKvs, GetValue_TooSmall) {
  ASSERT_EQ(Status::OK, kvs_.Put("key", uint32_t(0xfeedbeef)));

  uint8_t value = 0;
  EXPECT_EQ(Status::INVALID_ARGUMENT, kvs_.Get("key", &value));
  EXPECT_EQ(0u, value);
}

TEST_F(EmptyInitializedKvs, GetValue_TooLarge) {
  ASSERT_EQ(Status::OK, kvs_.Put("key", uint32_t(0xfeedbeef)));

  uint64_t value = 0;
  EXPECT_EQ(Status::INVALID_ARGUMENT, kvs_.Get("key", &value));
  EXPECT_EQ(0u, value);
}

TEST_F(EmptyInitializedKvs, Delete_GetDeletedKey_ReturnsNotFound) {
  ASSERT_EQ(Status::OK, kvs_.Put("kEy", as_bytes(span("123"))));
  ASSERT_EQ(Status::OK, kvs_.Delete("kEy"));

  EXPECT_EQ(Status::NOT_FOUND, kvs_.Get("kEy", {}).status());
  EXPECT_EQ(Status::NOT_FOUND, kvs_.ValueSize("kEy").status());
}

TEST_F(EmptyInitializedKvs, Delete_AddBackKey_PersistsAfterInitialization) {
  ASSERT_EQ(Status::OK, kvs_.Put("kEy", as_bytes(span("123"))));
  ASSERT_EQ(Status::OK, kvs_.Delete("kEy"));

  EXPECT_EQ(Status::OK, kvs_.Put("kEy", as_bytes(span("45678"))));
  char data[6] = {};
  ASSERT_EQ(Status::OK, kvs_.Get("kEy", &data));
  EXPECT_STREQ(data, "45678");

  // Ensure that the re-added key is still present after reinitialization.
  KeyValueStoreBuffer<kMaxEntries, kMaxUsableSectors> new_kvs(&test_partition,
                                                              default_format);
  ASSERT_EQ(Status::OK, new_kvs.Init());

  EXPECT_EQ(Status::OK, new_kvs.Put("kEy", as_bytes(span("45678"))));
  char new_data[6] = {};
  EXPECT_EQ(Status::OK, new_kvs.Get("kEy", &new_data));
  EXPECT_STREQ(data, "45678");
}

TEST_F(EmptyInitializedKvs, Delete_AllItems_KvsIsEmpty) {
  ASSERT_EQ(Status::OK, kvs_.Put("kEy", as_bytes(span("123"))));
  ASSERT_EQ(Status::OK, kvs_.Delete("kEy"));

  EXPECT_EQ(0u, kvs_.size());
  EXPECT_TRUE(kvs_.empty());
}

TEST_F(EmptyInitializedKvs, Collision_WithPresentKey) {
  // Both hash to 0x19df36f0.
  constexpr std::string_view key1 = "D4";
  constexpr std::string_view key2 = "dFU6S";

  ASSERT_EQ(Status::OK, kvs_.Put(key1, 1000));

  EXPECT_EQ(Status::ALREADY_EXISTS, kvs_.Put(key2, 999));

  int value = 0;
  EXPECT_EQ(Status::OK, kvs_.Get(key1, &value));
  EXPECT_EQ(1000, value);

  EXPECT_EQ(Status::NOT_FOUND, kvs_.Get(key2, &value));
  EXPECT_EQ(Status::NOT_FOUND, kvs_.ValueSize(key2).status());
  EXPECT_EQ(Status::NOT_FOUND, kvs_.Delete(key2));
}

TEST_F(EmptyInitializedKvs, Collision_WithDeletedKey) {
  // Both hash to 0x4060f732.
  constexpr std::string_view key1 = "1U2";
  constexpr std::string_view key2 = "ahj9d";

  ASSERT_EQ(Status::OK, kvs_.Put(key1, 1000));
  ASSERT_EQ(Status::OK, kvs_.Delete(key1));

  // key2 collides with key1's tombstone.
  EXPECT_EQ(Status::ALREADY_EXISTS, kvs_.Put(key2, 999));

  int value = 0;
  EXPECT_EQ(Status::NOT_FOUND, kvs_.Get(key1, &value));

  EXPECT_EQ(Status::NOT_FOUND, kvs_.Get(key2, &value));
  EXPECT_EQ(Status::NOT_FOUND, kvs_.ValueSize(key2).status());
  EXPECT_EQ(Status::NOT_FOUND, kvs_.Delete(key2));
}

TEST_F(EmptyInitializedKvs, Iteration_Empty_ByReference) {
  for (const KeyValueStore::Item& entry : kvs_) {
    FAIL();  // The KVS is empty; this shouldn't execute.
    static_cast<void>(entry);
  }
}

TEST_F(EmptyInitializedKvs, Iteration_Empty_ByValue) {
  for (KeyValueStore::Item entry : kvs_) {
    FAIL();  // The KVS is empty; this shouldn't execute.
    static_cast<void>(entry);
  }
}

TEST_F(EmptyInitializedKvs, Iteration_OneItem) {
  ASSERT_EQ(Status::OK, kvs_.Put("kEy", as_bytes(span("123"))));

  for (KeyValueStore::Item entry : kvs_) {
    EXPECT_STREQ(entry.key(), "kEy");  // Make sure null-terminated.

    char temp[sizeof("123")] = {};
    EXPECT_EQ(Status::OK, entry.Get(&temp));
    EXPECT_STREQ("123", temp);
  }
}

TEST_F(EmptyInitializedKvs, Iteration_GetWithOffset) {
  ASSERT_EQ(Status::OK, kvs_.Put("key", as_bytes(span("not bad!"))));

  for (KeyValueStore::Item entry : kvs_) {
    char temp[5];
    auto result = entry.Get(as_writable_bytes(span(temp)), 4);
    EXPECT_EQ(Status::OK, result.status());
    EXPECT_EQ(5u, result.size());
    EXPECT_STREQ("bad!", temp);
  }
}

TEST_F(EmptyInitializedKvs, Iteration_GetValue) {
  ASSERT_EQ(Status::OK, kvs_.Put("key", uint32_t(0xfeedbeef)));

  for (KeyValueStore::Item entry : kvs_) {
    uint32_t value = 0;
    EXPECT_EQ(Status::OK, entry.Get(&value));
    EXPECT_EQ(uint32_t(0xfeedbeef), value);
  }
}

TEST_F(EmptyInitializedKvs, Iteration_GetValue_TooSmall) {
  ASSERT_EQ(Status::OK, kvs_.Put("key", uint32_t(0xfeedbeef)));

  for (KeyValueStore::Item entry : kvs_) {
    uint8_t value = 0;
    EXPECT_EQ(Status::INVALID_ARGUMENT, entry.Get(&value));
    EXPECT_EQ(0u, value);
  }
}

TEST_F(EmptyInitializedKvs, Iteration_GetValue_TooLarge) {
  ASSERT_EQ(Status::OK, kvs_.Put("key", uint32_t(0xfeedbeef)));

  for (KeyValueStore::Item entry : kvs_) {
    uint64_t value = 0;
    EXPECT_EQ(Status::INVALID_ARGUMENT, entry.Get(&value));
    EXPECT_EQ(0u, value);
  }
}

TEST_F(EmptyInitializedKvs, Iteration_EmptyAfterDeletion) {
  ASSERT_EQ(Status::OK, kvs_.Put("kEy", as_bytes(span("123"))));
  ASSERT_EQ(Status::OK, kvs_.Delete("kEy"));

  for (KeyValueStore::Item entry : kvs_) {
    static_cast<void>(entry);
    FAIL();
  }
}

TEST_F(EmptyInitializedKvs, FuzzTest) {
  if (test_partition.sector_size_bytes() < 4 * 1024 ||
      test_partition.sector_count() < 4) {
    PW_LOG_INFO("Sectors too small, skipping test.");
    return;  // TODO: Test could be generalized
  }
  const char* key1 = "Buf1";
  const char* key2 = "Buf2";
  const size_t kLargestBufSize = 3 * 1024;
  static byte buf1[kLargestBufSize];
  static byte buf2[kLargestBufSize];
  std::memset(buf1, 1, sizeof(buf1));
  std::memset(buf2, 2, sizeof(buf2));

  // Start with things in KVS
  ASSERT_EQ(Status::OK, kvs_.Put(key1, buf1));
  ASSERT_EQ(Status::OK, kvs_.Put(key2, buf2));
  for (size_t j = 0; j < keys.size(); j++) {
    ASSERT_EQ(Status::OK, kvs_.Put(keys[j], j));
  }

  for (size_t i = 0; i < 100; i++) {
    // Vary two sizes
    size_t size1 = (kLargestBufSize) / (i + 1);
    size_t size2 = (kLargestBufSize) / (100 - i);
    for (size_t j = 0; j < 50; j++) {
      // Rewrite a single key many times, can fill up a sector
      ASSERT_EQ(Status::OK, kvs_.Put("some_data", j));
    }
    // Delete and re-add everything
    ASSERT_EQ(Status::OK, kvs_.Delete(key1));
    ASSERT_EQ(Status::OK, kvs_.Put(key1, span(buf1, size1)));
    ASSERT_EQ(Status::OK, kvs_.Delete(key2));
    ASSERT_EQ(Status::OK, kvs_.Put(key2, span(buf2, size2)));
    for (size_t j = 0; j < keys.size(); j++) {
      ASSERT_EQ(Status::OK, kvs_.Delete(keys[j]));
      ASSERT_EQ(Status::OK, kvs_.Put(keys[j], j));
    }

    // Re-enable and verify
    ASSERT_EQ(Status::OK, kvs_.Init());
    static byte buf[4 * 1024];
    ASSERT_EQ(Status::OK, kvs_.Get(key1, span(buf, size1)).status());
    ASSERT_EQ(std::memcmp(buf, buf1, size1), 0);
    ASSERT_EQ(Status::OK, kvs_.Get(key2, span(buf, size2)).status());
    ASSERT_EQ(std::memcmp(buf2, buf2, size2), 0);
    for (size_t j = 0; j < keys.size(); j++) {
      size_t ret = 1000;
      ASSERT_EQ(Status::OK, kvs_.Get(keys[j], &ret));
      ASSERT_EQ(ret, j);
    }
  }
}

TEST_F(EmptyInitializedKvs, Basic) {
  // Add some data
  uint8_t value1 = 0xDA;
  ASSERT_EQ(Status::OK,
            kvs_.Put(keys[0], as_bytes(span(&value1, sizeof(value1)))));

  uint32_t value2 = 0xBAD0301f;
  ASSERT_EQ(Status::OK, kvs_.Put(keys[1], value2));

  // Verify data
  uint32_t test2;
  EXPECT_EQ(Status::OK, kvs_.Get(keys[1], &test2));
  uint8_t test1;
  ASSERT_EQ(Status::OK, kvs_.Get(keys[0], &test1));

  EXPECT_EQ(test1, value1);
  EXPECT_EQ(test2, value2);

  // Delete a key
  EXPECT_EQ(Status::OK, kvs_.Delete(keys[0]));

  // Verify it was erased
  EXPECT_EQ(kvs_.Get(keys[0], &test1), Status::NOT_FOUND);
  test2 = 0;
  ASSERT_EQ(
      Status::OK,
      kvs_.Get(keys[1], span(reinterpret_cast<byte*>(&test2), sizeof(test2)))
          .status());
  EXPECT_EQ(test2, value2);

  // Delete other key
  kvs_.Delete(keys[1]);

  // Verify it was erased
  EXPECT_EQ(kvs_.size(), 0u);
}

TEST(InitCheck, TooFewSectors) {
  // Use test flash with 1 x 4k sectors, 16 byte alignment
  FakeFlashMemoryBuffer<4 * 1024, 1> test_flash(16);
  FlashPartition test_partition(&test_flash, 0, test_flash.sector_count());

  constexpr EntryFormat format{.magic = 0xBAD'C0D3, .checksum = nullptr};
  KeyValueStoreBuffer<kMaxEntries, kMaxUsableSectors> kvs(&test_partition,
                                                          format);

  EXPECT_EQ(kvs.Init(), Status::FAILED_PRECONDITION);
}

TEST(InitCheck, ZeroSectors) {
  // Use test flash with 1 x 4k sectors, 16 byte alignment
  FakeFlashMemoryBuffer<4 * 1024, 1> test_flash(16);

  // Set FlashPartition to have 0 sectors.
  FlashPartition test_partition(&test_flash, 0, 0);

  constexpr EntryFormat format{.magic = 0xBAD'C0D3, .checksum = nullptr};
  KeyValueStoreBuffer<kMaxEntries, kMaxUsableSectors> kvs(&test_partition,
                                                          format);

  EXPECT_EQ(kvs.Init(), Status::FAILED_PRECONDITION);
}

TEST(InitCheck, TooManySectors) {
  // Use test flash with 1 x 4k sectors, 16 byte alignment
  FakeFlashMemoryBuffer<4 * 1024, 5> test_flash(16);

  // Set FlashPartition to have 0 sectors.
  FlashPartition test_partition(&test_flash, 0, test_flash.sector_count());

  constexpr EntryFormat format{.magic = 0xBAD'C0D3, .checksum = nullptr};
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

    // Create and initialize the KVS.
    constexpr EntryFormat format{.magic = 0xBAD'C0D3, .checksum = nullptr};
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

  // Create and initialize the KVS.
  constexpr EntryFormat format{.magic = 0xBAD'C0D3, .checksum = nullptr};
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
  constexpr EntryFormat format{.magic = 0xBAD'C0D3, .checksum = nullptr};
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
  constexpr EntryFormat format{.magic = 0xBAD'C0D3, .checksum = nullptr};
  KeyValueStoreBuffer<kMaxEntries, kMaxUsableSectors> kvs(&flash.partition,
                                                          format);
  ASSERT_OK(kvs.Init());

  // Add two entries with different keys and values.
  uint8_t value1 = 0xDA;
  ASSERT_OK(kvs.Put(key1, as_bytes(span(&value1, sizeof(value1)))));
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

TEST_F(EmptyInitializedKvs, MaxKeyLength) {
  // Add some data
  char key[16] = "123456789abcdef";  // key length 15 (without \0)
  int value = 1;
  ASSERT_EQ(Status::OK, kvs_.Put(key, value));

  // Verify data
  int test = 0;
  ASSERT_EQ(Status::OK, kvs_.Get(key, &test));
  EXPECT_EQ(test, value);

  // Delete a key
  EXPECT_EQ(Status::OK, kvs_.Delete(key));

  // Verify it was erased
  EXPECT_EQ(kvs_.Get(key, &test), Status::NOT_FOUND);
}

TEST_F(EmptyInitializedKvs, LargeBuffers) {
  // Note this assumes that no other keys larger then key0
  static_assert(sizeof(keys[0]) >= sizeof(keys[1]) &&
                sizeof(keys[0]) >= sizeof(keys[2]));
  KvsAttributes kvs_attr(std::strlen(keys[0]), buffer.size());

  // Verify the data will fit in this test partition. This checks that all the
  // keys chunks will fit and a header for each sector will fit. It requires 1
  // empty sector also.
  const size_t kMinSize = kvs_attr.MinPutSize() * keys.size();
  const size_t kAvailSectorSpace =
      test_partition.sector_size_bytes() * (test_partition.sector_count() - 1);
  if (kAvailSectorSpace < kMinSize) {
    PW_LOG_INFO("KVS too small, skipping test.");
    return;
  }

  // Add and verify
  for (unsigned add_idx = 0; add_idx < keys.size(); add_idx++) {
    std::memset(buffer.data(), add_idx, buffer.size());
    ASSERT_EQ(Status::OK, kvs_.Put(keys[add_idx], buffer));
    EXPECT_EQ(kvs_.size(), add_idx + 1);
    for (unsigned verify_idx = 0; verify_idx <= add_idx; verify_idx++) {
      std::memset(buffer.data(), 0, buffer.size());
      ASSERT_EQ(Status::OK, kvs_.Get(keys[verify_idx], buffer).status());
      for (unsigned i = 0; i < buffer.size(); i++) {
        EXPECT_EQ(static_cast<unsigned>(buffer[i]), verify_idx);
      }
    }
  }

  // Erase and verify
  for (unsigned erase_idx = 0; erase_idx < keys.size(); erase_idx++) {
    ASSERT_EQ(Status::OK, kvs_.Delete(keys[erase_idx]));
    EXPECT_EQ(kvs_.size(), keys.size() - erase_idx - 1);
    for (unsigned verify_idx = 0; verify_idx < keys.size(); verify_idx++) {
      std::memset(buffer.data(), 0, buffer.size());
      if (verify_idx <= erase_idx) {
        ASSERT_EQ(kvs_.Get(keys[verify_idx], buffer).status(),
                  Status::NOT_FOUND);
      } else {
        ASSERT_EQ(Status::OK, kvs_.Get(keys[verify_idx], buffer).status());
        for (uint32_t i = 0; i < buffer.size(); i++) {
          EXPECT_EQ(buffer[i], static_cast<byte>(verify_idx));
        }
      }
    }
  }
}

TEST_F(EmptyInitializedKvs, Enable) {
  KvsAttributes kvs_attr(std::strlen(keys[0]), buffer.size());

  // Verify the data will fit in this test partition. This checks that all the
  // keys chunks will fit and a header for each sector will fit. It requires 1
  // empty sector also.
  const size_t kMinSize = kvs_attr.MinPutSize() * keys.size();
  const size_t kAvailSectorSpace =
      test_partition.sector_size_bytes() * (test_partition.sector_count() - 1);
  if (kAvailSectorSpace < kMinSize) {
    PW_LOG_INFO("KVS too small, skipping test.");
    return;
  }

  // Add some items
  for (unsigned add_idx = 0; add_idx < keys.size(); add_idx++) {
    std::memset(buffer.data(), add_idx, buffer.size());
    ASSERT_EQ(Status::OK, kvs_.Put(keys[add_idx], buffer));
    EXPECT_EQ(kvs_.size(), add_idx + 1);
  }

  // Enable different KVS which should be able to properly setup the same map
  // from what is stored in flash.
  static KeyValueStoreBuffer<kMaxEntries, kMaxUsableSectors> kvs_local(
      &test_partition, default_format);
  ASSERT_EQ(Status::OK, kvs_local.Init());
  EXPECT_EQ(kvs_local.size(), keys.size());

  // Ensure adding to new KVS works
  uint8_t value = 0xDA;
  const char* key = "new_key";
  ASSERT_EQ(Status::OK, kvs_local.Put(key, value));
  uint8_t test;
  ASSERT_EQ(Status::OK, kvs_local.Get(key, &test));
  EXPECT_EQ(value, test);
  EXPECT_EQ(kvs_local.size(), keys.size() + 1);

  // Verify previous data
  for (unsigned verify_idx = 0; verify_idx < keys.size(); verify_idx++) {
    std::memset(buffer.data(), 0, buffer.size());
    ASSERT_EQ(Status::OK, kvs_local.Get(keys[verify_idx], buffer).status());
    for (uint32_t i = 0; i < buffer.size(); i++) {
      EXPECT_EQ(static_cast<unsigned>(buffer[i]), verify_idx);
    }
  }
}

TEST_F(EmptyInitializedKvs, MultiSector) {
  // Calculate number of elements to ensure multiple sectors are required.
  uint16_t add_count = (test_partition.sector_size_bytes() / buffer.size()) + 1;

  if (kvs_.max_size() < add_count) {
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
    ASSERT_EQ(Status::OK, kvs_.Put(key, buffer));
    EXPECT_EQ(kvs_.size(), add_idx + 1);
  }

  for (unsigned verify_idx = 0; verify_idx < add_count; verify_idx++) {
    std::memset(buffer.data(), 0, buffer.size());
    snprintf(key, sizeof(key), "key_%u", verify_idx);
    ASSERT_EQ(Status::OK, kvs_.Get(key, buffer).status());
    for (uint32_t i = 0; i < buffer.size(); i++) {
      EXPECT_EQ(static_cast<unsigned>(buffer[i]), verify_idx);
    }
  }

  // Check erase
  for (unsigned erase_idx = 0; erase_idx < add_count; erase_idx++) {
    snprintf(key, sizeof(key), "key_%u", erase_idx);
    ASSERT_EQ(Status::OK, kvs_.Delete(key));
    EXPECT_EQ(kvs_.size(), add_count - erase_idx - 1);
  }
}

TEST_F(EmptyInitializedKvs, RewriteValue) {
  // Write first value
  const uint8_t kValue1 = 0xDA;
  const uint8_t kValue2 = 0x12;
  const char* key = "the_key";
  ASSERT_EQ(Status::OK, kvs_.Put(key, as_bytes(span(&kValue1, 1))));

  // Verify
  uint8_t value;
  ASSERT_EQ(Status::OK,
            kvs_.Get(key, as_writable_bytes(span(&value, 1))).status());
  EXPECT_EQ(kValue1, value);

  // Write new value for key
  ASSERT_EQ(Status::OK, kvs_.Put(key, as_bytes(span(&kValue2, 1))));

  // Verify
  ASSERT_EQ(Status::OK,
            kvs_.Get(key, as_writable_bytes(span(&value, 1))).status());
  EXPECT_EQ(kValue2, value);

  // Verify only 1 element exists
  EXPECT_EQ(kvs_.size(), 1u);
}

TEST_F(EmptyInitializedKvs, RepeatingValueWithOtherData) {
  std::byte set_buf[150];
  std::byte get_buf[sizeof(set_buf)];

  for (size_t set_index = 0; set_index < sizeof(set_buf); set_index++) {
    set_buf[set_index] = static_cast<std::byte>(set_index);
  }

  StatusWithSize result;

  // Test setting the same entry 10 times but varying the amount of data
  // that is already in env before each test
  for (size_t test_iteration = 0; test_iteration < sizeof(set_buf);
       test_iteration++) {
    // TOD0: Add KVS erase
    // Add a constant unchanging entry so that the updates are not
    // the only entries in the env.  The size of this initial entry
    // we vary between no bytes to sizeof(set_buf).
    ASSERT_EQ(Status::OK,
              kvs_.Put("const_entry", span(set_buf, test_iteration)));

    // The value we read back should be the last value we set
    std::memset(get_buf, 0, sizeof(get_buf));
    result = kvs_.Get("const_entry", span(get_buf));
    ASSERT_EQ(Status::OK, result.status());
    ASSERT_EQ(result.size(), test_iteration);
    for (size_t j = 0; j < test_iteration; j++) {
      EXPECT_EQ(set_buf[j], get_buf[j]);
    }

    // Update the test entry 5 times
    static_assert(sizeof(std::byte) == sizeof(uint8_t));
    uint8_t set_entry_buf[]{1, 2, 3, 4, 5, 6, 7, 8};
    std::byte* set_entry = reinterpret_cast<std::byte*>(set_entry_buf);
    std::byte get_entry_buf[sizeof(set_entry_buf)];
    for (size_t i = 0; i < 5; i++) {
      set_entry[0] = static_cast<std::byte>(i);
      ASSERT_EQ(Status::OK,
                kvs_.Put("test_entry", span(set_entry, sizeof(set_entry_buf))));
      std::memset(get_entry_buf, 0, sizeof(get_entry_buf));
      result = kvs_.Get("test_entry", span(get_entry_buf));
      ASSERT_TRUE(result.ok());
      ASSERT_EQ(result.size(), sizeof(get_entry_buf));
      for (uint32_t j = 0; j < sizeof(set_entry_buf); j++) {
        EXPECT_EQ(set_entry[j], get_entry_buf[j]);
      }
    }

    // Check that the const entry is still present and has the right value
    std::memset(get_buf, 0, sizeof(get_buf));
    result = kvs_.Get("const_entry", span(get_buf));
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.size(), test_iteration);
    for (size_t j = 0; j < test_iteration; j++) {
      EXPECT_EQ(set_buf[j], get_buf[j]);
    }
  }
}

TEST_F(EmptyInitializedKvs, OffsetRead) {
  const char* key = "the_key";
  constexpr size_t kReadSize = 16;  // needs to be a multiple of alignment
  constexpr size_t kTestBufferSize = kReadSize * 10;
  ASSERT_GT(buffer.size(), kTestBufferSize);
  ASSERT_LE(kTestBufferSize, 0xFFu);

  // Write the entire buffer
  for (size_t i = 0; i < kTestBufferSize; i++) {
    buffer[i] = byte(i);
  }
  ASSERT_EQ(Status::OK, kvs_.Put(key, span(buffer.data(), kTestBufferSize)));
  EXPECT_EQ(kvs_.size(), 1u);

  // Read in small chunks and verify
  for (unsigned i = 0; i < kTestBufferSize / kReadSize; i++) {
    std::memset(buffer.data(), 0, buffer.size());
    StatusWithSize result =
        kvs_.Get(key, span(buffer.data(), kReadSize), i * kReadSize);

    ASSERT_EQ(kReadSize, result.size());

    // Only last iteration is OK since all remaining data was read.
    if (i == kTestBufferSize / kReadSize - 1) {
      ASSERT_EQ(Status::OK, result.status());
    } else {  // RESOURCE_EXHAUSTED, since there is still data to read.
      ASSERT_EQ(Status::RESOURCE_EXHAUSTED, result.status());
    }

    for (unsigned j = 0; j < kReadSize; j++) {
      ASSERT_EQ(static_cast<unsigned>(buffer[j]), j + i * kReadSize);
    }
  }
}

TEST_F(EmptyInitializedKvs, MultipleRewrite) {
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
    ASSERT_EQ(Status::OK, kvs_.Put(key, buffer));
    EXPECT_EQ(kvs_.size(), 1u);
  }

  // Verify
  std::memset(buffer.data(), 0, buffer.size());
  ASSERT_EQ(Status::OK, kvs_.Get(key, buffer).status());
  for (uint32_t i = 0; i < buffer.size(); i++) {
    ASSERT_EQ(buffer[i], static_cast<byte>(kGoodVal));
  }
}

TEST_F(EmptyInitializedKvs, FillSector) {
  ASSERT_EQ(std::strlen(keys[0]), 8U);  // Easier for alignment
  ASSERT_EQ(std::strlen(keys[2]), 8U);  // Easier for alignment
  constexpr size_t kTestDataSize = 8;
  KvsAttributes kvs_attr(std::strlen(keys[2]), kTestDataSize);
  int bytes_remaining = test_partition.sector_size_bytes();
  constexpr byte kKey0Pattern = byte{0xBA};

  std::memset(
      buffer.data(), static_cast<int>(kKey0Pattern), kvs_attr.DataSize());
  ASSERT_EQ(Status::OK,
            kvs_.Put(keys[0], span(buffer.data(), kvs_attr.DataSize())));
  bytes_remaining -= kvs_attr.MinPutSize();
  std::memset(buffer.data(), 1, kvs_attr.DataSize());
  ASSERT_EQ(Status::OK,
            kvs_.Put(keys[2], span(buffer.data(), kvs_attr.DataSize())));
  bytes_remaining -= kvs_attr.MinPutSize();
  EXPECT_EQ(kvs_.size(), 2u);
  ASSERT_EQ(Status::OK, kvs_.Delete(keys[2]));
  bytes_remaining -= kvs_attr.EraseSize();
  EXPECT_EQ(kvs_.size(), 1u);

  // Intentionally adding erase size to trigger sector cleanup
  bytes_remaining += kvs_attr.EraseSize();
  FillKvs(keys[2], bytes_remaining);

  // Verify key[0]
  std::memset(buffer.data(), 0, kvs_attr.DataSize());
  ASSERT_EQ(
      Status::OK,
      kvs_.Get(keys[0], span(buffer.data(), kvs_attr.DataSize())).status());
  for (uint32_t i = 0; i < kvs_attr.DataSize(); i++) {
    EXPECT_EQ(buffer[i], kKey0Pattern);
  }
}

TEST_F(EmptyInitializedKvs, Interleaved) {
  const uint8_t kValue1 = 0xDA;
  const uint8_t kValue2 = 0x12;
  uint8_t value;
  ASSERT_EQ(Status::OK, kvs_.Put(keys[0], kValue1));
  EXPECT_EQ(kvs_.size(), 1u);
  ASSERT_EQ(Status::OK, kvs_.Delete(keys[0]));
  EXPECT_EQ(kvs_.Get(keys[0], &value), Status::NOT_FOUND);
  ASSERT_EQ(Status::OK, kvs_.Put(keys[1], as_bytes(span(&kValue1, 1))));
  ASSERT_EQ(Status::OK, kvs_.Put(keys[2], kValue2));
  ASSERT_EQ(Status::OK, kvs_.Delete(keys[1]));
  EXPECT_EQ(Status::OK, kvs_.Get(keys[2], &value));
  EXPECT_EQ(kValue2, value);

  EXPECT_EQ(kvs_.size(), 1u);
}

TEST_F(EmptyInitializedKvs, DeleteAndReinitialize) {
  // Write value
  const uint8_t kValue = 0xDA;
  ASSERT_EQ(Status::OK, kvs_.Put(keys[0], kValue));

  ASSERT_EQ(Status::OK, kvs_.Delete(keys[0]));
  uint8_t value;
  ASSERT_EQ(kvs_.Get(keys[0], &value), Status::NOT_FOUND);

  // Reset KVS, ensure captured at enable
  ASSERT_EQ(Status::OK, kvs_.Init());

  ASSERT_EQ(kvs_.Get(keys[0], &value), Status::NOT_FOUND);
}

TEST_F(EmptyInitializedKvs, TemplatedPutAndGet) {
  // Store a value with the convenience method.
  const uint32_t kValue = 0x12345678;
  ASSERT_EQ(Status::OK, kvs_.Put(keys[0], kValue));

  // Read it back with the other convenience method.
  uint32_t value;
  ASSERT_EQ(Status::OK, kvs_.Get(keys[0], &value));
  ASSERT_EQ(kValue, value);

  // Make sure we cannot get something where size isn't what we expect
  const uint8_t kSmallValue = 0xBA;
  uint8_t small_value = kSmallValue;
  ASSERT_EQ(kvs_.Get(keys[0], &small_value), Status::INVALID_ARGUMENT);
  ASSERT_EQ(small_value, kSmallValue);
}

// This test is derived from bug that was discovered. Testing this corner case
// relies on creating a new key-value just under the size that is left over in
// the sector.
TEST_F(EmptyInitializedKvs, FillSector2) {
  if (test_partition.sector_count() < 3) {
    PW_LOG_INFO("Not enough sectors, skipping test.");
    return;  // need at least 3 sectors
  }

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

  size_t expected_remaining = test_partition.sector_size_bytes() - kSizeToFill;
  ASSERT_EQ(new_keyvalue_size, expected_remaining);

  const char* kNewKey = "NewKey";
  constexpr size_t kValueLessThanChunkHeaderSize = 2;
  constexpr auto kTestPattern = byte{0xBA};
  new_keyvalue_size -= kValueLessThanChunkHeaderSize;
  std::memset(buffer.data(), static_cast<int>(kTestPattern), new_keyvalue_size);
  ASSERT_EQ(Status::OK,
            kvs_.Put(kNewKey, span(buffer.data(), new_keyvalue_size)));

  // In failed corner case, adding new key is deceptively successful. It isn't
  // until KVS is disabled and reenabled that issue can be detected.
  ASSERT_EQ(Status::OK, kvs_.Init());

  // Might as well check that new key-value is what we expect it to be
  ASSERT_EQ(Status::OK,
            kvs_.Get(kNewKey, span(buffer.data(), new_keyvalue_size)).status());
  for (size_t i = 0; i < new_keyvalue_size; i++) {
    EXPECT_EQ(buffer[i], kTestPattern);
  }
}

TEST_F(EmptyInitializedKvs, ValueSize_Positive) {
  constexpr auto kData = AsBytes('h', 'i', '!');
  ASSERT_EQ(Status::OK, kvs_.Put("TheKey", kData));

  auto result = kvs_.ValueSize("TheKey");

  EXPECT_EQ(Status::OK, result.status());
  EXPECT_EQ(kData.size(), result.size());
}

TEST_F(EmptyInitializedKvs, ValueSize_Zero) {
  ASSERT_EQ(Status::OK, kvs_.Put("TheKey", as_bytes(span("123", 3))));
  auto result = kvs_.ValueSize("TheKey");

  EXPECT_EQ(Status::OK, result.status());
  EXPECT_EQ(3u, result.size());
}

TEST_F(EmptyInitializedKvs, ValueSize_InvalidKey) {
  EXPECT_EQ(Status::INVALID_ARGUMENT, kvs_.ValueSize("").status());
}

TEST_F(EmptyInitializedKvs, ValueSize_MissingKey) {
  EXPECT_EQ(Status::NOT_FOUND, kvs_.ValueSize("Not in there").status());
}

TEST_F(EmptyInitializedKvs, ValueSize_DeletedKey) {
  ASSERT_EQ(Status::OK, kvs_.Put("TheKey", as_bytes(span("123", 3))));
  ASSERT_EQ(Status::OK, kvs_.Delete("TheKey"));

  EXPECT_EQ(Status::NOT_FOUND, kvs_.ValueSize("TheKey").status());
}

#if USE_MEMORY_BUFFER

class LargeEmptyInitializedKvs : public ::testing::Test {
 protected:
  LargeEmptyInitializedKvs() : kvs_(&large_test_partition, default_format) {
    ASSERT_EQ(
        Status::OK,
        large_test_partition.Erase(0, large_test_partition.sector_count()));
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

#endif  // USE_MEMORY_BUFFER

TEST_F(EmptyInitializedKvs, CallingEraseTwice_NothingWrittenToFlash) {
  const uint8_t kValue = 0xDA;
  ASSERT_EQ(Status::OK, kvs_.Put(keys[0], kValue));
  ASSERT_EQ(Status::OK, kvs_.Delete(keys[0]));

  // Compare before / after checksums to verify that nothing was written.
  const uint16_t crc = checksum::CcittCrc16(test_flash.buffer());

  EXPECT_EQ(kvs_.Delete(keys[0]), Status::NOT_FOUND);

  EXPECT_EQ(crc, checksum::CcittCrc16(test_flash.buffer()));
}

}  // namespace pw::kvs
