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

#include <array>
#include <cstddef>
#include <cstring>
#include <span>

#include "gtest/gtest.h"
#include "pw_blob_store/blob_store.h"
#include "pw_kvs/crc16_checksum.h"
#include "pw_kvs/fake_flash_memory.h"
#include "pw_kvs/flash_memory.h"
#include "pw_kvs/test_key_value_store.h"
#include "pw_log/log.h"
#include "pw_random/xor_shift.h"

namespace pw::blob_store {
namespace {

class DeferredWriteTest : public ::testing::Test {
 protected:
  DeferredWriteTest() : flash_(kFlashAlignment), partition_(&flash_) {}

  void InitFlashTo(std::span<const std::byte> contents) {
    partition_.Erase();
    std::memcpy(flash_.buffer().data(), contents.data(), contents.size());
  }

  void InitBufferToRandom(uint64_t seed) {
    partition_.Erase();
    random::XorShiftStarRng64 rng(seed);
    rng.Get(buffer_);
  }

  void InitBufferToFill(char fill) {
    partition_.Erase();
    std::memset(buffer_.data(), fill, buffer_.size());
  }

  // Fill the source buffer with random pattern based on given seed, written to
  // BlobStore in specified chunk size.
  void ChunkWriteTest(size_t chunk_size, size_t flush_interval) {
    constexpr size_t kWriteSize = 64;
    kvs::ChecksumCrc16 checksum;

    size_t bytes_since_flush = 0;

    char name[16] = {};
    snprintf(name, sizeof(name), "Blob%u", static_cast<unsigned>(chunk_size));

    BlobStoreBuffer<kBufferSize> blob(
        name, partition_, &checksum, kvs::TestKvs(), kWriteSize);
    EXPECT_EQ(Status::Ok(), blob.Init());

    BlobStore::DeferredWriter writer(blob);
    EXPECT_EQ(Status::Ok(), writer.Open());

    ByteSpan source = buffer_;
    while (source.size_bytes() > 0) {
      const size_t write_size = std::min(source.size_bytes(), chunk_size);

      PW_LOG_DEBUG("Do write of %u bytes, %u bytes remain",
                   static_cast<unsigned>(write_size),
                   static_cast<unsigned>(source.size_bytes()));

      ASSERT_EQ(Status::Ok(), writer.Write(source.first(write_size)));
      // TODO: Add check that the write did not go to flash yet.

      source = source.subspan(write_size);
      bytes_since_flush += write_size;

      if (bytes_since_flush >= flush_interval) {
        bytes_since_flush = 0;
        ASSERT_EQ(Status::Ok(), writer.Flush());
      }
    }

    EXPECT_EQ(Status::Ok(), writer.Close());

    // Use reader to check for valid data.
    BlobStore::BlobReader reader(blob);
    ASSERT_EQ(Status::Ok(), reader.Open());
    Result<ConstByteSpan> result = reader.GetMemoryMappedBlob();
    ASSERT_TRUE(result.ok());
    VerifyFlash(result.value());
    EXPECT_EQ(Status::Ok(), reader.Close());
  }

  void VerifyFlash(ConstByteSpan verify_bytes) {
    // Should be defined as same size.
    EXPECT_EQ(buffer_.size(), flash_.buffer().size_bytes());

    // Can't allow it to march off the end of buffer_.
    ASSERT_LE(verify_bytes.size_bytes(), buffer_.size());

    for (size_t i = 0; i < verify_bytes.size_bytes(); i++) {
      EXPECT_EQ(buffer_[i], verify_bytes[i]);
    }
  }

  static constexpr size_t kFlashAlignment = 16;
  static constexpr size_t kSectorSize = 1024;
  static constexpr size_t kSectorCount = 4;
  static constexpr size_t kBufferSize = 2 * kSectorSize;

  kvs::FakeFlashMemoryBuffer<kSectorSize, kSectorCount> flash_;
  kvs::FlashPartition partition_;
  std::array<std::byte, kSectorCount * kSectorSize> buffer_;
};

TEST_F(DeferredWriteTest, ChunkWrite1) {
  InitBufferToRandom(0x8675309);
  ChunkWriteTest(1, 16);
}

TEST_F(DeferredWriteTest, ChunkWrite2) {
  InitBufferToRandom(0x8675);
  ChunkWriteTest(2, 16);
}

TEST_F(DeferredWriteTest, ChunkWrite3) {
  InitBufferToFill(0);
  ChunkWriteTest(3, 16);
}

TEST_F(DeferredWriteTest, ChunkWrite4) {
  InitBufferToFill(1);
  ChunkWriteTest(4, 64);
}

TEST_F(DeferredWriteTest, ChunkWrite5) {
  InitBufferToFill(0xff);
  ChunkWriteTest(5, 64);
}

TEST_F(DeferredWriteTest, ChunkWrite16) {
  InitBufferToRandom(0x86);
  ChunkWriteTest(16, 128);
}

TEST_F(DeferredWriteTest, ChunkWrite64) {
  InitBufferToRandom(0x9);
  ChunkWriteTest(64, 128);
}

TEST_F(DeferredWriteTest, ChunkWrite64FullBufferFill) {
  InitBufferToRandom(0x9);
  ChunkWriteTest(64, kBufferSize);
}

TEST_F(DeferredWriteTest, ChunkWrite256) {
  InitBufferToRandom(0x12345678);
  ChunkWriteTest(256, 256);
}

// TODO: test that has dirty flash, invalidated blob, open writer, invalidate
// (not erase) and start writing (does the auto/implicit erase).

// TODO: test that has dirty flash, invalidated blob, open writer, explicit
// erase and start writing.

// TODO: test start with dirty flash/invalid blob, open writer, write, close.
// Verifies erase logic when write buffer has contents.

}  // namespace
}  // namespace pw::blob_store
