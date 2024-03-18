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

#include "pw_allocator/testing.h"
#include "pw_assert/check.h"
#include "pw_bytes/suffix.h"
#include "pw_multibuf/header_chunk_region_tracker.h"
#include "pw_span/span.h"
#include "pw_unit_test/framework.h"

namespace pw::multibuf {
namespace {

using ::pw::allocator::test::AllocatorForTest;

// Arbitrary size intended to be large enough to store the Chunk and data
// slices. This may be increased if `MakeChunk` or a Chunk-splitting operation
// fails.
const size_t kArbitraryAllocatorSize = 2048;
const size_t kArbitraryChunkSize = 32;

#if __cplusplus >= 202002L
static_assert(std::forward_iterator<MultiBuf::iterator>);
static_assert(std::forward_iterator<MultiBuf::const_iterator>);
static_assert(std::forward_iterator<MultiBuf::ChunkIterator>);
static_assert(std::forward_iterator<MultiBuf::ConstChunkIterator>);
#endif  // __cplusplus >= 202002L

OwnedChunk MakeChunk(pw::allocator::Allocator& allocator, size_t size) {
  std::optional<OwnedChunk> chunk =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(allocator, size);
  // If this check fails, `kArbitraryAllocatorSize` above may need increasing.
  PW_CHECK(chunk.has_value());
  return std::move(*chunk);
}

OwnedChunk MakeChunk(pw::allocator::Allocator& allocator,
                     std::initializer_list<std::byte> data) {
  std::optional<OwnedChunk> chunk =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(allocator, data.size());
  // If this check fails, `kArbitraryAllocatorSize` above may need increasing.
  PW_CHECK(chunk.has_value());
  std::copy(data.begin(), data.end(), (*chunk)->begin());
  return std::move(*chunk);
}

OwnedChunk MakeChunk(pw::allocator::Allocator& allocator,
                     pw::span<const std::byte> data) {
  std::optional<OwnedChunk> chunk =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(allocator, data.size());
  // If this check fails, `kArbitraryAllocatorSize` above may need increasing.
  PW_CHECK(chunk.has_value());
  std::copy(data.begin(), data.end(), (*chunk)->begin());
  return std::move(*chunk);
}

template <typename ActualIterable, typename ExpectedIterable>
void ExpectElementsEqual(const ActualIterable& actual,
                         const ExpectedIterable& expected) {
  EXPECT_EQ(actual.size(), expected.size());
  auto actual_iter = actual.begin();
  auto expected_iter = expected.begin();
  for (; expected_iter != expected.end(); ++actual_iter, ++expected_iter) {
    ASSERT_NE(actual_iter, actual.end());
    EXPECT_EQ(*actual_iter, *expected_iter);
  }
}

template <typename ActualIterable, typename T>
void ExpectElementsEqual(const ActualIterable& actual,
                         std::initializer_list<T> expected) {
  ExpectElementsEqual<ActualIterable, std::initializer_list<T>>(actual,
                                                                expected);
}

TEST(MultiBuf, IsDefaultConstructible) { [[maybe_unused]] MultiBuf buf; }

TEST(MultiBuf, WithOneChunkReleases) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  const allocator::AllMetrics& metrics = allocator.metrics();
  MultiBuf buf;
  buf.PushFrontChunk(MakeChunk(allocator, kArbitraryChunkSize));
  EXPECT_EQ(metrics.num_allocations.value(), 2U);
  buf.Release();
  EXPECT_EQ(metrics.num_deallocations.value(), 2U);
}

TEST(MultiBuf, WithOneChunkReleasesOnDestruction) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  const allocator::AllMetrics& metrics = allocator.metrics();
  {
    MultiBuf buf;
    buf.PushFrontChunk(MakeChunk(allocator, kArbitraryChunkSize));
    EXPECT_EQ(metrics.num_allocations.value(), 2U);
  }
  EXPECT_EQ(metrics.num_deallocations.value(), 2U);
}

TEST(MultiBuf, WithMultipleChunksReleasesAllOnDestruction) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  const allocator::AllMetrics& metrics = allocator.metrics();
  {
    MultiBuf buf;
    buf.PushFrontChunk(MakeChunk(allocator, kArbitraryChunkSize));
    buf.PushFrontChunk(MakeChunk(allocator, kArbitraryChunkSize));
    EXPECT_EQ(metrics.num_allocations.value(), 4U);
  }
  EXPECT_EQ(metrics.num_deallocations.value(), 4U);
}

TEST(MultiBuf, SizeReturnsNumberOfBytes) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  EXPECT_EQ(buf.size(), 0U);
  buf.PushFrontChunk(MakeChunk(allocator, kArbitraryChunkSize));
  EXPECT_EQ(buf.size(), kArbitraryChunkSize);
  buf.PushFrontChunk(MakeChunk(allocator, kArbitraryChunkSize));
  EXPECT_EQ(buf.size(), kArbitraryChunkSize * 2);
}

TEST(MultiBuf, EmptyIfNoChunks) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  EXPECT_EQ(buf.size(), 0U);
  EXPECT_TRUE(buf.empty());
  buf.PushFrontChunk(MakeChunk(allocator, kArbitraryChunkSize));
  EXPECT_NE(buf.size(), 0U);
  EXPECT_FALSE(buf.empty());
}

TEST(MultiBuf, EmptyIfOnlyEmptyChunks) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  EXPECT_TRUE(buf.empty());
  buf.PushFrontChunk(MakeChunk(allocator, 0));
  EXPECT_TRUE(buf.empty());
  buf.PushFrontChunk(MakeChunk(allocator, 0));
  EXPECT_TRUE(buf.empty());
  EXPECT_EQ(buf.size(), 0U);
}

TEST(MultiBuf, EmptyIsFalseIfAnyNonEmptyChunks) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  buf.PushFrontChunk(MakeChunk(allocator, 0));
  EXPECT_TRUE(buf.empty());
  buf.PushFrontChunk(MakeChunk(allocator, kArbitraryChunkSize));
  EXPECT_FALSE(buf.empty());
  EXPECT_EQ(buf.size(), kArbitraryChunkSize);
}

TEST(MultiBuf, ClaimPrefixReclaimsFirstChunkPrefix) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  OwnedChunk chunk = MakeChunk(allocator, 16);
  chunk->DiscardPrefix(7);
  buf.PushFrontChunk(std::move(chunk));
  EXPECT_EQ(buf.size(), 9U);
  EXPECT_EQ(buf.ClaimPrefix(7), true);
  EXPECT_EQ(buf.size(), 16U);
}

TEST(MultiBuf, ClaimPrefixOnFirstChunkWithoutPrefixReturnsFalse) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  buf.PushFrontChunk(MakeChunk(allocator, 16));
  EXPECT_EQ(buf.size(), 16U);
  EXPECT_EQ(buf.ClaimPrefix(7), false);
  EXPECT_EQ(buf.size(), 16U);
}

TEST(MultiBuf, ClaimPrefixWithoutChunksReturnsFalse) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  EXPECT_EQ(buf.size(), 0U);
  EXPECT_EQ(buf.ClaimPrefix(7), false);
  EXPECT_EQ(buf.size(), 0U);
}

TEST(MultiBuf, ClaimSuffixReclaimsLastChunkSuffix) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  OwnedChunk chunk = MakeChunk(allocator, 16U);
  chunk->Truncate(9U);
  buf.PushFrontChunk(std::move(chunk));
  buf.PushFrontChunk(MakeChunk(allocator, 4U));
  EXPECT_EQ(buf.size(), 13U);
  EXPECT_EQ(buf.ClaimSuffix(7U), true);
  EXPECT_EQ(buf.size(), 20U);
}

TEST(MultiBuf, ClaimSuffixOnLastChunkWithoutSuffixReturnsFalse) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  buf.PushFrontChunk(MakeChunk(allocator, 16U));
  EXPECT_EQ(buf.size(), 16U);
  EXPECT_EQ(buf.ClaimPrefix(7U), false);
  EXPECT_EQ(buf.size(), 16U);
}

TEST(MultiBuf, ClaimSuffixWithoutChunksReturnsFalse) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  EXPECT_EQ(buf.size(), 0U);
  EXPECT_EQ(buf.ClaimSuffix(7U), false);
  EXPECT_EQ(buf.size(), 0U);
}

TEST(MultiBuf, DiscardPrefixWithZeroDoesNothing) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  buf.DiscardPrefix(0);
  EXPECT_EQ(buf.size(), 0U);
}

TEST(MultiBuf, DiscardPrefixDiscardsPartialChunk) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  buf.PushFrontChunk(MakeChunk(allocator, 16U));
  buf.DiscardPrefix(5U);
  EXPECT_EQ(buf.size(), 11U);
}

TEST(MultiBuf, DiscardPrefixDiscardsWholeChunk) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  buf.PushFrontChunk(MakeChunk(allocator, 16U));
  buf.PushFrontChunk(MakeChunk(allocator, 3U));
  buf.DiscardPrefix(16U);
  EXPECT_EQ(buf.size(), 3U);
}

TEST(MultiBuf, DiscardPrefixDiscardsMultipleChunks) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  buf.PushFrontChunk(MakeChunk(allocator, 16U));
  buf.PushFrontChunk(MakeChunk(allocator, 4U));
  buf.PushFrontChunk(MakeChunk(allocator, 3U));
  buf.DiscardPrefix(21U);
  EXPECT_EQ(buf.size(), 2U);
}

TEST(MultiBuf, SliceDiscardsPrefixAndSuffixWholeAndPartialChunks) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  buf.PushBackChunk(MakeChunk(allocator, {1_b, 1_b, 1_b}));
  buf.PushBackChunk(MakeChunk(allocator, {2_b, 2_b, 2_b}));
  buf.PushBackChunk(MakeChunk(allocator, {3_b, 3_b, 3_b}));
  buf.PushBackChunk(MakeChunk(allocator, {4_b, 4_b, 4_b}));
  buf.Slice(4, 7);
  ExpectElementsEqual(buf, {2_b, 2_b, 3_b});
}

TEST(MultiBuf, SliceDoesNotModifyChunkMemory) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  std::array<std::byte, 4> kBytes = {1_b, 2_b, 3_b, 4_b};
  OwnedChunk chunk = MakeChunk(allocator, kBytes);
  ConstByteSpan span(chunk);
  buf.PushFrontChunk(std::move(chunk));
  buf.Slice(2, 3);
  ExpectElementsEqual(span, kBytes);
}

TEST(MultiBuf, TruncateRemovesWholeAndPartialChunks) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  buf.PushFrontChunk(MakeChunk(allocator, 3U));
  buf.PushFrontChunk(MakeChunk(allocator, 3U));
  buf.Truncate(2U);
  EXPECT_EQ(buf.size(), 2U);
}

TEST(MultiBuf, TakePrefixWithNoBytesDoesNothing) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  std::optional<MultiBuf> empty_front = buf.TakePrefix(0);
  ASSERT_TRUE(empty_front.has_value());
  EXPECT_EQ(buf.size(), 0U);
  EXPECT_EQ(empty_front->size(), 0U);
}

TEST(MultiBuf, TakePrefixReturnsPartialChunk) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  buf.PushBackChunk(MakeChunk(allocator, {1_b, 2_b, 3_b}));
  std::optional<MultiBuf> old_front = buf.TakePrefix(2);
  ASSERT_TRUE(old_front.has_value());
  ExpectElementsEqual(*old_front, {1_b, 2_b});
  ExpectElementsEqual(buf, {3_b});
}

TEST(MultiBuf, TakePrefixReturnsWholeAndPartialChunks) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  buf.PushBackChunk(MakeChunk(allocator, {1_b, 2_b, 3_b}));
  buf.PushBackChunk(MakeChunk(allocator, {4_b, 5_b, 6_b}));
  std::optional<MultiBuf> old_front = buf.TakePrefix(4);
  ASSERT_TRUE(old_front.has_value());
  ExpectElementsEqual(*old_front, {1_b, 2_b, 3_b, 4_b});
  ExpectElementsEqual(buf, {5_b, 6_b});
}

TEST(MultiBuf, TakeSuffixReturnsWholeAndPartialChunks) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  buf.PushBackChunk(MakeChunk(allocator, {1_b, 2_b, 3_b}));
  buf.PushBackChunk(MakeChunk(allocator, {4_b, 5_b, 6_b}));
  std::optional<MultiBuf> old_tail = buf.TakeSuffix(4);
  ASSERT_TRUE(old_tail.has_value());
  ExpectElementsEqual(buf, {1_b, 2_b});
  ExpectElementsEqual(*old_tail, {3_b, 4_b, 5_b, 6_b});
}

TEST(MultiBuf, PushPrefixPrependsData) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  buf.PushBackChunk(MakeChunk(allocator, {1_b, 2_b, 3_b}));
  buf.PushBackChunk(MakeChunk(allocator, {4_b, 5_b, 6_b}));
  MultiBuf buf2;
  buf2.PushBackChunk(MakeChunk(allocator, {7_b, 8_b}));
  buf2.PushPrefix(std::move(buf));
  ExpectElementsEqual(buf2, {1_b, 2_b, 3_b, 4_b, 5_b, 6_b, 7_b, 8_b});
}

TEST(MultiBuf, PushSuffixAppendsData) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  buf.PushBackChunk(MakeChunk(allocator, {1_b, 2_b, 3_b}));
  buf.PushBackChunk(MakeChunk(allocator, {4_b, 5_b, 6_b}));
  MultiBuf buf2;
  buf2.PushBackChunk(MakeChunk(allocator, {7_b, 8_b}));
  buf2.PushSuffix(std::move(buf));
  ExpectElementsEqual(buf2, {7_b, 8_b, 1_b, 2_b, 3_b, 4_b, 5_b, 6_b});
}

TEST(MultiBuf, PushFrontChunkAddsBytesToFront) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;

  const std::array<std::byte, 3> kBytesOne = {0_b, 1_b, 2_b};
  auto chunk_one = MakeChunk(allocator, kBytesOne);
  buf.PushFrontChunk(std::move(chunk_one));
  ExpectElementsEqual(buf, kBytesOne);

  const std::array<std::byte, 4> kBytesTwo = {9_b, 10_b, 11_b, 12_b};
  auto chunk_two = MakeChunk(allocator, kBytesTwo);
  buf.PushFrontChunk(std::move(chunk_two));

  // clang-format off
  ExpectElementsEqual(buf, {
      9_b, 10_b, 11_b, 12_b,
      0_b, 1_b, 2_b,
  });
  // clang-format on
}

TEST(MultiBuf, InsertChunkOnEmptyBufAddsFirstChunk) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;

  const std::array<std::byte, 3> kBytes = {0_b, 1_b, 2_b};
  auto chunk = MakeChunk(allocator, kBytes);
  auto inserted_iter = buf.InsertChunk(buf.Chunks().begin(), std::move(chunk));
  EXPECT_EQ(inserted_iter, buf.Chunks().begin());
  ExpectElementsEqual(buf, kBytes);
  EXPECT_EQ(++inserted_iter, buf.Chunks().end());
}

TEST(MultiBuf, InsertChunkAtEndOfBufAddsLastChunk) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;

  // Add a chunk to the beginning
  buf.PushFrontChunk(MakeChunk(allocator, kArbitraryChunkSize));

  const std::array<std::byte, 3> kBytes = {0_b, 1_b, 2_b};
  auto chunk = MakeChunk(allocator, kBytes);
  auto inserted_iter = buf.InsertChunk(buf.Chunks().end(), std::move(chunk));
  EXPECT_EQ(inserted_iter, ++buf.Chunks().begin());
  EXPECT_EQ(++inserted_iter, buf.Chunks().end());
  const Chunk& second_chunk = *(++buf.Chunks().begin());
  ExpectElementsEqual(second_chunk, kBytes);
}

TEST(MultiBuf, TakeChunkAtBeginRemovesAndReturnsFirstChunk) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  auto insert_iter = buf.Chunks().begin();
  insert_iter = buf.InsertChunk(insert_iter, MakeChunk(allocator, 2));
  insert_iter = buf.InsertChunk(++insert_iter, MakeChunk(allocator, 4));

  auto [chunk_iter, chunk] = buf.TakeChunk(buf.Chunks().begin());
  EXPECT_EQ(chunk.size(), 2U);
  EXPECT_EQ(chunk_iter->size(), 4U);
  ++chunk_iter;
  EXPECT_EQ(chunk_iter, buf.Chunks().end());
}

TEST(MultiBuf, TakeChunkOnLastInsertedIterReturnsLastInserted) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  auto iter = buf.Chunks().begin();
  iter = buf.InsertChunk(iter, MakeChunk(allocator, 42));
  iter = buf.InsertChunk(++iter, MakeChunk(allocator, 11));
  iter = buf.InsertChunk(++iter, MakeChunk(allocator, 65));
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

TEST(MultiBuf, IteratorAdvancesNAcrossChunks) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  buf.PushBackChunk(MakeChunk(allocator, {1_b, 2_b, 3_b}));
  buf.PushBackChunk(MakeChunk(allocator, {4_b, 5_b, 6_b}));

  MultiBuf::iterator iter = buf.begin();
  iter += 4;
  EXPECT_EQ(*iter, 5_b);
}

TEST(MultiBuf, IteratorAdvancesNAcrossZeroLengthChunk) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  buf.PushBackChunk(MakeChunk(allocator, {1_b, 2_b, 3_b}));
  buf.PushBackChunk(MakeChunk(allocator, 0));
  buf.PushBackChunk(MakeChunk(allocator, {4_b, 5_b, 6_b}));

  MultiBuf::iterator iter = buf.begin();
  iter += 4;
  EXPECT_EQ(*iter, 5_b);
}

TEST(MultiBuf, ConstIteratorAdvancesNAcrossChunks) {
  AllocatorForTest<kArbitraryAllocatorSize> allocator;
  MultiBuf buf;
  buf.PushBackChunk(MakeChunk(allocator, {1_b, 2_b, 3_b}));
  buf.PushBackChunk(MakeChunk(allocator, {4_b, 5_b, 6_b}));

  MultiBuf::const_iterator iter = buf.cbegin();
  iter += 4;
  EXPECT_EQ(*iter, 5_b);
}

}  // namespace
}  // namespace pw::multibuf
