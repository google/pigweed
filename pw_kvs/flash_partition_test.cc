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

size_t error_count = 0;

void WriteData(FlashPartition& partition, uint8_t fill_byte) {
  uint8_t test_data[kMaxFlashAlignment];
  memset(test_data, fill_byte, sizeof(test_data));

  const size_t alignment = partition.alignment_bytes();

  ASSERT_EQ(Status::Ok(), partition.Erase(0, partition.sector_count()));

  const size_t chunks_per_sector = partition.sector_size_bytes() / alignment;

  // Fill partition sector by sector. Fill the sector with an integer number
  // of alignment-size chunks. If the sector is not evenly divisible by
  // alignment-size, the remainder is not written.
  for (size_t sector_index = 0; sector_index < partition.sector_count();
       sector_index++) {
    FlashPartition::Address address =
        sector_index * partition.sector_size_bytes();

    for (size_t chunk_index = 0; chunk_index < chunks_per_sector;
         chunk_index++) {
      StatusWithSize status =
          partition.Write(address, as_bytes(std::span(test_data, alignment)));
      ASSERT_EQ(Status::Ok(), status.status());
      ASSERT_EQ(alignment, status.size());
      address += alignment;
    }
  }

  // Check the fill result. Use expect so the test doesn't bail on error.
  // Count the errors and print if any errors are found.
  for (size_t sector_index = 0; sector_index < partition.sector_count();
       sector_index++) {
    FlashPartition::Address address =
        sector_index * partition.sector_size_bytes();

    for (size_t chunk_index = 0; chunk_index < chunks_per_sector;
         chunk_index++) {
      memset(test_data, 0, sizeof(test_data));
      StatusWithSize status = partition.Read(address, alignment, test_data);

      EXPECT_EQ(Status::Ok(), status.status());
      EXPECT_EQ(alignment, status.size());
      if (!status.ok() || (alignment != status.size())) {
        error_count++;
        PW_LOG_DEBUG("   Read Error [%s], %u of %u",
                     status.status().str(),
                     unsigned(status.size()),
                     unsigned(alignment));
        continue;
      }

      for (size_t i = 0; i < alignment; i++) {
        if (test_data[i] != fill_byte) {
          error_count++;
          PW_LOG_DEBUG(
              "   Error %u, Read compare @ address %x, got 0x%02x, "
              "expected 0x%02x",
              unsigned(error_count),
              unsigned(address + i),
              unsigned(test_data[i]),
              unsigned(fill_byte));
        }
      }

      address += alignment;
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

  ASSERT_GE(kMaxFlashAlignment, test_partition.alignment_bytes());

  for (size_t i = 0; i < kTestIterations; i++) {
    PW_LOG_DEBUG("FillTest iteration %u, write '0'", unsigned(i));
    WriteData(test_partition, 0);
    PW_LOG_DEBUG("FillTest iteration %u, write '0xff'", unsigned(i));
    WriteData(test_partition, 0xff);
    PW_LOG_DEBUG("FillTest iteration %u, write '0x55'", unsigned(i));
    WriteData(test_partition, 0x55);
    PW_LOG_DEBUG("FillTest iteration %u, write '0xa3'", unsigned(i));
    WriteData(test_partition, 0xa3);
    PW_LOG_DEBUG("Completed iterations %u, Total errors %u",
                 unsigned(i),
                 unsigned(error_count));
  }
}

TEST(FlashPartitionTest, EraseTest) {
  FlashPartition& test_partition = FlashTestPartition();

  static const uint8_t fill_byte = 0x55;
  uint8_t test_data[kMaxFlashAlignment];
  memset(test_data, fill_byte, sizeof(test_data));

  ASSERT_GE(kMaxFlashAlignment, test_partition.alignment_bytes());

  const size_t block_size =
      std::min(sizeof(test_data), test_partition.sector_size_bytes());
  auto data_span = std::span(test_data, block_size);

  ASSERT_EQ(Status::Ok(),
            test_partition.Erase(0, test_partition.sector_count()));

  // Write to the first page of each sector.
  for (size_t sector_index = 0; sector_index < test_partition.sector_count();
       sector_index++) {
    FlashPartition::Address address =
        sector_index * test_partition.sector_size_bytes();

    StatusWithSize status = test_partition.Write(address, as_bytes(data_span));
    ASSERT_EQ(Status::Ok(), status.status());
    ASSERT_EQ(block_size, status.size());
  }

  // Preset the flag to make sure the check actually sets it.
  bool is_erased = true;
  ASSERT_EQ(Status::Ok(), test_partition.IsErased(&is_erased));
  ASSERT_EQ(false, is_erased);

  ASSERT_EQ(Status::Ok(), test_partition.Erase());

  // Preset the flag to make sure the check actually sets it.
  is_erased = false;
  ASSERT_EQ(Status::Ok(), test_partition.IsErased(&is_erased));
  ASSERT_EQ(true, is_erased);

  // Read the first page of each sector and make sure it has been erased.
  for (size_t sector_index = 0; sector_index < test_partition.sector_count();
       sector_index++) {
    FlashPartition::Address address =
        sector_index * test_partition.sector_size_bytes();

    StatusWithSize status =
        test_partition.Read(address, data_span.size_bytes(), data_span.data());
    EXPECT_EQ(Status::Ok(), status.status());
    EXPECT_EQ(data_span.size_bytes(), status.size());

    EXPECT_EQ(true, test_partition.AppearsErased(as_bytes(data_span)));
  }
}

TEST(FlashPartitionTest, AlignmentCheck) {
  FlashPartition& test_partition = FlashTestPartition();
  const size_t alignment = test_partition.alignment_bytes();
  const size_t sector_size_bytes = test_partition.sector_size_bytes();

  EXPECT_LE(alignment, kMaxFlashAlignment);
  EXPECT_GT(alignment, 0u);
  EXPECT_EQ(kMaxFlashAlignment % alignment, 0U);
  EXPECT_LE(kMaxFlashAlignment, sector_size_bytes);
  EXPECT_LE(sector_size_bytes % kMaxFlashAlignment, 0U);
}

#define TESTING_CHECK_FAILURES_IS_SUPPORTED 0
#if TESTING_CHECK_FAILURES_IS_SUPPORTED
// TODO: Ensure that this test triggers an assert.
TEST(FlashPartitionTest, BadWriteAddressAlignment) {
  FlashPartition& test_partition = FlashTestPartition();

  // Can't get bad alignment with alignment of 1.
  if (test_partition.alignment_bytes() == 1) {
    return;
  }

  std::array<std::byte, kMaxFlashAlignment> source_data;
  test_partition.Write(1, source_data);
}

// TODO: Ensure that this test triggers an assert.
TEST(FlashPartitionTest, BadWriteSizeAlignment) {
  FlashPartition& test_partition = FlashTestPartition();

  // Can't get bad alignment with alignment of 1.
  if (test_partition.alignment_bytes() == 1) {
    return;
  }

  std::array<std::byte, 1> source_data;
  test_partition.Write(0, source_data);
}

// TODO: Ensure that this test triggers an assert.
TEST(FlashPartitionTest, BadEraseAddressAlignment) {
  FlashPartition& test_partition = FlashTestPartition();

  // Can't get bad alignment with sector size of 1.
  if (test_partition.sector_size_bytes() == 1) {
    return;
  }

  // Try Erase at address 1 for 1 sector.
  test_partition.Erase(1, 1);
}

#endif  // TESTING_CHECK_FAILURES_IS_SUPPORTED

TEST(FlashPartitionTest, IsErased) {
  FlashPartition& test_partition = FlashTestPartition();
  const size_t alignment = test_partition.alignment_bytes();

  // Make sure the partition is big enough to do this test.
  ASSERT_GE(test_partition.size_bytes(), 3 * kMaxFlashAlignment);

  ASSERT_EQ(Status::Ok(), test_partition.Erase());

  bool is_erased = true;
  ASSERT_EQ(Status::Ok(), test_partition.IsErased(&is_erased));
  ASSERT_EQ(true, is_erased);

  static const uint8_t fill_byte = 0x55;
  uint8_t test_data[kMaxFlashAlignment];
  memset(test_data, fill_byte, sizeof(test_data));
  auto data_span = std::span(test_data);

  // Write the chunk with fill byte.
  StatusWithSize status = test_partition.Write(alignment, as_bytes(data_span));
  ASSERT_EQ(Status::Ok(), status.status());
  ASSERT_EQ(data_span.size_bytes(), status.size());

  EXPECT_EQ(Status::Ok(), test_partition.IsErased(&is_erased));
  EXPECT_EQ(false, is_erased);

  // Check the chunk that was written.
  EXPECT_EQ(Status::Ok(),
            test_partition.IsRegionErased(
                alignment, data_span.size_bytes(), &is_erased));
  EXPECT_EQ(false, is_erased);

  // Check a region that starts erased but later has been written.
  EXPECT_EQ(Status::Ok(),
            test_partition.IsRegionErased(0, 2 * alignment, &is_erased));
  EXPECT_EQ(false, is_erased);

  // Check erased for a region smaller than kMaxFlashAlignment. This has been a
  // bug in the past.
  EXPECT_EQ(Status::Ok(),
            test_partition.IsRegionErased(0, alignment, &is_erased));
  EXPECT_EQ(true, is_erased);
}

}  // namespace
}  // namespace pw::kvs::PartitionTest
