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

namespace pw::kvs::internal {

class EmptyEntryCache : public ::testing::Test {
 protected:
  static constexpr size_t kRedundancy = 3;

  EmptyEntryCache() : entries_(descriptors_, addresses_, kRedundancy) {}

  Vector<KeyDescriptor, 32> descriptors_;
  EntryCache::AddressList<32, kRedundancy> addresses_;

  EntryCache entries_;
};

TEST_F(EmptyEntryCache, AddNew) {
  EntryMetadata metadata = entries_.AddNew(
      {.key_hash = 1, .transaction_id = 123, .state = EntryState::kValid}, 5);
  EXPECT_EQ(1u, metadata.hash());
  EXPECT_EQ(123u, metadata.transaction_id());

  EXPECT_EQ(5u, metadata.first_address());
  EXPECT_EQ(1u, metadata.addresses().size());
}

// TODO(hepler): Expand these tests!

}  // namespace pw::kvs::internal
