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

#include "pw_blob_store/blob_store.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <span>

#include "gtest/gtest.h"
#include "pw_kvs/crc16_checksum.h"
#include "pw_kvs/fake_flash_memory.h"
#include "pw_kvs/flash_memory.h"
#include "pw_kvs/test_key_value_store.h"
#include "pw_log/log.h"
#include "pw_random/xor_shift.h"

namespace pw::blob_store {
namespace {

class BlobStoreTest : public ::testing::Test {
 protected:
  BlobStoreTest() : flash_(kFlashAlignment), partition_(&flash_) {}

  void InitFlashTo(std::span<const std::byte> contents) {
    partition_.Erase();
    std::memcpy(flash_.buffer().data(), contents.data(), contents.size());
  }

  void InitBufferToRandom(uint64_t seed) {
    partition_.Erase();
    random::XorShiftStarRng64 rng(seed);
    rng.Get(buffer_);
  }

  // Fill the source buffer with random pattern based on given seed, written to
  // BlobStore in specified chunk size.
  void ChunkWriteTest(size_t chunk_size, uint64_t seed) {
    InitBufferToRandom(seed);

    constexpr size_t kBufferSize = 256;
    kvs::ChecksumCrc16 checksum;

    char name[16] = {};
    snprintf(name, sizeof(name), "Blob%u", unsigned(chunk_size));

    BlobStoreBuffer<kBufferSize> blob(
        name, &partition_, &checksum, kvs::TestKvs());
    EXPECT_EQ(Status::OK, blob.Init());

    BlobStore::BlobWriter writer(blob);
    EXPECT_EQ(Status::OK, writer.Open());

    ByteSpan source = buffer_;
    while (source.size_bytes() > 0) {
      const size_t write_size = std::min(source.size_bytes(), chunk_size);

      PW_LOG_DEBUG("Do write of %u bytes, %u bytes remain",
                   unsigned(write_size),
                   unsigned(source.size_bytes()));

      EXPECT_EQ(Status::OK, writer.Write(source.first(write_size)));

      source = source.subspan(write_size);
    }

    EXPECT_EQ(Status::OK, writer.Close());

    // Use reader to check for valid data.
    BlobStore::BlobReader reader(blob, 0);
    EXPECT_EQ(Status::OK, reader.Open());
    Result<ByteSpan> result = reader.GetMemoryMappedBlob();
    ASSERT_TRUE(result.ok());
    VerifyFlash(result.value());
    EXPECT_EQ(Status::OK, reader.Close());
  }

  void VerifyFlash(ByteSpan verify_bytes) {
    // Should be defined as same size.
    EXPECT_EQ(buffer_.size(), flash_.buffer().size_bytes());

    // Can't allow it to march off the end of buffer_.
    ASSERT_LE(verify_bytes.size_bytes(), buffer_.size());

    for (size_t i = 0; i < verify_bytes.size_bytes(); i++) {
      EXPECT_EQ(buffer_[i], verify_bytes[i]);
    }
  }

  static constexpr size_t kFlashAlignment = 16;
  static constexpr size_t kSectorSize = 2048;
  static constexpr size_t kSectorCount = 2;

  kvs::FakeFlashMemoryBuffer<kSectorSize, kSectorCount> flash_;
  kvs::FlashPartition partition_;
  std::array<std::byte, kSectorCount * kSectorSize> buffer_;
};

TEST_F(BlobStoreTest, Init_Ok) {
  BlobStoreBuffer<256> blob("Blob_OK", &partition_, nullptr, kvs::TestKvs());
  EXPECT_EQ(Status::OK, blob.Init());
}

TEST_F(BlobStoreTest, ChunkWrite1) { ChunkWriteTest(1, 0x8675309); }

TEST_F(BlobStoreTest, ChunkWrite2) { ChunkWriteTest(2, 0x8675); }

TEST_F(BlobStoreTest, ChunkWrite16) { ChunkWriteTest(16, 0x86); }

TEST_F(BlobStoreTest, ChunkWrite64) { ChunkWriteTest(64, 0x9); }

TEST_F(BlobStoreTest, ChunkWrite256) { ChunkWriteTest(256, 0x12345678); }

TEST_F(BlobStoreTest, ChunkWrite512) { ChunkWriteTest(512, 0x42); }

TEST_F(BlobStoreTest, ChunkWrite4096) { ChunkWriteTest(4096, 0x89); }

TEST_F(BlobStoreTest, ChunkWriteSingleFull) {
  ChunkWriteTest((kSectorCount * kSectorSize), 0x98765);
}

}  // namespace
}  // namespace pw::blob_store
