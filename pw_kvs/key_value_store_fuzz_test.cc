// Copyright 2021 The Pigweed Authors
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

#include <array>
#include <cstring>
#include <span>

#include "gtest/gtest.h"
#include "pw_checksum/crc16_ccitt.h"
#include "pw_kvs/crc16_checksum.h"
#include "pw_kvs/flash_memory.h"
#include "pw_kvs/flash_test_partition.h"
#include "pw_kvs/key_value_store.h"
#include "pw_log/log.h"
#include "pw_status/status.h"

namespace pw::kvs {
namespace {

using std::byte;

constexpr size_t kMaxEntries = 256;
constexpr size_t kMaxUsableSectors = 1024;

constexpr std::array<const char*, 3> keys{"TestKey1", "Key2", "TestKey3"};

ChecksumCrc16 checksum;
// For KVS magic value always use a random 32 bit integer rather than a
// human readable 4 bytes. See pw_kvs/format.h for more information.
constexpr EntryFormat default_format{.magic = 0x749c361e,
                                     .checksum = &checksum};
}  // namespace

TEST(KvsFuzz, FuzzTest) {
  FlashPartition& test_partition = FlashTestPartition();
  test_partition.Erase()
      .IgnoreError();  // TODO(pwbug/387): Handle Status properly

  KeyValueStoreBuffer<kMaxEntries, kMaxUsableSectors> kvs_(&test_partition,
                                                           default_format);

  ASSERT_EQ(OkStatus(), kvs_.Init());

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
  ASSERT_EQ(OkStatus(), kvs_.Put(key1, buf1));
  ASSERT_EQ(OkStatus(), kvs_.Put(key2, buf2));
  for (size_t j = 0; j < keys.size(); j++) {
    ASSERT_EQ(OkStatus(), kvs_.Put(keys[j], j));
  }

  for (size_t i = 0; i < 100; i++) {
    // Vary two sizes
    size_t size1 = (kLargestBufSize) / (i + 1);
    size_t size2 = (kLargestBufSize) / (100 - i);
    for (size_t j = 0; j < 50; j++) {
      // Rewrite a single key many times, can fill up a sector
      ASSERT_EQ(OkStatus(), kvs_.Put("some_data", j));
    }
    // Delete and re-add everything
    ASSERT_EQ(OkStatus(), kvs_.Delete(key1));
    ASSERT_EQ(OkStatus(), kvs_.Put(key1, std::span(buf1, size1)));
    ASSERT_EQ(OkStatus(), kvs_.Delete(key2));
    ASSERT_EQ(OkStatus(), kvs_.Put(key2, std::span(buf2, size2)));
    for (size_t j = 0; j < keys.size(); j++) {
      ASSERT_EQ(OkStatus(), kvs_.Delete(keys[j]));
      ASSERT_EQ(OkStatus(), kvs_.Put(keys[j], j));
    }

    // Re-enable and verify
    ASSERT_EQ(OkStatus(), kvs_.Init());
    static byte buf[4 * 1024];
    ASSERT_EQ(OkStatus(), kvs_.Get(key1, std::span(buf, size1)).status());
    ASSERT_EQ(std::memcmp(buf, buf1, size1), 0);
    ASSERT_EQ(OkStatus(), kvs_.Get(key2, std::span(buf, size2)).status());
    ASSERT_EQ(std::memcmp(buf2, buf2, size2), 0);
    for (size_t j = 0; j < keys.size(); j++) {
      size_t ret = 1000;
      ASSERT_EQ(OkStatus(), kvs_.Get(keys[j], &ret));
      ASSERT_EQ(ret, j);
    }
  }
}

}  // namespace pw::kvs
