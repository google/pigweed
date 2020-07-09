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

#include <span>

#include "gtest/gtest.h"
#include "pw_kvs/flash_memory.h"
#include "pw_kvs/flash_test_partition.h"
#include "pw_kvs_private/config.h"
#include "pw_log/log.h"

namespace pw::kvs::PartitionTest {
namespace {

#ifndef PW_FLASH_TEST_ITERATIONS
#define PW_FLASH_TEST_ITERATIONS 2
#endif  // PW_FLASH_TEST_ITERATIONS

constexpr size_t kTestIterations = PW_FLASH_TEST_ITERATIONS;

constexpr size_t kTestDataSize = kMaxFlashAlignment;

void WriteData(FlashPartition& partition, uint8_t fill_byte) {
  uint8_t test_data[kTestDataSize];
  memset(test_data, fill_byte, sizeof(test_data));

  ASSERT_GE(kTestDataSize, partition.alignment_bytes());

  partition.Erase(0, partition.sector_count());

  const size_t chunks_per_sector =
      partition.sector_size_bytes() / partition.alignment_bytes();

  // Fill partition sector by sector. Fill the sector with an integer number
  // of alignment-size chunks. If the sector is not evenly divisible by
  // alignment-size, the remainder is not written.
  for (size_t sector_index = 0; sector_index < partition.sector_count();
       sector_index++) {
    FlashPartition::Address address =
        sector_index * partition.sector_size_bytes();

    for (size_t chunk_index = 0; chunk_index < chunks_per_sector;
         chunk_index++) {
      StatusWithSize status = partition.Write(
          address, as_bytes(std::span(test_data, partition.alignment_bytes())));
      ASSERT_EQ(Status::OK, status.status());
      ASSERT_EQ(partition.alignment_bytes(), status.size());
      address += partition.alignment_bytes();
    }
  }

  // Check the fill result. Use expect so the test doesn't bail on error.
  // Count the errors and print if any errors are found.
  size_t error_count = 0;
  for (size_t sector_index = 0; sector_index < partition.sector_count();
       sector_index++) {
    FlashPartition::Address address =
        sector_index * partition.sector_size_bytes();

    for (size_t chunk_index = 0; chunk_index < chunks_per_sector;
         chunk_index++) {
      memset(test_data, 0, sizeof(test_data));
      StatusWithSize status =
          partition.Read(address, partition.alignment_bytes(), test_data);

      EXPECT_EQ(Status::OK, status.status());
      EXPECT_EQ(partition.alignment_bytes(), status.size());
      if (!status.ok() || (partition.alignment_bytes() != status.size())) {
        error_count++;
        continue;
      }

      for (size_t i = 0; i < partition.alignment_bytes(); i++) {
        if (test_data[i] != fill_byte) {
          error_count++;
        }
      }

      address += partition.alignment_bytes();
    }
  }

  EXPECT_EQ(error_count, 0U);
  if (error_count != 0) {
    PW_LOG_ERROR("Partition test, fill '%c', %u errors found",
                 fill_byte,
                 unsigned(error_count));
  }
}

TEST(FlashPartitionTest, FillTest) {
  FlashPartition& test_partition = FlashTestPartition();

  ASSERT_GE(kTestDataSize, test_partition.alignment_bytes());

  for (size_t i = 0; i < kTestIterations; i++) {
    WriteData(test_partition, 0);
    WriteData(test_partition, 0xff);
    WriteData(test_partition, 0x55);
    WriteData(test_partition, 0xa3);
  }
}

TEST(FlashPartitionTest, EraseTest) {
  FlashPartition& test_partition = FlashTestPartition();

  static const uint8_t fill_byte = 0x55;
  uint8_t test_data[kTestDataSize];
  memset(test_data, fill_byte, sizeof(test_data));

  ASSERT_GE(kTestDataSize, test_partition.alignment_bytes());

  const size_t block_size =
      std::min(sizeof(test_data), test_partition.sector_size_bytes());
  auto data_span = std::span(test_data, block_size);

  ASSERT_EQ(Status::OK, test_partition.Erase(0, test_partition.sector_count()));

  // Write to the first page of each sector.
  for (size_t sector_index = 0; sector_index < test_partition.sector_count();
       sector_index++) {
    FlashPartition::Address address =
        sector_index * test_partition.sector_size_bytes();

    StatusWithSize status = test_partition.Write(address, as_bytes(data_span));
    ASSERT_EQ(Status::OK, status.status());
    ASSERT_EQ(block_size, status.size());
  }

  // Preset the flag to make sure the check actually sets it.
  bool is_erased = true;
  ASSERT_EQ(Status::OK,
            test_partition.IsRegionErased(
                0, test_partition.size_bytes(), &is_erased));
  ASSERT_EQ(false, is_erased);

  ASSERT_EQ(Status::OK, test_partition.Erase());

  // Preset the flag to make sure the check actually sets it.
  is_erased = false;
  ASSERT_EQ(Status::OK,
            test_partition.IsRegionErased(
                0, test_partition.size_bytes(), &is_erased));
  ASSERT_EQ(true, is_erased);

  // Read the first page of each sector and make sure it has been erased.
  for (size_t sector_index = 0; sector_index < test_partition.sector_count();
       sector_index++) {
    FlashPartition::Address address =
        sector_index * test_partition.sector_size_bytes();

    StatusWithSize status =
        test_partition.Read(address, data_span.size_bytes(), data_span.data());
    ASSERT_EQ(Status::OK, status.status());
    ASSERT_EQ(data_span.size_bytes(), status.size());

    ASSERT_EQ(true, test_partition.AppearsErased(as_bytes(data_span)));
  }
}

}  // namespace
}  // namespace pw::kvs::PartitionTest
