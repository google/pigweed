// Copyright 2024 The Pigweed Authors
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

#include "pw_allocator/bucket_block_allocator.h"

#include "pw_allocator/allocator.h"
#include "pw_allocator/block_allocator_testing.h"
#include "pw_unit_test/framework.h"

namespace {

// Test fixtures.

constexpr size_t kMinChunkSize = 64;
constexpr size_t kNumBuckets = 4;

using ::pw::allocator::Layout;
using ::pw::allocator::test::Preallocation;
using BucketBlockAllocator =
    ::pw::allocator::BucketBlockAllocator<uint16_t, kMinChunkSize, kNumBuckets>;
using BlockAllocatorTest =
    ::pw::allocator::test::BlockAllocatorTest<BucketBlockAllocator>;

class BucketBlockAllocatorTest : public BlockAllocatorTest {
 public:
  BucketBlockAllocatorTest() : BlockAllocatorTest(allocator_) {}

 private:
  BucketBlockAllocator allocator_;
};

// Unit tests.

TEST_F(BucketBlockAllocatorTest, CanAutomaticallyInit) {
  BucketBlockAllocator allocator(GetBytes());
  CanAutomaticallyInit(allocator);
}

TEST_F(BucketBlockAllocatorTest, CanExplicitlyInit) {
  BucketBlockAllocator allocator;
  CanExplicitlyInit(allocator);
}

TEST_F(BucketBlockAllocatorTest, GetCapacity) { GetCapacity(); }

TEST_F(BucketBlockAllocatorTest, AllocateLarge) { AllocateLarge(); }

TEST_F(BucketBlockAllocatorTest, AllocateSmall) { AllocateSmall(); }

TEST_F(BucketBlockAllocatorTest, AllocateLargeAlignment) {
  AllocateLargeAlignment();
}

TEST_F(BucketBlockAllocatorTest, AllocateAlignmentFailure) {
  AllocateAlignmentFailure();
}

TEST_F(BucketBlockAllocatorTest, AllocatesFromCompatibleBucket) {
  // Bucket sizes are: [ 64, 128, 256 ]
  // Start with everything allocated in order to recycle blocks into buckets.
  auto& allocator = GetAllocator({
      {kSmallerOuterSize, 0},
      {63 + BlockType::kBlockOverhead, 1},
      {kSmallerOuterSize, 2},
      {128 + BlockType::kBlockOverhead, 3},
      {kSmallerOuterSize, 4},
      {255 + BlockType::kBlockOverhead, 5},
      {kSmallerOuterSize, 6},
      {257 + BlockType::kBlockOverhead, 7},
      {Preallocation::kSizeRemaining, 8},
  });

  // Deallocate to fill buckets.
  void* bucket0_ptr = Fetch(1);
  Store(1, nullptr);
  allocator.Deallocate(bucket0_ptr);

  void* bucket1_ptr = Fetch(3);
  Store(3, nullptr);
  allocator.Deallocate(bucket1_ptr);

  void* bucket2_ptr = Fetch(5);
  Store(5, nullptr);
  allocator.Deallocate(bucket2_ptr);

  // Bucket 3 is the implicit, unbounded bucket.
  void* bucket3_ptr = Fetch(7);
  Store(7, nullptr);
  allocator.Deallocate(bucket3_ptr);

  // Allocate in a different order. The correct bucket should be picked for each
  // allocation

  // The allocation from bucket 2 splits a trailing block off the chunk.
  Store(5, allocator.Allocate(Layout(129, 1)));
  auto* block2 = BlockType::FromUsableSpace(bucket2_ptr);
  EXPECT_FALSE(block2->Used());
  EXPECT_EQ(Fetch(5), block2->Next()->UsableSpace());

  // This allocation exactly matches the chunk size of bucket 1.
  Store(3, allocator.Allocate(Layout(128, 1)));
  EXPECT_EQ(Fetch(3), bucket1_ptr);

  // 129 should start with bucket 2, then use bucket 3 since 2 is empty.
  // The allocation from bucket 3 splits a trailing block off the chunk.
  auto* block3 = BlockType::FromUsableSpace(bucket3_ptr);
  Store(7, allocator.Allocate(Layout(129, 1)));
  EXPECT_FALSE(block3->Used());
  EXPECT_EQ(Fetch(7), block3->Next()->UsableSpace());

  // The allocation from bucket 0 splits a trailing block off the chunk.
  auto* block0 = BlockType::FromUsableSpace(bucket0_ptr);
  Store(1, allocator.Allocate(Layout(32, 1)));
  EXPECT_FALSE(block0->Used());
  EXPECT_EQ(Fetch(1), block0->Next()->UsableSpace());
}

TEST_F(BucketBlockAllocatorTest, UnusedPortionIsRecycled) {
  auto& allocator = GetAllocator({
      {128 + BlockType::kBlockOverhead, 0},
      {Preallocation::kSizeRemaining, 3},
  });

  // Deallocate to fill buckets.
  allocator.Deallocate(Fetch(0));
  Store(0, nullptr);

  Store(1, allocator.Allocate(Layout(65, 1)));
  ASSERT_NE(Fetch(1), nullptr);

  // The remainder should be recycled to a smaller bucket.
  Store(2, allocator.Allocate(Layout(32, 1)));
  ASSERT_NE(Fetch(2), nullptr);
}

TEST_F(BucketBlockAllocatorTest, ExhaustBucket) {
  auto& allocator = GetAllocator({
      {128 + BlockType::kBlockOverhead, 0},
      {kSmallerOuterSize, 1},
      {128 + BlockType::kBlockOverhead, 2},
      {kSmallerOuterSize, 3},
      {128 + BlockType::kBlockOverhead, 4},
      {Preallocation::kSizeRemaining, 5},
  });

  // Deallocate to fill buckets.
  allocator.Deallocate(Fetch(0));
  Store(0, nullptr);
  allocator.Deallocate(Fetch(2));
  Store(2, nullptr);
  allocator.Deallocate(Fetch(4));
  Store(4, nullptr);

  void* ptr0 = allocator.Allocate(Layout(65, 1));
  EXPECT_NE(ptr0, nullptr);
  Store(0, ptr0);

  void* ptr2 = allocator.Allocate(Layout(65, 1));
  EXPECT_NE(ptr2, nullptr);
  Store(2, ptr2);

  void* ptr4 = allocator.Allocate(Layout(65, 1));
  EXPECT_NE(ptr4, nullptr);
  Store(4, ptr4);

  EXPECT_EQ(allocator.Allocate(Layout(65, 1)), nullptr);
}

TEST_F(BucketBlockAllocatorTest, DeallocateNull) { DeallocateNull(); }

TEST_F(BucketBlockAllocatorTest, DeallocateShuffled) { DeallocateShuffled(); }

TEST_F(BucketBlockAllocatorTest, IterateOverBlocks) { IterateOverBlocks(); }

TEST_F(BucketBlockAllocatorTest, ResizeNull) { ResizeNull(); }

TEST_F(BucketBlockAllocatorTest, ResizeLargeSame) { ResizeLargeSame(); }

TEST_F(BucketBlockAllocatorTest, ResizeLargeSmaller) { ResizeLargeSmaller(); }

TEST_F(BucketBlockAllocatorTest, ResizeLargeLarger) { ResizeLargeLarger(); }

TEST_F(BucketBlockAllocatorTest, ResizeLargeLargerFailure) {
  ResizeLargeLargerFailure();
}

TEST_F(BucketBlockAllocatorTest, ResizeSmallSame) { ResizeSmallSame(); }

TEST_F(BucketBlockAllocatorTest, ResizeSmallSmaller) { ResizeSmallSmaller(); }

TEST_F(BucketBlockAllocatorTest, ResizeSmallLarger) { ResizeSmallLarger(); }

TEST_F(BucketBlockAllocatorTest, ResizeSmallLargerFailure) {
  ResizeSmallLargerFailure();
}

TEST_F(BucketBlockAllocatorTest, CanMeasureFragmentation) {
  CanMeasureFragmentation();
}

}  // namespace
