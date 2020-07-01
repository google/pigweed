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
#include "pw_kvs/fake_flash_memory.h"
#include "pw_kvs/flash_memory.h"
#include "pw_kvs/flash_partition_test.h"
#include "pw_log/log.h"

namespace pw::kvs::PartitionTest {
namespace {

TEST(FakeFlashPartitionTest, FillTest16) {
  FakeFlashMemoryBuffer<512, 8> flash(16);
  FlashPartition test_partition(&flash);

  // WriteTest(test_partition);
}

TEST(FakeFlashPartitionTest, FillTest64) {
  FakeFlashMemoryBuffer<512, 8> flash(64);
  FlashPartition test_partition(&flash);

  // WriteTest(test_partition);
}

TEST(FakeFlashPartitionTest, FillTest256) {
  FakeFlashMemoryBuffer<512, 8> flash(256);
  FlashPartition test_partition(&flash);

  // WriteTest(test_partition);
}

TEST(FakeFlashPartitionTest, EraseTest) {
  FakeFlashMemoryBuffer<512, 8> flash(16);
  FlashPartition test_partition(&flash);

  // EraseTest(test_partition);
}

TEST(FakeFlashPartitionTest, ReadOnlyTest) {
  FakeFlashMemoryBuffer<512, 8> flash(16);
  FlashPartition test_partition(
      &flash, 0, flash.sector_count(), 0, PartitionPermission::kReadOnly);

  // ReadOnlyTest(test_partition);
}

}  // namespace
}  // namespace pw::kvs::PartitionTest
