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
#include "pw_kvs/flash_memory.h"
#include "pw_kvs/in_memory_fake_flash.h"
#include "pw_kvs/key_value_store.h"
#include "pw_log/log.h"

namespace pw::kvs {
namespace {

constexpr size_t kTestPartitionSectorSize = 4 * 1024;
constexpr size_t kTestPartitionSectorCount = 6;
constexpr size_t kMaxEntries = 256;
constexpr size_t kMaxUsableSectors = kTestPartitionSectorCount;

typedef KeyValueStoreBuffer<kMaxEntries, kMaxUsableSectors> TestKvs;

// Creates a FakeFlashBuffer that tracks erase count.
template <size_t kSectorSize, size_t kSectorCount, size_t kInjectedErrors = 8>
class FakeFlashBufferWithEraseCount
    : public FakeFlashBuffer<kSectorSize, kSectorCount, kInjectedErrors> {
 public:
  // Creates a flash memory with no data written.
  explicit FakeFlashBufferWithEraseCount(
      size_t alignment_bytes = InMemoryFakeFlash::kDefaultAlignmentBytes)
      : FakeFlashBufferWithEraseCount(std::array<std::byte, 0>{},
                                      alignment_bytes) {}

  // Creates a flash memory initialized to the provided contents.
  explicit FakeFlashBufferWithEraseCount(
      span<const std::byte> contents,
      size_t alignment_bytes = InMemoryFakeFlash::kDefaultAlignmentBytes)
      : FakeFlashBuffer<kSectorSize, kSectorCount, kInjectedErrors>(
            contents, alignment_bytes) {
    std::memset(erase_counts_.data(), 0, erase_counts_.size() * sizeof(size_t));
  }

  // Override the in-memory flash fake to clear the erase counts when the entire
  // flash partition is erased.
  Status Clear() {
    std::memset(erase_counts_.data(), 0, erase_counts_.size() * sizeof(size_t));
    return Erase(FlashMemory::start_address(), FlashMemory::sector_count());
  }

  // Override the in-memory flash fake to track sector erase count.
  Status Erase(FlashMemory::Address address, size_t num_sectors) override {
    Status status;
    if (status = InMemoryFakeFlash::Erase(address, num_sectors); !status.ok()) {
      return status;
    }
    for (size_t i = 0; i < num_sectors; ++i) {
      erase_counts_[address / kSectorSize + i]++;
    }
    return Status::OK;
  }

  // Reports the erase count of the sector with the least erases.
  size_t MinEraseCount() {
    size_t min_erases = ~static_cast<size_t>(0);
    for (auto sector_erases : erase_counts_) {
      if (sector_erases < min_erases) {
        min_erases = sector_erases;
      }
    }
    return min_erases;
  }

 private:
  std::array<size_t, kSectorCount> erase_counts_;
};

FakeFlashBufferWithEraseCount<kTestPartitionSectorSize,
                              kTestPartitionSectorCount>
    test_flash(16);
FlashPartition test_partition(&test_flash, 0, test_flash.sector_count());

constexpr EntryFormat format{.magic = 0xBAD'C0D3, .checksum = nullptr};

}  // namespace

// Write a large key (i.e. only one entry fits in each sector) enough times to
// fill up the KVS multiple times, and ensure every sector was garbage collected
// multiple additional times.
TEST(WearLeveling, RepeatedLargeEntry) {
  // Initialize an empty KVS, erasing flash and all tracked sector erase counts.
  test_flash.Clear();
  EXPECT_EQ(test_flash.MinEraseCount(), 1u);
  TestKvs new_kvs(&test_partition, format);
  new_kvs.Init();

  // Add enough large entries to fill the entire KVS several times.
  std::byte data[kTestPartitionSectorSize / 2] = {std::byte(0)};
  for (size_t i = 0; i < kMaxUsableSectors * 10; ++i) {
    EXPECT_TRUE(new_kvs.Put("large_entry", span(data)).ok());
  }

  // Ensure every sector has been erased at several times due to garbage
  // collection.
  EXPECT_GE(test_flash.MinEraseCount(), 7u);
}

}  // namespace pw::kvs
