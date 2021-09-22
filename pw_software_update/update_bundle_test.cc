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

#include <array>

#include "gtest/gtest.h"
#include "pw_kvs/fake_flash_memory.h"
#include "pw_kvs/test_key_value_store.h"
#include "pw_software_update/bundled_update_backend.h"
#include "test_bundles.h"

#define ASSERT_OK(status) ASSERT_EQ(OkStatus(), status)

namespace pw::software_update {
namespace {

constexpr size_t kBufferSize = 256;
static constexpr size_t kFlashAlignment = 16;
constexpr size_t kSectorSize = 2048;
constexpr size_t kSectorCount = 2;
constexpr size_t kMetadataBufferSize =
    blob_store::BlobStore::BlobWriter::RequiredMetadataBufferSize(0);

class TestBundledUpdateBackend final : public BundledUpdateBackend {
 public:
  Status FinalizeApply() override { return OkStatus(); }

  Status ApplyTargetFile(std::string_view, stream::Reader&) override {
    return OkStatus();
  }

  Result<uint32_t> EnableBundleTransferHandler() override { return 0; }

  void DisableBundleTransferHandler() override {}
};

class UpdateBundleTest : public testing::Test {
 public:
  UpdateBundleTest()
      : blob_flash_(kFlashAlignment),
        blob_partition_(&blob_flash_),
        bundle_blob_("TestBundle",
                     blob_partition_,
                     nullptr,
                     kvs::TestKvs(),
                     kBufferSize) {}

  blob_store::BlobStoreBuffer<kBufferSize>& bundle_blob() {
    return bundle_blob_;
  }

  BundledUpdateBackend& backend() { return backend_; }

  void StageTestBundle(ConstByteSpan bundle_data) {
    ASSERT_OK(bundle_blob_.Init());
    blob_store::BlobStore::BlobWriter blob_writer(bundle_blob(),
                                                  metadata_buffer_);
    ASSERT_OK(blob_writer.Open());
    ASSERT_OK(blob_writer.Write(bundle_data));
    ASSERT_OK(blob_writer.Close());
  }

 private:
  kvs::FakeFlashMemoryBuffer<kSectorSize, kSectorCount> blob_flash_;
  kvs::FlashPartition blob_partition_;
  blob_store::BlobStoreBuffer<kBufferSize> bundle_blob_;
  std::array<std::byte, kMetadataBufferSize> metadata_buffer_;
  TestBundledUpdateBackend backend_;
};

}  // namespace

TEST_F(UpdateBundleTest, GetTargetPayload) {
  StageTestBundle(kTestBundle);
  UpdateBundle update_bundle(bundle_blob(), backend());

  Manifest current_manifest;
  ASSERT_OK(update_bundle.OpenAndVerify(current_manifest));

  {
    stream::IntervalReader res = update_bundle.GetTargetPayload("file1");
    ASSERT_OK(res.status());

    const char kExpectedContent[] = "file 1 content";
    char read_buffer[sizeof(kExpectedContent) + 1] = {0};
    ASSERT_TRUE(res.Read(read_buffer, sizeof(kExpectedContent)).ok());
    ASSERT_STREQ(read_buffer, kExpectedContent);
  }

  {
    stream::IntervalReader res = update_bundle.GetTargetPayload("file2");
    ASSERT_OK(res.status());

    const char kExpectedContent[] = "file 2 content";
    char read_buffer[sizeof(kExpectedContent) + 1] = {0};
    ASSERT_TRUE(res.Read(read_buffer, sizeof(kExpectedContent)).ok());
    ASSERT_STREQ(read_buffer, kExpectedContent);
  }

  {
    stream::IntervalReader res = update_bundle.GetTargetPayload("non-exist");
    ASSERT_EQ(res.status(), Status::NotFound());
  }
}

TEST_F(UpdateBundleTest, IsTargetPayloadIncluded) {
  StageTestBundle(kTestBundle);
  UpdateBundle update_bundle(bundle_blob(), backend());

  Manifest current_manifest;
  ASSERT_OK(update_bundle.OpenAndVerify(current_manifest));

  Result<bool> res = update_bundle.IsTargetPayloadIncluded("file1");
  ASSERT_OK(res.status());
  ASSERT_TRUE(res.value());

  res = update_bundle.IsTargetPayloadIncluded("file2");
  ASSERT_OK(res.status());
  ASSERT_TRUE(res.value());

  res = update_bundle.IsTargetPayloadIncluded("non-exist");
  ASSERT_OK(res.status());
  ASSERT_FALSE(res.value());
}

TEST_F(UpdateBundleTest, WriteManifest) {
  StageTestBundle(kTestBundle);
  UpdateBundle update_bundle(bundle_blob(), backend());

  Manifest current_manifest;
  ASSERT_OK(update_bundle.OpenAndVerify(current_manifest));

  std::byte manifest_buffer[sizeof(kTestBundleManifest)];
  stream::MemoryWriter manifest_writer(manifest_buffer);
  ASSERT_OK(update_bundle.WriteManifest(manifest_writer));

  ASSERT_EQ(
      memcmp(manifest_buffer, kTestBundleManifest, sizeof(kTestBundleManifest)),
      0);
}

}  // namespace pw::software_update
