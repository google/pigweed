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

#include "gtest/gtest.h"
#include "pw_kvs/assert.h"
#include "pw_kvs/flash_memory.h"
#include "pw_kvs/key_value_store.h"
#include "pw_status/status.h"

#define USE_MEMORY_BUFFER 1

#if USE_MEMORY_BUFFER
#include "pw_kvs/in_memory_fake_flash.h"
#endif  // USE_MEMORY_BUFFER

namespace pw::kvs {
namespace {

#if USE_MEMORY_BUFFER
InMemoryFakeFlash<1024, 4> test_flash(8);  // 4 x 1k sectors, 8 byte alignment
FlashPartition test_partition(&test_flash, 0, test_flash.GetSectorCount());
// Test KVS against FlashSubSector.  Expose less than a sector.
FlashMemorySubSector test_subsector_flash(&test_flash, 0, 128);
#else   // TODO: Test with real flash
FlashPartition& test_partition = FlashExternalTestPartition();
FlashMemorySubSector& test_subsector_flash = FlashMemorySubSectorTestChunk();
#endif  // USE_MEMORY_BUFFER

FlashPartition test_subsector_partition(&test_subsector_flash, 0, 1);
KeyValueStore subsector_kvs(&test_subsector_partition);

std::array<const char*, 3> keys{"TestKey1", "Key2", "TestKey3"};

}  // namespace

TEST(KeyValueStoreTest, WorksWithFlashSubSector) {
  // The subsector region is assumed to be a part of the test partition.
  // In order to clear state before the test, we must erase the entire test
  // partition because erase operations are disallowed on FlashMemorySubSectors.
  ASSERT_EQ(Status::OK,
            test_partition.Erase(0, test_partition.GetSectorCount()));

  // Reset KVS
  subsector_kvs.Disable();
  ASSERT_EQ(Status::OK, subsector_kvs.Enable());

  // Add some data
  uint8_t value1 = 0xDA;
  ASSERT_EQ(Status::OK, subsector_kvs.Put(keys[0], &value1, sizeof(value1)));

  uint32_t value2 = 0xBAD0301f;
  ASSERT_EQ(Status::OK,
            subsector_kvs.Put(
                keys[1], reinterpret_cast<uint8_t*>(&value2), sizeof(value2)));

  // Verify data
  uint32_t test2;
  EXPECT_EQ(Status::OK,
            subsector_kvs.Get(
                keys[1], reinterpret_cast<uint8_t*>(&test2), sizeof(test2)));
  uint8_t test1;
  ASSERT_EQ(Status::OK,
            subsector_kvs.Get(
                keys[0], reinterpret_cast<uint8_t*>(&test1), sizeof(test1)));

  EXPECT_EQ(test1, value1);

  // Erase a key
  ASSERT_EQ(Status::OK, subsector_kvs.Erase(keys[0]));

  // Verify it was erased
  EXPECT_EQ(subsector_kvs.Get(
                keys[0], reinterpret_cast<uint8_t*>(&test1), sizeof(test1)),
            Status::NOT_FOUND);
  test2 = 0;
  ASSERT_EQ(Status::OK,
            subsector_kvs.Get(
                keys[1], reinterpret_cast<uint8_t*>(&test2), sizeof(test2)));
  EXPECT_EQ(test2, value2);

  // Erase other key
  subsector_kvs.Erase(keys[1]);

  // Verify it was erased
  EXPECT_EQ(subsector_kvs.KeyCount(), 0);
}

TEST(KeyValueStoreTest, WorksWithFlashSubSector_MemoryExhausted) {
  // The subsector region is assumed to be a part of the test partition.
  // In order to clear state before the test, we must erase the entire test
  // partition because erase operations are disallowed on FlashMemorySubSectors.
  ASSERT_EQ(Status::OK,
            test_partition.Erase(0, test_partition.GetSectorCount()));

  // Reset KVS
  subsector_kvs.Disable();
  ASSERT_EQ(Status::OK, subsector_kvs.Enable());

  // Store as much data as possible in the KVS until it fills up.
  uint64_t test = 0;
  for (; test < test_subsector_flash.GetSizeBytes(); test++) {
    Status status = subsector_kvs.Put(keys[0], test);
    if (status.ok()) {
      continue;
    }
    // Expected code for erasure failure.
    ASSERT_EQ(Status::RESOURCE_EXHAUSTED, status);
    break;
  }
  EXPECT_GT(test, 0U);  // Should've at least succeeded with one Put.

  // Even though we failed to fill the KVS, it still works, and we
  // should have the previous test value as the most recent value in the KVS.
  uint64_t value = 0;
  ASSERT_EQ(Status::OK, subsector_kvs.Get(keys[0], &value));
  EXPECT_EQ(test - 1, value);
}

}  // namespace pw::kvs
