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

#include <random>
#include <string>
#include <string_view>
#include <unordered_map>

#define DUMP_KVS_CONTENTS 0

#if DUMP_KVS_CONTENTS
#include <iostream>
#endif  // DUMP_KVS_CONTENTS

#include "gtest/gtest.h"
#include "pw_kvs/crc16_checksum.h"
#include "pw_kvs/in_memory_fake_flash.h"
#include "pw_kvs/key_value_store.h"
#include "pw_span/span.h"

namespace pw::kvs {
namespace {

using std::byte;

constexpr std::string_view kChars =
    "abcdefghijklmnopqrstuvwxyz"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "0123456789";

struct TestParameters {
  size_t sector_size;
  size_t sector_count;
  size_t sector_alignment;
  size_t partition_start_sector;
  size_t partition_sector_count;
  size_t partition_alignment;
};

template <const TestParameters& kParams>
class KvsTester {
 public:
  static constexpr EntryHeaderFormat kFormat{.magic = 0xBAD'C0D3,
                                             .checksum = nullptr};

  KvsTester()
      : partition_(&flash_,
                   kParams.partition_start_sector,
                   kParams.partition_sector_count,
                   kParams.partition_alignment),
        kvs_(&partition_, kFormat) {
    EXPECT_EQ(Status::OK, partition_.Erase());
    Status result = kvs_.Init();
    EXPECT_EQ(Status::OK, result);

    if (!result.ok()) {
      std::abort();
    }
  }

  ~KvsTester() {
#if DUMP_KVS_CONTENTS
    std::cout << "/==============================================\\\n";
    std::cout << "KVS EXPECTED CONTENTS\n";
    std::cout << "------------------------------------------------\n";
    std::cout << "Entries: " << map_.size() << '\n';
    std::cout << "------------------------------------------------\n";
    for (const auto& [key, value] : map_) {
      std::cout << key << " = " << value << '\n';
    }
    std::cout << "\\===============================================/\n";
#endif  // DUMP_KVS_CONTENTS

    EXPECT_EQ(map_.size(), kvs_.size());

    size_t count = 0;

    for (auto& item : kvs_) {
      count += 1;

      auto map_entry = map_.find(std::string(item.key()));
      EXPECT_NE(map_entry, map_.end());

      if (map_entry != map_.end()) {
        EXPECT_EQ(map_entry->first, item.key());

        char value[kMaxValueLength + 1] = {};
        EXPECT_EQ(Status::OK,
                  item.Get(as_writable_bytes(span(value))).status());
        EXPECT_EQ(map_entry->second, std::string(value));
      }
    }

    EXPECT_EQ(count, map_.size());
  }

  void TestRandomValidInputs(int iterations) {
    std::mt19937 random(6006411);
    std::uniform_int_distribution<unsigned> distro;
    auto random_int = [&] { return distro(random); };

    auto random_string = [&](size_t length) {
      std::string value;
      for (size_t i = 0; i < length; ++i) {
        value.push_back(kChars[random_int() % kChars.size()]);
      }
      return value;
    };

    for (int i = 0; i < iterations; ++i) {
      // One out of 4 times, delete a key.
      if (random_int() % 4 == 0) {
        // Either delete a non-existent key or delete an existing one.
        if (empty() || random_int() % 8 == 0) {
          Delete("not_a_key" + std::to_string(random_int()));
        } else {
          Delete(RandomKey());
        }
      } else {
        std::string key;

        // Either add a new key or replace an existing one.
        if (empty() || random_int() % 2 == 0) {
          key = random_string(random_int() % KeyValueStore::kMaxKeyLength);
        } else {
          key = RandomKey();
        }

        Put(key, random_string(random_int() % kMaxValueLength));
      }
    }
  }

  void TestPut() {
    Put("base_key", "base_value");
    for (int i = 0; i < 100; ++i) {
      Put("other_key", std::to_string(i));
    }
    for (int i = 0; i < 100; ++i) {
      Put("key_" + std::to_string(i), std::to_string(i));
    }
  }

 private:
  // Adds a key to the KVS, if there is room for it.
  void Put(const std::string& key, const std::string& value) {
    ASSERT_LE(value.size(), kMaxValueLength);

    Status result = kvs_.Put(key, as_bytes(span(value)));

    if (key.empty() || key.size() > KeyValueStore::kMaxKeyLength) {
      EXPECT_EQ(Status::INVALID_ARGUMENT, result);
    } else if (map_.size() == KeyValueStore::kMaxEntries) {
      EXPECT_EQ(Status::RESOURCE_EXHAUSTED, result);
    } else {
      ASSERT_EQ(Status::OK, result);
      map_[key] = value;
    }
  }

  // Deletes a key from the KVS if it is present.
  void Delete(const std::string& key) {
    Status result = kvs_.Delete(key);

    if (key.empty() || key.size() >= KeyValueStore::kMaxKeyLength) {
      EXPECT_EQ(Status::INVALID_ARGUMENT, result);
    } else if (map_.count(key) == 0) {
      EXPECT_EQ(Status::NOT_FOUND, result);
    } else {
      ASSERT_EQ(Status::OK, result);
      map_.erase(key);
    }
  }

  bool empty() const { return map_.empty(); }

  std::string RandomKey() const {
    return map_.empty() ? "" : map_.begin()->second;
  }

  static constexpr size_t kMaxValueLength = 64;

  static InMemoryFakeFlash<kParams.sector_size, kParams.sector_count> flash_;
  FlashPartition partition_;

  KeyValueStore kvs_;
  std::unordered_map<std::string, std::string> map_;
};

template <const TestParameters& kParams>
InMemoryFakeFlash<kParams.sector_size, kParams.sector_count>
    KvsTester<kParams>::flash_ =
        InMemoryFakeFlash<kParams.sector_size, kParams.sector_count>(
            kParams.sector_alignment);

// Defines a test fixture that runs all tests against a flash with the specified
// parameters.
#define RUN_TESTS_WITH_PARAMETERS(name, ...)                                \
  class name : public ::testing::Test {                                     \
   protected:                                                               \
    static constexpr TestParameters kParams = {__VA_ARGS__};                \
                                                                            \
    KvsTester<kParams> tester_;                                             \
  };                                                                        \
                                                                            \
  /* Run each test defined in the KvsTester class with these parameters. */ \
                                                                            \
  TEST_F(name, Put) { tester_.TestPut(); }                                  \
  TEST_F(name, DISABLED_RandomValidInputs) { /* TODO: Get this passing! */  \
    tester_.TestRandomValidInputs(1000);                                    \
  }                                                                         \
  static_assert(true, "Don't forget a semicolon!")

RUN_TESTS_WITH_PARAMETERS(Basic,
                          .sector_size = 4 * 1024,
                          .sector_count = 4,
                          .sector_alignment = 16,
                          .partition_start_sector = 0,
                          .partition_sector_count = 4,
                          .partition_alignment = 16);

// TODO: This test suite causes an infinite loop.
RUN_TESTS_WITH_PARAMETERS(DISABLED_NonPowerOf2Alignment,
                          .sector_size = 1000,
                          .sector_count = 4,
                          .sector_alignment = 10,
                          .partition_start_sector = 0,
                          .partition_sector_count = 4,
                          .partition_alignment = 100);

// TODO: This test suite fails to initialize.
RUN_TESTS_WITH_PARAMETERS(DISABLED_Unaligned,
                          .sector_size = 1026,
                          .sector_count = 3,
                          .sector_alignment = 10,
                          .partition_start_sector = 1,
                          .partition_sector_count = 2,
                          .partition_alignment = 9);

}  // namespace
}  // namespace pw::kvs
