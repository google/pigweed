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

#include "pw_kvs/internal/entry_cache.h"

#include "gtest/gtest.h"
#include "pw_kvs/flash_memory.h"
#include "pw_kvs/in_memory_fake_flash.h"
#include "pw_kvs/internal/hash.h"
#include "pw_kvs/internal/key_descriptor.h"
#include "pw_kvs_private/byte_utils.h"

namespace pw::kvs::internal {
namespace {

using std::byte;

class EmptyEntryCache : public ::testing::Test {
 protected:
  static constexpr size_t kMaxEntries = 32;
  static constexpr size_t kRedundancy = 3;

  EmptyEntryCache() : entries_(descriptors_, addresses_, kRedundancy) {}

  Vector<KeyDescriptor, kMaxEntries> descriptors_;
  EntryCache::AddressList<kMaxEntries, kRedundancy> addresses_;

  EntryCache entries_;
};

constexpr char kTheKey[] = "The Key";

constexpr KeyDescriptor kDescriptor = {.key_hash = Hash(kTheKey),
                                       .transaction_id = 123,
                                       .state = EntryState::kValid};

TEST_F(EmptyEntryCache, AddNew) {
  EntryMetadata metadata = entries_.AddNew(kDescriptor, 5);
  EXPECT_EQ(kDescriptor.key_hash, metadata.hash());
  EXPECT_EQ(kDescriptor.transaction_id, metadata.transaction_id());
  EXPECT_EQ(kDescriptor.state, metadata.state());

  EXPECT_EQ(5u, metadata.first_address());
  EXPECT_EQ(1u, metadata.addresses().size());
}

TEST_F(EmptyEntryCache, EntryMetadata_AddNewAddress) {
  EntryMetadata metadata = entries_.AddNew(kDescriptor, 100);

  metadata.AddNewAddress(999);

  EXPECT_EQ(2u, metadata.addresses().size());
  EXPECT_EQ(100u, metadata.first_address());
  EXPECT_EQ(100u, metadata.addresses()[0]);
  EXPECT_EQ(999u, metadata.addresses()[1]);
}

TEST_F(EmptyEntryCache, EntryMetadata_Reset) {
  EntryMetadata metadata = entries_.AddNew(kDescriptor, 100);
  metadata.AddNewAddress(999);

  metadata.Reset(
      {.key_hash = 987, .transaction_id = 5, .state = EntryState::kDeleted},
      8888);

  EXPECT_EQ(987u, metadata.hash());
  EXPECT_EQ(5u, metadata.transaction_id());
  EXPECT_EQ(EntryState::kDeleted, metadata.state());
  EXPECT_EQ(1u, metadata.addresses().size());
  EXPECT_EQ(8888u, metadata.first_address());
  EXPECT_EQ(8888u, metadata.addresses()[0]);
}

TEST_F(EmptyEntryCache, AddNewOrUpdateExisting_NewEntry) {
  ASSERT_EQ(Status::OK,
            entries_.AddNewOrUpdateExisting(kDescriptor, 1000, 2000));

  EXPECT_EQ(1u, entries_.present_entries());

  for (const EntryMetadata& entry : entries_) {
    EXPECT_EQ(1000u, entry.first_address());
    EXPECT_EQ(kDescriptor.key_hash, entry.hash());
    EXPECT_EQ(kDescriptor.transaction_id, entry.transaction_id());
  }
}

TEST_F(EmptyEntryCache, AddNewOrUpdateExisting_NewEntry_Full) {
  for (uint32_t i = 0; i < kMaxEntries; ++i) {
    ASSERT_EQ(  // Fill up the cache
        Status::OK,
        entries_.AddNewOrUpdateExisting({i, i, EntryState::kValid}, i, 1));
  }
  ASSERT_EQ(kMaxEntries, entries_.total_entries());
  ASSERT_TRUE(entries_.full());

  EXPECT_EQ(Status::RESOURCE_EXHAUSTED,
            entries_.AddNewOrUpdateExisting(kDescriptor, 1000, 1));
  EXPECT_EQ(kMaxEntries, entries_.total_entries());
}

TEST_F(EmptyEntryCache, AddNewOrUpdateExisting_UpdatedEntry) {
  KeyDescriptor kd = kDescriptor;
  kd.transaction_id += 3;

  ASSERT_EQ(Status::OK, entries_.AddNewOrUpdateExisting(kd, 3210, 2000));

  EXPECT_EQ(1u, entries_.present_entries());

  for (const EntryMetadata& entry : entries_) {
    EXPECT_EQ(3210u, entry.first_address());
    EXPECT_EQ(kDescriptor.key_hash, entry.hash());
    EXPECT_EQ(kDescriptor.transaction_id + 3, entry.transaction_id());
  }
}

TEST_F(EmptyEntryCache, AddNewOrUpdateExisting_AddDuplicateEntry) {
  ASSERT_EQ(Status::OK,
            entries_.AddNewOrUpdateExisting(kDescriptor, 1000, 2000));
  ASSERT_EQ(Status::OK,
            entries_.AddNewOrUpdateExisting(kDescriptor, 3000, 2000));
  ASSERT_EQ(Status::OK,
            entries_.AddNewOrUpdateExisting(kDescriptor, 7000, 2000));

  // Duplicates beyond the redundancy are ignored.
  ASSERT_EQ(Status::OK,
            entries_.AddNewOrUpdateExisting(kDescriptor, 9000, 2000));

  EXPECT_EQ(1u, entries_.present_entries());

  for (const EntryMetadata& entry : entries_) {
    EXPECT_EQ(3u, entry.addresses().size());
    EXPECT_EQ(1000u, entry.addresses()[0]);
    EXPECT_EQ(3000u, entry.addresses()[1]);
    EXPECT_EQ(7000u, entry.addresses()[2]);

    EXPECT_EQ(kDescriptor.key_hash, entry.hash());
    EXPECT_EQ(kDescriptor.transaction_id, entry.transaction_id());
  }
}

TEST_F(EmptyEntryCache, AddNewOrUpdateExisting_AddDuplicateEntryInSameSector) {
  ASSERT_EQ(Status::OK,
            entries_.AddNewOrUpdateExisting(kDescriptor, 1000, 1000));
  EXPECT_EQ(Status::DATA_LOSS,
            entries_.AddNewOrUpdateExisting(kDescriptor, 1950, 1000));

  EXPECT_EQ(1u, entries_.present_entries());

  for (const EntryMetadata& entry : entries_) {
    EXPECT_EQ(1u, entry.addresses().size());
    EXPECT_EQ(1000u, entry.addresses()[0]);

    EXPECT_EQ(kDescriptor.key_hash, entry.hash());
    EXPECT_EQ(kDescriptor.transaction_id, entry.transaction_id());
  }
}

TEST_F(EmptyEntryCache, Iterator_Mutable_CanModify) {
  entries_.AddNew(kDescriptor, 1);
  EntryCache::iterator it = entries_.begin();

  static_assert(kRedundancy > 1);
  it->AddNewAddress(1234);

  EXPECT_EQ(1u, it->first_address());
  EXPECT_EQ(1u, (*it).addresses()[0]);
  EXPECT_EQ(1234u, it->addresses()[1]);
}

TEST_F(EmptyEntryCache, Iterator_Const) {
  entries_.AddNew(kDescriptor, 99);
  EntryCache::const_iterator it = entries_.cbegin();

  EXPECT_EQ(99u, (*it).first_address());
  EXPECT_EQ(99u, it->first_address());
}

TEST_F(EmptyEntryCache, Iterator_Const_CanBeAssignedFromMutable) {
  entries_.AddNew(kDescriptor, 99);
  EntryCache::const_iterator it = entries_.begin();

  EXPECT_EQ(99u, (*it).first_address());
  EXPECT_EQ(99u, it->first_address());
}

constexpr auto kTheEntry = AsBytes(uint32_t(12345),  // magic
                                   uint32_t(0),      // checksum
                                   uint8_t(0),       // alignment (16 B)
                                   uint8_t(sizeof(kTheKey) - 1),  // key length
                                   uint16_t(0),                   // value size
                                   uint32_t(123),  // transaction ID
                                   ByteStr(kTheKey));
constexpr std::array<byte, 16 - kTheEntry.size() % 16> kPadding1{};

constexpr char kCollision1[] = "9FDC";
constexpr char kCollision2[] = "axzzK";

constexpr auto kCollisionEntry =
    AsBytes(uint32_t(12345),                   // magic
            uint32_t(0),                       // checksum
            uint8_t(0),                        // alignment (16 B)
            uint8_t(sizeof(kCollision1) - 1),  // key length
            uint16_t(0),                       // value size
            uint32_t(123),                     // transaction ID
            ByteStr(kCollision1));
constexpr std::array<byte, 16 - kCollisionEntry.size() % 16> kPadding2{};

constexpr auto kDeletedEntry =
    AsBytes(uint32_t(12345),                  // magic
            uint32_t(0),                      // checksum
            uint8_t(0),                       // alignment (16 B)
            uint8_t(sizeof("delorted") - 1),  // key length
            uint16_t(0xffff),                 // value size (deleted)
            uint32_t(123),                    // transaction ID
            ByteStr("delorted"));

class InitializedEntryCache : public EmptyEntryCache {
 protected:
  static_assert(Hash(kCollision1) == Hash(kCollision2));

  InitializedEntryCache()
      : flash_(AsBytes(
            kTheEntry, kPadding1, kCollisionEntry, kPadding2, kDeletedEntry)),
        partition_(&flash_) {
    entries_.AddNew(kDescriptor, 0);
    entries_.AddNew({.key_hash = Hash(kCollision1),
                     .transaction_id = 125,
                     .state = EntryState::kDeleted},
                    kTheEntry.size() + kPadding1.size());
    entries_.AddNew({.key_hash = Hash("delorted"),
                     .transaction_id = 256,
                     .state = EntryState::kDeleted},
                    kTheEntry.size() + kPadding1.size() +
                        kCollisionEntry.size() + kPadding2.size());
  }

  FakeFlashBuffer<64, 128> flash_;
  FlashPartition partition_;
};

TEST_F(InitializedEntryCache, EntryCounts) {
  EXPECT_EQ(3u, entries_.total_entries());
  EXPECT_EQ(1u, entries_.present_entries());
  EXPECT_EQ(kMaxEntries, entries_.max_entries());
}

TEST_F(InitializedEntryCache, Reset_ClearsEntryCounts) {
  entries_.Reset();

  EXPECT_EQ(0u, entries_.total_entries());
  EXPECT_EQ(0u, entries_.present_entries());
  EXPECT_EQ(kMaxEntries, entries_.max_entries());
}

TEST_F(InitializedEntryCache, Find_PresentEntry) {
  EntryMetadata metadata;
  ASSERT_EQ(Status::OK, entries_.Find(partition_, kTheKey, &metadata));
  EXPECT_EQ(Hash(kTheKey), metadata.hash());
  EXPECT_EQ(EntryState::kValid, metadata.state());
}

TEST_F(InitializedEntryCache, Find_DeletedEntry) {
  EntryMetadata metadata;
  ASSERT_EQ(Status::OK, entries_.Find(partition_, "delorted", &metadata));
  EXPECT_EQ(Hash("delorted"), metadata.hash());
  EXPECT_EQ(EntryState::kDeleted, metadata.state());
}

TEST_F(InitializedEntryCache, Find_MissingEntry) {
  EntryMetadata metadata;
  ASSERT_EQ(Status::NOT_FOUND, entries_.Find(partition_, "3.141", &metadata));
}

TEST_F(InitializedEntryCache, Find_Collision) {
  EntryMetadata metadata;
  EXPECT_EQ(Status::ALREADY_EXISTS,
            entries_.Find(partition_, kCollision2, &metadata));
}

TEST_F(InitializedEntryCache, FindExisting_PresentEntry) {
  EntryMetadata metadata;
  ASSERT_EQ(Status::OK, entries_.FindExisting(partition_, kTheKey, &metadata));
  EXPECT_EQ(Hash(kTheKey), metadata.hash());
}

TEST_F(InitializedEntryCache, FindExisting_DeletedEntry) {
  EntryMetadata metadata;
  ASSERT_EQ(Status::NOT_FOUND,
            entries_.FindExisting(partition_, "delorted", &metadata));
}

TEST_F(InitializedEntryCache, FindExisting_MissingEntry) {
  EntryMetadata metadata;
  ASSERT_EQ(Status::NOT_FOUND,
            entries_.FindExisting(partition_, "3.141", &metadata));
}

TEST_F(InitializedEntryCache, FindExisting_Collision) {
  EntryMetadata metadata;
  EXPECT_EQ(Status::NOT_FOUND,
            entries_.FindExisting(partition_, kCollision2, &metadata));
}

}  // namespace
}  // namespace pw::kvs::internal
