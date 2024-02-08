// Copyright 2023 The Pigweed Authors
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

#include "pw_multibuf/multibuf.h"

#include "pw_allocator/allocator_testing.h"
#include "pw_assert/check.h"
#include "pw_bytes/suffix.h"
#include "pw_multibuf/chunk_region_tracker.h"
#include "pw_unit_test/framework.h"

namespace pw::multibuf {
namespace {

using ::pw::allocator::test::AllocatorForTest;

const size_t kArbitraryAllocatorSize = 1024;
const size_t kArbitraryChunkSize = 32;

#if __cplusplus >= 202002L
static_assert(std::forward_iterator<MultiBuf::iterator>);
static_assert(std::forward_iterator<MultiBuf::const_iterator>);
static_assert(std::forward_iterator<MultiBuf::ChunkIterator>);
static_assert(std::forward_iterator<MultiBuf::ConstChunkIterator>);
#endif  // __cplusplus >= 202002L

OwnedChunk MakeChunk(pw::allocator::Allocator* allocator, size_t size) {
  std::optional<OwnedChunk> chunk =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(allocator, size);
  PW_CHECK(chunk.has_value());
  return std::move(*chunk);
}

TEST(MultiBuf, IsDefaultConstructible) { [[maybe_unused]] MultiBuf buf; }

TEST(MultiBuf, WithOneChunkReleases) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  buf.PushFrontChunk(MakeChunk(&allocator, kArbitraryChunkSize));
  EXPECT_EQ(allocator.num_allocations(), 2U);
  buf.Release();
  EXPECT_EQ(allocator.num_deallocations(), 2U);
}

TEST(MultiBuf, WithOneChunkReleasesOnDestruction) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  {
    MultiBuf buf;
    buf.PushFrontChunk(MakeChunk(&allocator, kArbitraryChunkSize));
    EXPECT_EQ(allocator.num_allocations(), 2U);
  }
  EXPECT_EQ(allocator.num_deallocations(), 2U);
}

TEST(MultiBuf, WithMultipleChunksReleasesAllOnDestruction) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  {
    MultiBuf buf;
    buf.PushFrontChunk(MakeChunk(&allocator, kArbitraryChunkSize));
    buf.PushFrontChunk(MakeChunk(&allocator, kArbitraryChunkSize));
    EXPECT_EQ(allocator.num_allocations(), 4U);
  }
  EXPECT_EQ(allocator.num_deallocations(), 4U);
}

TEST(MultiBuf, SizeReturnsNumberOfBytes) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  EXPECT_EQ(buf.size(), 0U);
  buf.PushFrontChunk(MakeChunk(&allocator, kArbitraryChunkSize));
  EXPECT_EQ(buf.size(), kArbitraryChunkSize);
  buf.PushFrontChunk(MakeChunk(&allocator, kArbitraryChunkSize));
  EXPECT_EQ(buf.size(), kArbitraryChunkSize * 2);
}

TEST(MultiBuf, PushFrontChunkAddsBytesToFront) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;

  const std::array<std::byte, 3> kBytesOne = {0_b, 1_b, 2_b};
  auto chunk_one = MakeChunk(&allocator, kBytesOne.size());
  std::copy(kBytesOne.begin(), kBytesOne.end(), chunk_one->begin());
  buf.PushFrontChunk(std::move(chunk_one));

  size_t i = 0;
  auto buf_iter = buf.begin();
  for (; i < kBytesOne.size(); i++, buf_iter++) {
    ASSERT_NE(buf_iter, buf.end());
    EXPECT_EQ(*buf_iter, kBytesOne[i]);
  }

  const std::array<std::byte, 4> kBytesTwo = {9_b, 10_b, 11_b, 12_b};
  auto chunk_two = MakeChunk(&allocator, kBytesTwo.size());
  std::copy(kBytesTwo.begin(), kBytesTwo.end(), chunk_two->begin());
  buf.PushFrontChunk(std::move(chunk_two));

  std::array<std::byte, 7> expected = {
      9_b,
      10_b,
      11_b,
      12_b,
      0_b,
      1_b,
      2_b,
  };
  i = 0;
  buf_iter = buf.begin();
  for (; i < expected.size(); i++, buf_iter++) {
    ASSERT_NE(buf_iter, buf.end());
    EXPECT_EQ(*buf_iter, expected[i]);
  }
}

TEST(MultiBuf, InsertChunkOnEmptyBufAddsFirstChunk) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;

  const std::array<std::byte, 3> kBytes = {0_b, 1_b, 2_b};
  auto chunk = MakeChunk(&allocator, kBytes.size());
  std::copy(kBytes.begin(), kBytes.end(), chunk->begin());
  auto inserted_iter = buf.InsertChunk(buf.Chunks().begin(), std::move(chunk));
  EXPECT_EQ(inserted_iter, buf.Chunks().begin());

  size_t i = 0;
  auto buf_iter = buf.begin();
  for (; i < kBytes.size(); i++, buf_iter++) {
    ASSERT_NE(buf_iter, buf.end());
    EXPECT_EQ(*buf_iter, kBytes[i]);
  }

  EXPECT_EQ(++inserted_iter, buf.Chunks().end());
}

TEST(MultiBuf, InsertChunkAtEndOfBufAddsLastChunk) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;

  // Add a chunk to the beginning
  buf.PushFrontChunk(MakeChunk(&allocator, kArbitraryChunkSize));

  const std::array<std::byte, 3> kBytes = {0_b, 1_b, 2_b};
  auto chunk = MakeChunk(&allocator, kBytes.size());
  std::copy(kBytes.begin(), kBytes.end(), chunk->begin());
  auto inserted_iter = buf.InsertChunk(buf.Chunks().end(), std::move(chunk));
  EXPECT_EQ(inserted_iter, ++buf.Chunks().begin());
  EXPECT_EQ(++inserted_iter, buf.Chunks().end());

  size_t i = 0;
  auto buf_iter = buf.Chunks().begin();
  buf_iter++;
  auto chunk_iter = buf_iter->begin();
  for (; i < kBytes.size(); i++, chunk_iter++) {
    ASSERT_NE(chunk_iter, buf_iter->end());
    EXPECT_EQ(*chunk_iter, kBytes[i]);
  }
}

TEST(MultiBuf, TakeChunkAtBeginRemovesAndReturnsFirstChunk) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  auto insert_iter = buf.Chunks().begin();
  insert_iter = buf.InsertChunk(insert_iter, MakeChunk(&allocator, 2));
  insert_iter = buf.InsertChunk(++insert_iter, MakeChunk(&allocator, 4));

  auto [chunk_iter, chunk] = buf.TakeChunk(buf.Chunks().begin());
  EXPECT_EQ(chunk.size(), 2U);
  EXPECT_EQ(chunk_iter->size(), 4U);
  chunk_iter++;
  EXPECT_EQ(chunk_iter, buf.Chunks().end());
}

TEST(MultiBuf, TakeChunkOnLastInsertedIterReturnsLastInserted) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  auto iter = buf.Chunks().begin();
  iter = buf.InsertChunk(iter, MakeChunk(&allocator, 42));
  iter = buf.InsertChunk(++iter, MakeChunk(&allocator, 11));
  iter = buf.InsertChunk(++iter, MakeChunk(&allocator, 65));
  OwnedChunk chunk;
  std::tie(iter, chunk) = buf.TakeChunk(iter);
  EXPECT_EQ(iter, buf.Chunks().end());
  EXPECT_EQ(chunk.size(), 65U);
}

TEST(MultiBuf, RangeBasedForLoopsCompile) {
  MultiBuf buf;
  for ([[maybe_unused]] std::byte& byte : buf) {
  }
  for ([[maybe_unused]] const std::byte& byte : buf) {
  }
  for ([[maybe_unused]] Chunk& chunk : buf.Chunks()) {
  }
  for ([[maybe_unused]] const Chunk& chunk : buf.Chunks()) {
  }

  const MultiBuf const_buf;
  for ([[maybe_unused]] const std::byte& byte : const_buf) {
  }
  for ([[maybe_unused]] const Chunk& chunk : const_buf.Chunks()) {
  }
}

}  // namespace
}  // namespace pw::multibuf
