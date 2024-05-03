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

#include "pw_allocator/last_fit_block_allocator.h"

#include "pw_allocator/block_allocator_testing.h"
#include "pw_unit_test/framework.h"

namespace {

// Test fixtures.

using ::pw::allocator::Layout;
using ::pw::allocator::test::BlockAllocatorTest;
using ::pw::allocator::test::Preallocation;
using LastFitBlockAllocatorType =
    ::pw::allocator::LastFitBlockAllocator<BlockAllocatorTest::OffsetType>;

class LastFitBlockAllocatorTest : public BlockAllocatorTest {
 public:
  LastFitBlockAllocatorTest() : BlockAllocatorTest(allocator_) {}

 private:
  LastFitBlockAllocatorType allocator_;
};

// Unit tests.

TEST_F(LastFitBlockAllocatorTest, CanAutomaticallyInit) {
  LastFitBlockAllocatorType allocator(GetBytes());
  CanAutomaticallyInit(allocator);
}

TEST_F(LastFitBlockAllocatorTest, CanExplicitlyInit) {
  LastFitBlockAllocatorType allocator;
  CanExplicitlyInit(allocator);
}

TEST_F(LastFitBlockAllocatorTest, GetCapacity) { GetCapacity(); }

TEST_F(LastFitBlockAllocatorTest, AllocateLarge) { AllocateLarge(); }

TEST_F(LastFitBlockAllocatorTest, AllocateSmall) { AllocateSmall(); }

TEST_F(LastFitBlockAllocatorTest, AllocateLargeAlignment) {
  AllocateLargeAlignment();
}

TEST_F(LastFitBlockAllocatorTest, AllocateAlignmentFailure) {
  AllocateAlignmentFailure();
}

TEST_F(LastFitBlockAllocatorTest, AllocatesLastCompatible) {
  auto& allocator = GetAllocator({
      {kLargeOuterSize, Preallocation::kIndexFree},
      {kSmallerOuterSize, 1},
      {kSmallOuterSize, Preallocation::kIndexFree},
      {kSmallerOuterSize, 3},
      {kSmallOuterSize, Preallocation::kIndexFree},
      {Preallocation::kSizeRemaining, 5},
  });

  Store(0, allocator.Allocate(Layout(kLargeInnerSize, 1)));
  EXPECT_EQ(NextAfter(0), Fetch(1));
  Store(4, allocator.Allocate(Layout(kSmallInnerSize, 1)));
  EXPECT_EQ(NextAfter(3), Fetch(4));
  EXPECT_EQ(NextAfter(4), Fetch(5));
}

TEST_F(LastFitBlockAllocatorTest, DeallocateNull) { DeallocateNull(); }

TEST_F(LastFitBlockAllocatorTest, DeallocateShuffled) { DeallocateShuffled(); }

TEST_F(LastFitBlockAllocatorTest, IterateOverBlocks) { IterateOverBlocks(); }

TEST_F(LastFitBlockAllocatorTest, ResizeNull) { ResizeNull(); }

TEST_F(LastFitBlockAllocatorTest, ResizeLargeSame) { ResizeLargeSame(); }

TEST_F(LastFitBlockAllocatorTest, ResizeLargeSmaller) { ResizeLargeSmaller(); }

TEST_F(LastFitBlockAllocatorTest, ResizeLargeLarger) { ResizeLargeLarger(); }

TEST_F(LastFitBlockAllocatorTest, ResizeLargeLargerFailure) {
  ResizeLargeLargerFailure();
}

TEST_F(LastFitBlockAllocatorTest, ResizeSmallSame) { ResizeSmallSame(); }

TEST_F(LastFitBlockAllocatorTest, ResizeSmallSmaller) { ResizeSmallSmaller(); }

TEST_F(LastFitBlockAllocatorTest, ResizeSmallLarger) { ResizeSmallLarger(); }

TEST_F(LastFitBlockAllocatorTest, ResizeSmallLargerFailure) {
  ResizeSmallLargerFailure();
}

TEST_F(LastFitBlockAllocatorTest, CanGetLayoutFromValidPointer) {
  CanGetLayoutFromValidPointer();
}

TEST_F(LastFitBlockAllocatorTest, CannotGetLayoutFromInvalidPointer) {
  CannotGetLayoutFromInvalidPointer();
}

}  // namespace
