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

#include "pw_software_update/update_bundle.h"

#include "gtest/gtest.h"
#include "pw_kvs/fake_flash_memory.h"
#include "pw_kvs/test_key_value_store.h"

namespace pw::software_update {
namespace {

constexpr size_t kBufferSize = 256;
static constexpr size_t kFlashAlignment = 16;
constexpr size_t kSectorSize = 2048;
constexpr size_t kSectorCount = 2;

class UpdateBundleTest : public testing::Test {
 public:
  UpdateBundleTest()
      : blob_flash_(kFlashAlignment),
        blob_partition_(&blob_flash_),
        bunble_blob_("TestBundle",
                     blob_partition_,
                     nullptr,
                     kvs::TestKvs(),
                     kBufferSize) {}

  blob_store::BlobStoreBuffer<kBufferSize>& bunble_blob() {
    return bunble_blob_;
  }

  BundledUpdateHelper& helper() { return helper_; }

 private:
  kvs::FakeFlashMemoryBuffer<kSectorSize, kSectorCount> blob_flash_;
  kvs::FlashPartition blob_partition_;
  blob_store::BlobStoreBuffer<kBufferSize> bunble_blob_;
  BundledUpdateHelper helper_;
};

}  // namespace

TEST_F(UpdateBundleTest, Create) { UpdateBundle(bunble_blob(), helper()); }

}  // namespace pw::software_update
