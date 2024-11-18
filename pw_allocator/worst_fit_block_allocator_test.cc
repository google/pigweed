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

#include "pw_allocator/worst_fit_block_allocator.h"

#include <cstdint>

#include "pw_allocator/block_allocator_testing.h"
#include "pw_unit_test/framework.h"

namespace {

// Test fixtures.

using ::pw::allocator::Layout;
using ::pw::allocator::test::Preallocation;
using WorstFitBlockAllocator =
    ::pw::allocator::WorstFitBlockAllocator<uint16_t>;
using BlockAllocatorTest =
    ::pw::allocator::test::BlockAllocatorTest<WorstFitBlockAllocator>;

class WorstFitBlockAllocatorTest : public BlockAllocatorTest {
 public:
  WorstFitBlockAllocatorTest() : BlockAllocatorTest(allocator_) {}

 private:
  WorstFitBlockAllocator allocator_;
};

// Unit tests.

TEST_F(WorstFitBlockAllocatorTest, CanAutomaticallyInit) {
  WorstFitBlockAllocator allocator(GetBytes());
  CanAutomaticallyInit(allocator);
}

TEST_F(WorstFitBlockAllocatorTest, CanExplicitlyInit) {
  WorstFitBlockAllocator allocator;
  CanExplicitlyInit(allocator);
}

TEST_F(WorstFitBlockAllocatorTest, GetCapacity) { GetCapacity(); }

TEST_F(WorstFitBlockAllocatorTest, AllocateLarge) { AllocateLarge(); }

TEST_F(WorstFitBlockAllocatorTest, AllocateSmall) { AllocateSmall(); }

TEST_F(WorstFitBlockAllocatorTest, AllocateLargeAlignment) {
  AllocateLargeAlignment();
}

TEST_F(WorstFitBlockAllocatorTest, AllocateAlignmentFailure) {
  AllocateAlignmentFailure();
}

TEST_F(WorstFitBlockAllocatorTest, AllocatesWorstCompatible) {
  auto& allocator = GetAllocator({
      {kLargeOuterSize, Preallocation::kFree},    // 0
      {kSmallerOuterSize, Preallocation::kUsed},  // 1
      {kSmallOuterSize, Preallocation::kFree},    // 2
      {kSmallerOuterSize, Preallocation::kUsed},  // 3
      {kLargeOuterSize, Preallocation::kFree},    // 4
      {Preallocation::kSizeRemaining, Preallocation::kUsed},
  });

  void* ptr1 = allocator.Allocate(Layout(kSmallInnerSize, 1));
  EXPECT_LT(ptr1, Fetch(1));

  void* ptr2 = allocator.Allocate(Layout(kSmallInnerSize, 1));
  EXPECT_LT(Fetch(3), ptr2);
  EXPECT_LT(ptr2, Fetch(5));

  // A second small block fits in the leftovers of the first "Large" block.
  void* ptr3 = allocator.Allocate(Layout(kSmallInnerSize, 1));
  EXPECT_LT(ptr3, Fetch(1));

  allocator.Deallocate(ptr1);
  allocator.Deallocate(ptr2);
  allocator.Deallocate(ptr3);
}

TEST_F(WorstFitBlockAllocatorTest, DeallocateNull) { DeallocateNull(); }

TEST_F(WorstFitBlockAllocatorTest, DeallocateShuffled) { DeallocateShuffled(); }

TEST_F(WorstFitBlockAllocatorTest, IterateOverBlocks) { IterateOverBlocks(); }

TEST_F(WorstFitBlockAllocatorTest, ResizeNull) { ResizeNull(); }

TEST_F(WorstFitBlockAllocatorTest, ResizeLargeSame) { ResizeLargeSame(); }

TEST_F(WorstFitBlockAllocatorTest, ResizeLargeSmaller) { ResizeLargeSmaller(); }

TEST_F(WorstFitBlockAllocatorTest, ResizeLargeLarger) { ResizeLargeLarger(); }

TEST_F(WorstFitBlockAllocatorTest, ResizeLargeLargerFailure) {
  ResizeLargeLargerFailure();
}

TEST_F(WorstFitBlockAllocatorTest, ResizeSmallSame) { ResizeSmallSame(); }

TEST_F(WorstFitBlockAllocatorTest, ResizeSmallSmaller) { ResizeSmallSmaller(); }

TEST_F(WorstFitBlockAllocatorTest, ResizeSmallLarger) { ResizeSmallLarger(); }

TEST_F(WorstFitBlockAllocatorTest, ResizeSmallLargerFailure) {
  ResizeSmallLargerFailure();
}

TEST_F(WorstFitBlockAllocatorTest, CanMeasureFragmentation) {
  CanMeasureFragmentation();
}

TEST_F(WorstFitBlockAllocatorTest, PoisonPeriodically) { PoisonPeriodically(); }

}  // namespace
