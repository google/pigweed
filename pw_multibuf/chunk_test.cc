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

#include "pw_multibuf/chunk.h"

#include <memory>

#if __cplusplus >= 202002L
#include <ranges>
#endif  // __cplusplus >= 202002L

#include "gtest/gtest.h"
#include "pw_allocator/allocator_metric_proxy.h"
#include "pw_allocator/split_free_list_allocator.h"

namespace pw::multibuf {
namespace {

class TrackingAllocator : public pw::allocator::Allocator {
 public:
  TrackingAllocator(ByteSpan span) : alloc_stats_(kFakeToken) {
    Status status = alloc_.Init(span, kFakeThreshold);
    EXPECT_EQ(status, OkStatus());
    alloc_stats_.Initialize(alloc_);
  }

  size_t count() const { return alloc_stats_.count(); }
  size_t used() const { return alloc_stats_.used(); }

 protected:
  void* DoAllocate(size_t size, size_t alignment) override {
    return alloc_stats_.AllocateUnchecked(size, alignment);
  }
  bool DoResize(void* ptr,
                size_t old_size,
                size_t old_align,
                size_t new_size) override {
    return alloc_stats_.ResizeUnchecked(ptr, old_size, old_align, new_size);
  }
  void DoDeallocate(void* ptr, size_t size, size_t alignment) override {
    alloc_stats_.DeallocateUnchecked(ptr, size, alignment);
  }

 private:
  const size_t kFakeThreshold = 0;
  const int32_t kFakeToken = 0;

  pw::allocator::SplitFreeListAllocator<> alloc_;
  pw::allocator::AllocatorMetricProxy alloc_stats_;
};

template <auto NumBytes>
class TrackingAllocatorWithMemory : public pw::allocator::Allocator {
 public:
  TrackingAllocatorWithMemory() : mem_(), alloc_(mem_) {}
  size_t count() const { return alloc_.count(); }
  size_t used() const { return alloc_.used(); }
  void* DoAllocate(size_t size, size_t alignment) override {
    return alloc_.AllocateUnchecked(size, alignment);
  }
  bool DoResize(void* ptr,
                size_t old_size,
                size_t old_align,
                size_t new_size) override {
    return alloc_.ResizeUnchecked(ptr, old_size, old_align, new_size);
  }
  void DoDeallocate(void* ptr, size_t size, size_t alignment) override {
    alloc_.DeallocateUnchecked(ptr, size, alignment);
  }

 private:
  std::array<std::byte, NumBytes> mem_;
  TrackingAllocator alloc_;
};

class HeaderChunkRegionTracker final : public ChunkRegionTracker {
 public:
  static std::optional<OwnedChunk> AllocateRegionAsChunk(
      pw::allocator::Allocator* alloc, size_t size) {
    HeaderChunkRegionTracker* tracker = AllocateRegion(alloc, size);
    if (tracker == nullptr) {
      return std::nullopt;
    }
    std::optional<OwnedChunk> chunk = Chunk::CreateFirstForRegion(*tracker);
    if (!chunk.has_value()) {
      tracker->Destroy();
      return std::nullopt;
    }
    return chunk;
  }

  static HeaderChunkRegionTracker* AllocateRegion(
      pw::allocator::Allocator* alloc, size_t size) {
    size_t alloc_size = size + sizeof(HeaderChunkRegionTracker);
    size_t align = alignof(HeaderChunkRegionTracker);
    void* ptr = alloc->AllocateUnchecked(alloc_size, align);
    if (ptr == nullptr) {
      return nullptr;
    }
    std::byte* data =
        reinterpret_cast<std::byte*>(ptr) + sizeof(HeaderChunkRegionTracker);
    return new (ptr) HeaderChunkRegionTracker(ByteSpan(data, size), alloc);
  }

  ByteSpan Region() const final { return region_; }
  ~HeaderChunkRegionTracker() final {}

 protected:
  void Destroy() final {
    std::byte* ptr = reinterpret_cast<std::byte*>(this);
    size_t size = sizeof(HeaderChunkRegionTracker) + region_.size();
    size_t align = alignof(HeaderChunkRegionTracker);
    auto alloc = alloc_;
    std::destroy_at(this);
    alloc->DeallocateUnchecked(ptr, size, align);
  }
  void* AllocateChunkClass() final {
    return alloc_->Allocate(pw::allocator::Layout::Of<Chunk>());
  }
  void DeallocateChunkClass(void* ptr) final {
    alloc_->Deallocate(ptr, pw::allocator::Layout::Of<Chunk>());
  }

 private:
  ByteSpan region_;
  pw::allocator::Allocator* alloc_;

  // NOTE: `region` must directly follow this `FakeChunkRegionTracker`
  // in memory allocated by allocated by `alloc`.
  HeaderChunkRegionTracker(ByteSpan region, pw::allocator::Allocator* alloc)
      : region_(region), alloc_(alloc) {}
};

/// Returns literal with ``_size`` suffix as a ``size_t``.
///
/// This is useful for writing size-related test assertions without
/// explicit (verbose) casts.
constexpr size_t operator"" _size(unsigned long long n) { return n; }

const size_t kArbitraryAllocatorSize = 1024;
const size_t kArbitraryChunkSize = 32;

#if __cplusplus >= 202002L
static_assert(std::ranges::contiguous_range<Chunk>);
#endif  // __cplusplus >= 202002L

void TakesSpan([[maybe_unused]] ByteSpan span) {}

TEST(Chunk, IsImplicitlyConvertibleToSpan) {
  TrackingAllocatorWithMemory<kArbitraryAllocatorSize> alloc;
  std::optional<OwnedChunk> chunk =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryChunkSize);
  ASSERT_TRUE(chunk.has_value());
  // ``Chunk`` should convert to ``ByteSpan``.
  TakesSpan(**chunk);
}

TEST(OwnedChunk, ReleaseDestroysChunkRegion) {
  TrackingAllocatorWithMemory<kArbitraryAllocatorSize> alloc;
  auto tracker =
      HeaderChunkRegionTracker::AllocateRegion(&alloc, kArbitraryChunkSize);
  ASSERT_NE(tracker, nullptr);
  EXPECT_EQ(alloc.count(), 1_size);

  std::optional<OwnedChunk> chunk_opt = Chunk::CreateFirstForRegion(*tracker);
  ASSERT_TRUE(chunk_opt.has_value());
  auto& chunk = *chunk_opt;
  EXPECT_EQ(alloc.count(), 2_size);
  EXPECT_EQ(chunk.size(), kArbitraryChunkSize);

  chunk.Release();
  EXPECT_EQ(chunk.size(), 0_size);
  EXPECT_EQ(alloc.count(), 0_size);
  EXPECT_EQ(alloc.used(), 0_size);
}

TEST(OwnedChunk, DestructorDestroysChunkRegion) {
  TrackingAllocatorWithMemory<kArbitraryAllocatorSize> alloc;
  auto tracker =
      HeaderChunkRegionTracker::AllocateRegion(&alloc, kArbitraryChunkSize);
  ASSERT_NE(tracker, nullptr);
  EXPECT_EQ(alloc.count(), 1_size);

  {
    std::optional<OwnedChunk> chunk = Chunk::CreateFirstForRegion(*tracker);
    ASSERT_TRUE(chunk.has_value());
    EXPECT_EQ(alloc.count(), 2_size);
    EXPECT_EQ(chunk->size(), kArbitraryChunkSize);
  }

  EXPECT_EQ(alloc.count(), 0_size);
  EXPECT_EQ(alloc.used(), 0_size);
}

TEST(Chunk, DiscardFrontDiscardsFrontOfSpan) {
  TrackingAllocatorWithMemory<kArbitraryAllocatorSize> alloc;
  std::optional<OwnedChunk> chunk_opt =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryChunkSize);
  ASSERT_TRUE(chunk_opt.has_value());
  auto& chunk = *chunk_opt;
  ConstByteSpan old_span = chunk.span();
  const size_t kDiscarded = 4;
  chunk->DiscardFront(kDiscarded);
  EXPECT_EQ(chunk.size(), old_span.size() - kDiscarded);
  EXPECT_EQ(chunk.data(), old_span.data() + kDiscarded);
}

TEST(Chunk, TakeFrontTakesFrontOfSpan) {
  TrackingAllocatorWithMemory<kArbitraryAllocatorSize> alloc;
  std::optional<OwnedChunk> chunk_opt =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryChunkSize);
  ASSERT_TRUE(chunk_opt.has_value());
  auto& chunk = *chunk_opt;
  ConstByteSpan old_span = chunk.span();
  const size_t kTaken = 4;
  std::optional<OwnedChunk> front_opt = chunk->TakeFront(kTaken);
  ASSERT_TRUE(front_opt.has_value());
  auto& front = *front_opt;
  EXPECT_EQ(front->size(), kTaken);
  EXPECT_EQ(front->data(), old_span.data());
  EXPECT_EQ(chunk.size(), old_span.size() - kTaken);
  EXPECT_EQ(chunk.data(), old_span.data() + kTaken);
}

TEST(Chunk, TruncateDiscardsEndOfSpan) {
  TrackingAllocatorWithMemory<kArbitraryAllocatorSize> alloc;
  std::optional<OwnedChunk> chunk_opt =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryChunkSize);
  ASSERT_TRUE(chunk_opt.has_value());
  auto& chunk = *chunk_opt;
  ConstByteSpan old_span = chunk.span();
  const size_t kShorter = 5;
  chunk->Truncate(old_span.size() - kShorter);
  EXPECT_EQ(chunk.size(), old_span.size() - kShorter);
  EXPECT_EQ(chunk.data(), old_span.data());
}

TEST(Chunk, TakeTailTakesEndOfSpan) {
  TrackingAllocatorWithMemory<kArbitraryAllocatorSize> alloc;
  std::optional<OwnedChunk> chunk_opt =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryChunkSize);
  ASSERT_TRUE(chunk_opt.has_value());
  auto& chunk = *chunk_opt;
  ConstByteSpan old_span = chunk.span();
  const size_t kTaken = 5;
  std::optional<OwnedChunk> tail_opt = chunk->TakeTail(kTaken);
  ASSERT_TRUE(tail_opt.has_value());
  auto& tail = *tail_opt;
  EXPECT_EQ(tail.size(), kTaken);
  EXPECT_EQ(tail.data(), old_span.data() + old_span.size() - kTaken);
  EXPECT_EQ(chunk.size(), old_span.size() - kTaken);
  EXPECT_EQ(chunk.data(), old_span.data());
}

TEST(Chunk, SliceRemovesSidesOfSpan) {
  TrackingAllocatorWithMemory<kArbitraryAllocatorSize> alloc;
  std::optional<OwnedChunk> chunk_opt =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryChunkSize);
  ASSERT_TRUE(chunk_opt.has_value());
  auto& chunk = *chunk_opt;
  ConstByteSpan old_span = chunk.span();
  const size_t kBegin = 4;
  const size_t kEnd = 9;
  chunk->Slice(kBegin, kEnd);
  EXPECT_EQ(chunk.data(), old_span.data() + kBegin);
  EXPECT_EQ(chunk.size(), kEnd - kBegin);
}

TEST(Chunk, RegionPersistsUntilAllChunksReleased) {
  TrackingAllocatorWithMemory<kArbitraryAllocatorSize> alloc;
  std::optional<OwnedChunk> chunk_opt =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryChunkSize);
  ASSERT_TRUE(chunk_opt.has_value());
  auto& chunk = *chunk_opt;
  // One allocation for the region tracker, one for the chunk.
  EXPECT_EQ(alloc.count(), 2_size);
  const size_t kSplitPoint = 13;
  auto split_opt = chunk->TakeFront(kSplitPoint);
  ASSERT_TRUE(split_opt.has_value());
  auto& split = *split_opt;
  // One allocation for the region tracker, one for each of two chunks.
  EXPECT_EQ(alloc.count(), 3_size);
  chunk.Release();
  EXPECT_EQ(alloc.count(), 2_size);
  split.Release();
  EXPECT_EQ(alloc.count(), 0_size);
}

TEST(Chunk, ClaimPrefixReclaimsDiscardedFront) {
  TrackingAllocatorWithMemory<kArbitraryAllocatorSize> alloc;
  std::optional<OwnedChunk> chunk_opt =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryChunkSize);
  ASSERT_TRUE(chunk_opt.has_value());
  auto& chunk = *chunk_opt;
  ConstByteSpan old_span = chunk.span();
  const size_t kDiscarded = 4;
  chunk->DiscardFront(kDiscarded);
  EXPECT_TRUE(chunk->ClaimPrefix(kDiscarded));
  EXPECT_EQ(chunk.size(), old_span.size());
  EXPECT_EQ(chunk.data(), old_span.data());
}

TEST(Chunk, ClaimPrefixFailsOnFullRegionChunk) {
  TrackingAllocatorWithMemory<kArbitraryAllocatorSize> alloc;
  std::optional<OwnedChunk> chunk_opt =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryChunkSize);
  ASSERT_TRUE(chunk_opt.has_value());
  auto& chunk = *chunk_opt;
  EXPECT_FALSE(chunk->ClaimPrefix(1));
}

TEST(Chunk, ClaimPrefixFailsOnNeighboringChunk) {
  TrackingAllocatorWithMemory<kArbitraryAllocatorSize> alloc;
  std::optional<OwnedChunk> chunk_opt =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryChunkSize);
  ASSERT_TRUE(chunk_opt.has_value());
  auto& chunk = *chunk_opt;
  const size_t kSplitPoint = 22;
  auto front = chunk->TakeFront(kSplitPoint);
  ASSERT_TRUE(front.has_value());
  EXPECT_FALSE(chunk->ClaimPrefix(1));
}

TEST(Chunk,
     ClaimPrefixFailsAtStartOfRegionEvenAfterReleasingChunkAtEndOfRegion) {
  TrackingAllocatorWithMemory<kArbitraryAllocatorSize> alloc;
  std::optional<OwnedChunk> chunk_opt =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryChunkSize);
  ASSERT_TRUE(chunk_opt.has_value());
  auto& chunk = *chunk_opt;
  const size_t kTaken = 13;
  auto split = chunk->TakeTail(kTaken);
  ASSERT_TRUE(split.has_value());
  split->Release();
  EXPECT_FALSE(chunk->ClaimPrefix(1));
}

TEST(Chunk, ClaimPrefixReclaimsPrecedingChunksDiscardedSuffix) {
  TrackingAllocatorWithMemory<kArbitraryAllocatorSize> alloc;
  std::optional<OwnedChunk> chunk_opt =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryChunkSize);
  ASSERT_TRUE(chunk_opt.has_value());
  auto& chunk = *chunk_opt;
  const size_t kSplitPoint = 13;
  auto split_opt = chunk->TakeFront(kSplitPoint);
  ASSERT_TRUE(split_opt.has_value());
  auto& split = *split_opt;
  const size_t kDiscard = 3;
  split->Truncate(split.size() - kDiscard);
  EXPECT_TRUE(chunk->ClaimPrefix(kDiscard));
  EXPECT_FALSE(chunk->ClaimPrefix(1));
}

TEST(Chunk, ClaimSuffixReclaimsTruncatedEnd) {
  TrackingAllocatorWithMemory<kArbitraryAllocatorSize> alloc;
  std::optional<OwnedChunk> chunk_opt =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryChunkSize);
  ASSERT_TRUE(chunk_opt.has_value());
  auto& chunk = *chunk_opt;
  ConstByteSpan old_span = chunk->span();
  const size_t kDiscarded = 4;
  chunk->Truncate(old_span.size() - kDiscarded);
  EXPECT_TRUE(chunk->ClaimSuffix(kDiscarded));
  EXPECT_EQ(chunk->size(), old_span.size());
  EXPECT_EQ(chunk->data(), old_span.data());
}

TEST(Chunk, ClaimSuffixFailsOnFullRegionChunk) {
  TrackingAllocatorWithMemory<kArbitraryAllocatorSize> alloc;
  std::optional<OwnedChunk> chunk_opt =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryChunkSize);
  ASSERT_TRUE(chunk_opt.has_value());
  auto& chunk = *chunk_opt;
  EXPECT_FALSE(chunk->ClaimSuffix(1));
}

TEST(Chunk, ClaimSuffixFailsWithNeighboringChunk) {
  TrackingAllocatorWithMemory<kArbitraryAllocatorSize> alloc;
  std::optional<OwnedChunk> chunk_opt =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryChunkSize);
  ASSERT_TRUE(chunk_opt.has_value());
  auto& chunk = *chunk_opt;
  const size_t kSplitPoint = 22;
  auto split_opt = chunk->TakeFront(kSplitPoint);
  ASSERT_TRUE(split_opt.has_value());
  auto& split = *split_opt;
  EXPECT_FALSE(split->ClaimSuffix(1));
}

TEST(Chunk, ClaimSuffixFailsAtEndOfRegionEvenAfterReleasingFirstChunkInRegion) {
  TrackingAllocatorWithMemory<kArbitraryAllocatorSize> alloc;
  std::optional<OwnedChunk> chunk_opt =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryChunkSize);
  ASSERT_TRUE(chunk_opt.has_value());
  auto& chunk = *chunk_opt;
  const size_t kTaken = 22;
  auto split_opt = chunk->TakeTail(kTaken);
  ASSERT_TRUE(split_opt.has_value());
  auto& split = *split_opt;
  EXPECT_FALSE(split->ClaimSuffix(1));
}

TEST(Chunk, ClaimSuffixReclaimsFollowingChunksDiscardedPrefix) {
  TrackingAllocatorWithMemory<kArbitraryAllocatorSize> alloc;
  std::optional<OwnedChunk> chunk_opt =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryChunkSize);
  ASSERT_TRUE(chunk_opt.has_value());
  auto& chunk = *chunk_opt;
  const size_t kSplitPoint = 22;
  auto split_opt = chunk->TakeFront(kSplitPoint);
  ASSERT_TRUE(split_opt.has_value());
  auto& split = *split_opt;
  const size_t kDiscarded = 3;
  chunk->DiscardFront(kDiscarded);
  EXPECT_TRUE(split->ClaimSuffix(kDiscarded));
  EXPECT_FALSE(split->ClaimSuffix(1));
}

TEST(Chunk, MergeReturnsFalseForChunksFromDifferentRegions) {
  TrackingAllocatorWithMemory<kArbitraryAllocatorSize> alloc;
  std::optional<OwnedChunk> chunk_1_opt =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryChunkSize);
  ASSERT_TRUE(chunk_1_opt.has_value());
  OwnedChunk& chunk_1 = *chunk_1_opt;
  std::optional<OwnedChunk> chunk_2_opt =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryChunkSize);
  ASSERT_TRUE(chunk_2_opt.has_value());
  OwnedChunk& chunk_2 = *chunk_2_opt;
  EXPECT_FALSE(chunk_1->CanMerge(*chunk_2));
  EXPECT_FALSE(chunk_1->Merge(chunk_2));
  // Ensure that neither chunk was modified
  EXPECT_EQ(chunk_1.size(), kArbitraryChunkSize);
  EXPECT_EQ(chunk_2.size(), kArbitraryChunkSize);
}

TEST(Chunk, MergeReturnsFalseForNonAdjacentChunksFromSameRegion) {
  const size_t kTakenFromOne = 8;
  const size_t kTakenFromTwo = 4;

  TrackingAllocatorWithMemory<kArbitraryAllocatorSize> alloc;
  std::optional<OwnedChunk> chunk_1_opt =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryChunkSize);
  ASSERT_TRUE(chunk_1_opt.has_value());
  OwnedChunk& chunk_1 = *chunk_1_opt;

  std::optional<OwnedChunk> chunk_2_opt = chunk_1->TakeTail(kTakenFromOne);
  ASSERT_TRUE(chunk_2_opt.has_value());
  OwnedChunk& chunk_2 = *chunk_2_opt;

  std::optional<OwnedChunk> chunk_3_opt = chunk_2->TakeTail(kTakenFromTwo);
  ASSERT_TRUE(chunk_3_opt.has_value());
  OwnedChunk& chunk_3 = *chunk_3_opt;

  EXPECT_FALSE(chunk_1->CanMerge(*chunk_3));
  EXPECT_FALSE(chunk_1->Merge(chunk_3));
  EXPECT_EQ(chunk_1.size(), kArbitraryChunkSize - kTakenFromOne);
  EXPECT_EQ(chunk_2.size(), kTakenFromOne - kTakenFromTwo);
  EXPECT_EQ(chunk_3.size(), kTakenFromTwo);
}

TEST(Chunk, MergeJoinsMultipleAdjacentChunksFromSameRegion) {
  const size_t kTakenFromOne = 8;
  const size_t kTakenFromTwo = 4;

  TrackingAllocatorWithMemory<kArbitraryAllocatorSize> alloc;
  std::optional<OwnedChunk> chunk_1_opt =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryChunkSize);
  ASSERT_TRUE(chunk_1_opt.has_value());
  OwnedChunk& chunk_1 = *chunk_1_opt;

  std::optional<OwnedChunk> chunk_2_opt = chunk_1->TakeTail(kTakenFromOne);
  ASSERT_TRUE(chunk_2_opt.has_value());
  OwnedChunk& chunk_2 = *chunk_2_opt;

  std::optional<OwnedChunk> chunk_3_opt = chunk_2->TakeTail(kTakenFromTwo);
  ASSERT_TRUE(chunk_3_opt.has_value());
  OwnedChunk& chunk_3 = *chunk_3_opt;

  EXPECT_TRUE(chunk_1->CanMerge(*chunk_2));
  EXPECT_TRUE(chunk_1->Merge(chunk_2));
  EXPECT_TRUE(chunk_1->CanMerge(*chunk_3));
  EXPECT_TRUE(chunk_1->Merge(chunk_3));

  EXPECT_EQ(chunk_1.size(), kArbitraryChunkSize);
  EXPECT_EQ(chunk_2.size(), 0_size);
  EXPECT_EQ(chunk_3.size(), 0_size);
}

TEST(Chunk, MergeJoinsAdjacentChunksFromSameRegion) {
  const size_t kTaken = 4;

  TrackingAllocatorWithMemory<kArbitraryAllocatorSize> alloc;
  std::optional<OwnedChunk> chunk_1_opt =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryChunkSize);
  ASSERT_TRUE(chunk_1_opt.has_value());
  OwnedChunk& chunk_1 = *chunk_1_opt;
  std::optional<OwnedChunk> chunk_2_opt = chunk_1->TakeTail(kTaken);
  ASSERT_TRUE(chunk_2_opt.has_value());
  OwnedChunk& chunk_2 = *chunk_2_opt;
  EXPECT_EQ(chunk_1.size(), kArbitraryChunkSize - kTaken);
  EXPECT_EQ(chunk_2.size(), kTaken);

  EXPECT_TRUE(chunk_1->CanMerge(*chunk_2));
  EXPECT_TRUE(chunk_1->Merge(chunk_2));
  EXPECT_EQ(chunk_1.size(), kArbitraryChunkSize);
  EXPECT_EQ(chunk_2.size(), 0_size);
}

}  // namespace
}  // namespace pw::multibuf
