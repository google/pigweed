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

#include "pw_allocator/bucket_allocator.h"

#include "pw_allocator/allocator.h"
#include "pw_allocator/block_allocator_testing.h"
#include "pw_unit_test/framework.h"

namespace {

// Test fixtures.

constexpr size_t kMinChunkSize = 64;
constexpr size_t kNumBuckets = 4;

using ::pw::allocator::Layout;
using ::pw::allocator::test::Preallocation;
using BucketAllocator =
    ::pw::allocator::BucketAllocator<kMinChunkSize, kNumBuckets>;
using BlockAllocatorTest =
    ::pw::allocator::test::BlockAllocatorTest<BucketAllocator>;

class BucketAllocatorTest : public BlockAllocatorTest {
 public:
  BucketAllocatorTest() : BlockAllocatorTest(allocator_) {}

 private:
  BucketAllocator allocator_;
};

// Unit tests.

TEST_F(BucketAllocatorTest, CanAutomaticallyInit) {
  BucketAllocator allocator(GetBytes());
  CanAutomaticallyInit(allocator);
}

TEST_F(BucketAllocatorTest, CanExplicitlyInit) {
  BucketAllocator allocator;
  CanExplicitlyInit(allocator);
}

TEST_F(BucketAllocatorTest, GetCapacity) { GetCapacity(); }

TEST_F(BucketAllocatorTest, AllocateLarge) { AllocateLarge(); }

TEST_F(BucketAllocatorTest, AllocateSmall) { AllocateSmall(); }

TEST_F(BucketAllocatorTest, AllocateLargeAlignment) {
  AllocateLargeAlignment();
}

TEST_F(BucketAllocatorTest, AllocateAlignmentFailure) {
  AllocateAlignmentFailure();
}

TEST_F(BucketAllocatorTest, AllocatesFromCompatibleBucket) {
  // Bucket sizes are: [ 64, 128, 256 ]
  // Start with everything allocated in order to recycle blocks into buckets.
  auto& allocator = GetAllocator({
      {63 + BlockType::kBlockOverhead, Preallocation::kUsed},
      {kSmallerOuterSize, Preallocation::kUsed},
      {128 + BlockType::kBlockOverhead, Preallocation::kUsed},
      {kSmallerOuterSize, Preallocation::kUsed},
      {255 + BlockType::kBlockOverhead, Preallocation::kUsed},
      {kSmallerOuterSize, Preallocation::kUsed},
      {257 + BlockType::kBlockOverhead, Preallocation::kUsed},
      {Preallocation::kSizeRemaining, Preallocation::kUsed},
  });

  // Deallocate to fill buckets.
  void* bucket0_ptr = Fetch(0);
  Store(0, nullptr);
  allocator.Deallocate(bucket0_ptr);

  void* bucket1_ptr = Fetch(2);
  Store(2, nullptr);
  allocator.Deallocate(bucket1_ptr);

  void* bucket2_ptr = Fetch(4);
  Store(4, nullptr);
  allocator.Deallocate(bucket2_ptr);

  // Bucket 3 is the implicit, unbounded bucket.
  void* bucket3_ptr = Fetch(6);
  Store(6, nullptr);
  allocator.Deallocate(bucket3_ptr);

  // Allocate in a different order. The correct bucket should be picked for each
  // allocation

  // The allocation from bucket 2 splits a trailing block off the chunk.
  Store(4, allocator.Allocate(Layout(129, 1)));
  auto* block2 = BlockType::FromUsableSpace(bucket2_ptr);
  EXPECT_TRUE(block2->IsFree());
  EXPECT_EQ(Fetch(4), block2->Next()->UsableSpace());

  // This allocation exactly matches the chunk size of bucket 1.
  Store(2, allocator.Allocate(Layout(128, 1)));
  EXPECT_EQ(Fetch(2), bucket1_ptr);

  // 129 should start with bucket 2, then use bucket 3 since 2 is empty.
  // The allocation from bucket 3 splits a trailing block off the chunk.
  auto* block3 = BlockType::FromUsableSpace(bucket3_ptr);
  Store(6, allocator.Allocate(Layout(129, 1)));
  EXPECT_TRUE(block3->IsFree());
  EXPECT_EQ(Fetch(6), block3->Next()->UsableSpace());

  // The allocation from bucket 0 splits a trailing block off the chunk.
  auto* block0 = BlockType::FromUsableSpace(bucket0_ptr);
  Store(0, allocator.Allocate(Layout(32, 1)));
  EXPECT_TRUE(block0->IsFree());
  EXPECT_EQ(Fetch(0), block0->Next()->UsableSpace());
}

TEST_F(BucketAllocatorTest, UnusedPortionIsRecycled) {
  auto& allocator = GetAllocator({
      {128 + BlockType::kBlockOverhead, Preallocation::kUsed},
      {Preallocation::kSizeRemaining, Preallocation::kUsed},
  });

  // Deallocate to fill buckets.
  allocator.Deallocate(Fetch(0));
  Store(0, nullptr);

  Store(2, allocator.Allocate(Layout(65, 1)));
  ASSERT_NE(Fetch(2), nullptr);

  // The remainder should be recycled to a smaller bucket.
  Store(3, allocator.Allocate(Layout(32, 1)));
  ASSERT_NE(Fetch(3), nullptr);
}

TEST_F(BucketAllocatorTest, ExhaustBucket) {
  auto& allocator = GetAllocator({
      {128 + BlockType::kBlockOverhead, Preallocation::kUsed},
      {kSmallerOuterSize, Preallocation::kUsed},
      {128 + BlockType::kBlockOverhead, Preallocation::kUsed},
      {kSmallerOuterSize, Preallocation::kUsed},
      {128 + BlockType::kBlockOverhead, Preallocation::kUsed},
      {Preallocation::kSizeRemaining, Preallocation::kUsed},
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

TEST_F(BucketAllocatorTest, DeallocateNull) { DeallocateNull(); }

TEST_F(BucketAllocatorTest, DeallocateShuffled) { DeallocateShuffled(); }

TEST_F(BucketAllocatorTest, IterateOverBlocks) { IterateOverBlocks(); }

TEST_F(BucketAllocatorTest, ResizeNull) { ResizeNull(); }

TEST_F(BucketAllocatorTest, ResizeLargeSame) { ResizeLargeSame(); }

TEST_F(BucketAllocatorTest, ResizeLargeSmaller) { ResizeLargeSmaller(); }

TEST_F(BucketAllocatorTest, ResizeLargeLarger) { ResizeLargeLarger(); }

TEST_F(BucketAllocatorTest, ResizeLargeLargerFailure) {
  ResizeLargeLargerFailure();
}

TEST_F(BucketAllocatorTest, ResizeSmallSame) { ResizeSmallSame(); }

TEST_F(BucketAllocatorTest, ResizeSmallSmaller) { ResizeSmallSmaller(); }

TEST_F(BucketAllocatorTest, ResizeSmallLarger) { ResizeSmallLarger(); }

TEST_F(BucketAllocatorTest, ResizeSmallLargerFailure) {
  ResizeSmallLargerFailure();
}

TEST_F(BucketAllocatorTest, CanMeasureFragmentation) {
  CanMeasureFragmentation();
}

TEST_F(BucketAllocatorTest, FirstSmallSplitIsRecycled) {
  auto& allocator = GetAllocator({
      {128 + (BlockType::kBlockOverhead * 2), Preallocation::kUsed},
      {Preallocation::kSizeRemaining, Preallocation::kUsed},
  });

  // Deallocate to fill buckets.
  allocator.Deallocate(Fetch(0));
  Store(0, nullptr);

  Store(2, allocator.Allocate(Layout(129, 1)));
  ASSERT_NE(Fetch(2), nullptr);

  auto& bucket_block_allocator = static_cast<BucketAllocator&>(allocator);
  for (auto* block : bucket_block_allocator.blocks()) {
    ASSERT_TRUE(block->IsValid());
  }
}

TEST_F(BucketAllocatorTest, LaterSmallSplitNotIsRecycled) {
  auto& allocator = GetAllocator({
      {128 + BlockType::kBlockOverhead, Preallocation::kUsed},
      {128 + (BlockType::kBlockOverhead * 2), Preallocation::kUsed},
      {Preallocation::kSizeRemaining, Preallocation::kUsed},
  });

  // Deallocate to fill buckets.
  allocator.Deallocate(Fetch(1));
  Store(1, nullptr);

  auto& bucket_block_allocator = static_cast<BucketAllocator&>(allocator);
  size_t old_size = (*bucket_block_allocator.blocks().begin())->OuterSize();

  // The leftover space is not larger than `kBlockOverhead`, and so should be
  // merged with the previous block.
  Store(3, allocator.Allocate(Layout(128, 1)));
  ASSERT_NE(Fetch(3), nullptr);
  size_t new_size = (*bucket_block_allocator.blocks().begin())->OuterSize();
  EXPECT_NE(old_size, new_size);

  for (auto* block : bucket_block_allocator.blocks()) {
    ASSERT_TRUE(block->IsValid());
  }
}

}  // namespace
