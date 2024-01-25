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

#include "pw_allocator/split_free_list_allocator.h"

#include "pw_allocator/allocator_fuzzing.h"
#include "pw_allocator/block.h"
#include "pw_allocator/buffer.h"
#include "pw_bytes/alignment.h"
#include "pw_bytes/span.h"
#include "pw_containers/vector.h"
#include "pw_fuzzer/fuzztest.h"
#include "pw_unit_test/framework.h"

namespace pw::allocator {
namespace {

// Test fixtures.

// An `SplitFreeListAllocator` that is automatically initialized on
// construction.
template <typename BlockType, size_t kCapacity, size_t kThreshold>
class SplitFreeListAllocatorWithBuffer
    : public WithBuffer<SplitFreeListAllocator<BlockType>,
                        kCapacity,
                        BlockType> {
 public:
  SplitFreeListAllocatorWithBuffer() {
    EXPECT_EQ((*this)->Init(ByteSpan(this->data(), this->size()), kThreshold),
              OkStatus());
  }
};

// Test case fixture that allows individual tests to cache allocations and
// release them automatically on tear-down.
class SplitFreeListAllocatorTest : public ::testing::Test {
 protected:
  using BlockType = Block<uint16_t>;

  // Size of the memory region to use in the tests below.
  static constexpr size_t kCapacity = 512;

  // Minimum size of a "large" allocation; allocation less than this size are
  // considered "small".
  static constexpr size_t kThreshold = 64;

  static constexpr size_t kMaxSize = kCapacity - BlockType::kBlockOverhead;
  static constexpr size_t kNumPtrs = 16;

  void SetUp() override {
    for (size_t i = 0; i < kNumPtrs; ++i) {
      ptrs_[i] = nullptr;
    }
  }

  // This method simply ensures the memory is usable by writing to it.
  void UseMemory(void* ptr, size_t size) { memset(ptr, 0x5a, size); }

  void TearDown() override {
    for (size_t i = 0; i < kNumPtrs; ++i) {
      if (ptrs_[i] != nullptr) {
        // `SplitFreeListAllocator::Deallocate` doesn't actually use the layout,
        // as the information it needs is encoded in the blocks.
        allocator_->Deallocate(ptrs_[i], Layout::Of<void*>());
      }
    }
  }

  SplitFreeListAllocatorWithBuffer<BlockType, kCapacity, kThreshold> allocator_;

  // Tests can store allocations in this array to have them automatically
  // freed in `TearDown`, including on ASSERT failure. If pointers are manually
  // deallocated, they should be set to null in the array.
  void* ptrs_[kNumPtrs];
};

// Unit tests.

TEST_F(SplitFreeListAllocatorTest, AutomaticallyInit) {
  alignas(BlockType) std::array<std::byte, kCapacity> buffer;
  ByteSpan bytes(buffer.data(), buffer.size());

  SplitFreeListAllocator<Block<>> allocator(bytes, kThreshold);

  EXPECT_NE(*(allocator.blocks().begin()), nullptr);
}

TEST_F(SplitFreeListAllocatorTest, ExplicitlyInit) {
  // The test fixture uses aligned memory to make it easier to reason about
  // allocations, but that isn't strictly required.
  alignas(BlockType) std::array<std::byte, kCapacity> buffer;
  ByteSpan bytes(buffer.data(), buffer.size());

  SplitFreeListAllocator<Block<>> allocator;

  EXPECT_EQ(*(allocator.blocks().begin()), nullptr);
  EXPECT_EQ(allocator.Init(bytes.subspan(1)), OkStatus());
  EXPECT_NE(*(allocator.blocks().begin()), nullptr);
}

TEST_F(SplitFreeListAllocatorTest, AllocateLarge) {
  constexpr Layout layout = Layout::Of<std::byte[kThreshold]>();
  ptrs_[0] = allocator_->Allocate(layout);
  ASSERT_NE(ptrs_[0], nullptr);
  EXPECT_GE(ptrs_[0], allocator_.data());
  EXPECT_LT(ptrs_[0], allocator_.data() + allocator_.size());
  UseMemory(ptrs_[0], layout.size());
}

TEST_F(SplitFreeListAllocatorTest, AllocateSmall) {
  // Returned pointer should not be from the beginning, but should still be in
  // range. Exact pointer depends on allocator's minimum allocation size.
  constexpr Layout layout = Layout::Of<uint8_t>();
  ptrs_[0] = allocator_->Allocate(layout);
  ASSERT_NE(ptrs_[0], nullptr);
  EXPECT_GT(ptrs_[0], allocator_.data());
  EXPECT_LT(ptrs_[0], allocator_.data() + allocator_.size());
  UseMemory(ptrs_[0], layout.size());
}

TEST_F(SplitFreeListAllocatorTest, AllocateTooLarge) {
  ptrs_[0] = allocator_->Allocate(Layout::Of<std::byte[kCapacity * 2]>());
  EXPECT_EQ(ptrs_[0], nullptr);
}

TEST_F(SplitFreeListAllocatorTest, AllocateLargeAlignment) {
  constexpr size_t kSize = sizeof(uint32_t);
  constexpr size_t kAlignment = 64;
  ptrs_[0] = allocator_->Allocate(Layout(kSize, kAlignment));
  ASSERT_NE(ptrs_[0], nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(ptrs_[0]) % kAlignment, 0U);
  UseMemory(ptrs_[0], kSize);

  ptrs_[1] = allocator_->Allocate(Layout(kSize, kAlignment));
  ASSERT_NE(ptrs_[1], nullptr);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(ptrs_[1]) % kAlignment, 0U);
  UseMemory(ptrs_[1], kSize);
}

TEST_F(SplitFreeListAllocatorTest, AllocateFromUnaligned) {
  SplitFreeListAllocator<Block<>> unaligned;
  ByteSpan bytes(allocator_.data(), allocator_.size());
  ASSERT_EQ(unaligned.Init(bytes.subspan(1), kThreshold), OkStatus());

  constexpr Layout layout = Layout::Of<std::byte[kThreshold + 8]>();
  void* ptr = unaligned.Allocate(layout);
  ASSERT_NE(ptr, nullptr);
  UseMemory(ptr, layout.size());
  unaligned.Deallocate(ptr, layout);
}

TEST_F(SplitFreeListAllocatorTest, AllocateAlignmentFailure) {
  constexpr size_t kAlignment = 128;

  // Allocate a block, an unaligned block, and any remaining space.
  // This approach needs to work with and without heap poisoning, and so employs
  // a "guess and check" strategy rather than trying to calculate exact values.
  Layout layout0(kThreshold, 1);
  Layout layout1(kThreshold * 2, 1);
  while (true) {
    // Allocate space from the front.
    ptrs_[0] = allocator_->Allocate(layout0);
    ASSERT_NE(ptrs_[0], nullptr);

    // Allocate a possibly unaligned block.
    ptrs_[1] = allocator_->Allocate(layout1);
    ASSERT_NE(ptrs_[1], nullptr);
    auto addr = reinterpret_cast<uintptr_t>(ptrs_[1]);
    if (addr % kAlignment != 0) {
      break;
    }

    // If the second block was aligned, release both blocks, increase the size
    // of the first and try again.
    allocator_->Deallocate(ptrs_[0], layout0);
    allocator_->Deallocate(ptrs_[1], layout1);
    layout0 = Layout(layout0.size() + 1, 1);
  }

  // Consume any remaining memory.
  Layout layout2(kCapacity, 1);
  while (layout2.size() != 0) {
    ptrs_[2] = allocator_->Allocate(layout2);
    if (ptrs_[2] != nullptr) {
      break;
    }
    layout2 = Layout(layout2.size() - 1, 1);
  }

  // Free the second region. This leaves exactly one unaligned region available.
  allocator_->Deallocate(ptrs_[1], layout1);
  ptrs_[1] = nullptr;

  // The allocator should be unable to create an aligned region..
  ptrs_[1] = allocator_->Allocate(Layout(layout1.size(), kAlignment));
  EXPECT_EQ(ptrs_[1], nullptr);
}

TEST_F(SplitFreeListAllocatorTest, DeallocateNull) {
  constexpr Layout layout = Layout::Of<uint8_t>();
  allocator_->Deallocate(nullptr, layout);
}

TEST_F(SplitFreeListAllocatorTest, DeallocateShuffled) {
  constexpr Layout layout = Layout::Of<std::byte[32]>();
  // Allocate until the pool is exhausted.
  for (size_t i = 0; i < kNumPtrs; ++i) {
    ptrs_[i] = allocator_->Allocate(layout);
    if (ptrs_[i] == nullptr) {
      break;
    }
  }
  // Mix up the order of allocations.
  for (size_t i = 0; i < kNumPtrs; ++i) {
    if (i % 2 == 0 && i + 1 < kNumPtrs) {
      std::swap(ptrs_[i], ptrs_[i + 1]);
    }
    if (i % 3 == 0 && i + 2 < kNumPtrs) {
      std::swap(ptrs_[i], ptrs_[i + 2]);
    }
  }
  // Deallocate everything.
  for (size_t i = 0; i < kNumPtrs; ++i) {
    allocator_->Deallocate(ptrs_[i], layout);
    ptrs_[i] = nullptr;
  }
}

TEST_F(SplitFreeListAllocatorTest, IterateOverBlocks) {
  // Pick sizes small enough that blocks fit, even with poisoning.
  constexpr Layout layout1 = Layout::Of<std::byte[16]>();
  constexpr Layout layout2 = Layout::Of<std::byte[8]>();

  // Allocate six blocks of alternating sizes. After this, the will also be a
  // seventh, unallocated block of the remaining memory.
  for (size_t i = 0; i < 3; ++i) {
    ptrs_[i] = allocator_->Allocate(layout1);
    ASSERT_NE(ptrs_[i], nullptr);
    ptrs_[i + 3] = allocator_->Allocate(layout2);
    ASSERT_NE(ptrs_[i + 3], nullptr);
  }

  // Deallocate every other block. After this there will be three more
  // unallocated blocks, for a total of four.
  for (size_t i = 0; i < 3; ++i) {
    allocator_->Deallocate(ptrs_[i], layout1);
  }

  // Count the blocks. The unallocated ones vary in size, but the allocated ones
  // should all be the same.
  size_t free_count = 0;
  size_t used_count = 0;
  for (auto* block : allocator_->blocks()) {
    if (block->Used()) {
      EXPECT_GE(block->InnerSize(), layout2.size());
      ++used_count;
    } else {
      ++free_count;
    }
  }
  EXPECT_EQ(used_count, 3U);
  EXPECT_EQ(free_count, 4U);
}

TEST_F(SplitFreeListAllocatorTest, QueryLargeValid) {
  constexpr Layout layout = Layout::Of<std::byte[kThreshold * 2]>();
  ptrs_[0] = allocator_->Allocate(layout);
  EXPECT_EQ(allocator_->Query(ptrs_[0], layout), OkStatus());
}

TEST_F(SplitFreeListAllocatorTest, QuerySmallValid) {
  constexpr Layout layout = Layout::Of<uint8_t>();
  ptrs_[0] = allocator_->Allocate(layout);
  EXPECT_EQ(allocator_->Query(ptrs_[0], layout), OkStatus());
}

TEST_F(SplitFreeListAllocatorTest, QueryInvalidPtr) {
  constexpr Layout layout = Layout::Of<SplitFreeListAllocatorTest>();
  EXPECT_EQ(allocator_->Query(this, layout), Status::OutOfRange());
}

TEST_F(SplitFreeListAllocatorTest, ResizeNull) {
  constexpr Layout old_layout = Layout::Of<uint8_t>();
  size_t new_size = 1;
  EXPECT_FALSE(allocator_->Resize(nullptr, old_layout, new_size));
}

TEST_F(SplitFreeListAllocatorTest, ResizeSame) {
  constexpr Layout old_layout = Layout::Of<uint32_t>();
  ptrs_[0] = allocator_->Allocate(old_layout);
  ASSERT_NE(ptrs_[0], nullptr);

  constexpr Layout new_layout = Layout::Of<uint32_t>();
  EXPECT_TRUE(allocator_->Resize(ptrs_[0], old_layout, new_layout.size()));
  ASSERT_NE(ptrs_[0], nullptr);
  UseMemory(ptrs_[0], new_layout.size());
}

TEST_F(SplitFreeListAllocatorTest, ResizeLargeSmaller) {
  constexpr Layout old_layout = Layout::Of<std::byte[kMaxSize]>();
  ptrs_[0] = allocator_->Allocate(old_layout);
  ASSERT_NE(ptrs_[0], nullptr);

  // Shrinking always succeeds.
  constexpr Layout new_layout = Layout::Of<std::byte[kThreshold]>();
  EXPECT_TRUE(allocator_->Resize(ptrs_[0], old_layout, new_layout.size()));
  ASSERT_NE(ptrs_[0], nullptr);
  UseMemory(ptrs_[0], new_layout.size());
}

TEST_F(SplitFreeListAllocatorTest, ResizeLargeLarger) {
  constexpr Layout old_layout = Layout::Of<std::byte[kThreshold]>();
  ptrs_[0] = allocator_->Allocate(old_layout);
  ASSERT_NE(ptrs_[0], nullptr);

  // Nothing after ptr, so `Resize` should succeed.
  constexpr Layout new_layout = Layout::Of<std::byte[kMaxSize]>();
  EXPECT_TRUE(allocator_->Resize(ptrs_[0], old_layout, new_layout.size()));
  ASSERT_NE(ptrs_[0], nullptr);
  UseMemory(ptrs_[0], new_layout.size());
}

TEST_F(SplitFreeListAllocatorTest, ResizeLargeLargerFailure) {
  constexpr Layout old_layout = Layout::Of<std::byte[kThreshold]>();
  ptrs_[0] = allocator_->Allocate(old_layout);
  ASSERT_NE(ptrs_[0], nullptr);

  ptrs_[1] = allocator_->Allocate(old_layout);
  ASSERT_NE(ptrs_[1], nullptr);

  // Memory after ptr is already allocated, so `Resize` should fail.
  EXPECT_FALSE(allocator_->Resize(ptrs_[0], old_layout, kMaxSize));
}

TEST_F(SplitFreeListAllocatorTest, ResizeLargeSmallerAcrossThreshold) {
  constexpr Layout old_layout = Layout::Of<std::byte[kThreshold]>();
  ptrs_[0] = allocator_->Allocate(old_layout);
  ASSERT_NE(ptrs_[0], nullptr);

  // Shrinking succeeds, and the pointer is unchanged even though it is now
  // below the threshold.
  constexpr Layout new_layout = Layout::Of<std::byte[kThreshold / 4]>();
  EXPECT_TRUE(allocator_->Resize(ptrs_[0], old_layout, new_layout.size()));
  ASSERT_NE(ptrs_[0], nullptr);
  UseMemory(ptrs_[0], new_layout.size());
}

TEST_F(SplitFreeListAllocatorTest, ResizeSmallSmaller) {
  constexpr Layout old_layout = Layout::Of<uint32_t>();
  ptrs_[0] = allocator_->Allocate(old_layout);
  ASSERT_NE(ptrs_[0], nullptr);

  // Shrinking always succeeds.
  constexpr Layout new_layout = Layout::Of<uint8_t>();
  EXPECT_TRUE(allocator_->Resize(ptrs_[0], old_layout, new_layout.size()));
}

TEST_F(SplitFreeListAllocatorTest, ResizeSmallLarger) {
  // First, allocate a trailing block.
  constexpr Layout layout1 = Layout::Of<std::byte[kThreshold / 4]>();
  ptrs_[0] = allocator_->Allocate(layout1);
  ASSERT_NE(ptrs_[0], nullptr);

  // Next allocate the memory to be resized.
  constexpr Layout old_layout = Layout::Of<std::byte[kThreshold / 4]>();
  ptrs_[1] = allocator_->Allocate(old_layout);
  ASSERT_NE(ptrs_[1], nullptr);

  // Now free the trailing block.
  allocator_->Deallocate(ptrs_[0], layout1);
  ptrs_[0] = nullptr;

  // And finally, resize. Since the memory after the block is available and big
  // enough, `Resize` should succeed.
  constexpr Layout new_layout = Layout::Of<std::byte[kThreshold / 2]>();
  EXPECT_TRUE(allocator_->Resize(ptrs_[1], old_layout, new_layout.size()));
  ASSERT_NE(ptrs_[1], nullptr);
  UseMemory(ptrs_[1], new_layout.size());
}

TEST_F(SplitFreeListAllocatorTest, ResizeSmallLargerFailure) {
  // First, allocate a trailing block.
  constexpr Layout layout1 = Layout::Of<std::byte[kThreshold / 4]>();
  ptrs_[0] = allocator_->Allocate(layout1);
  ASSERT_NE(ptrs_[0], nullptr);

  // Next allocate the memory to be resized.
  constexpr Layout old_layout = Layout::Of<std::byte[kThreshold / 4]>();
  ptrs_[1] = allocator_->Allocate(old_layout);
  ASSERT_NE(ptrs_[1], nullptr);

  // Now free the trailing block.
  allocator_->Deallocate(ptrs_[0], layout1);
  ptrs_[0] = nullptr;

  // And finally, resize. Since the memory after the block is available but not
  // big enough, `Resize` should fail.
  size_t new_size = (kThreshold / 2) + BlockType::kBlockOverhead + 1;
  EXPECT_FALSE(allocator_->Resize(ptrs_[1], old_layout, new_size));
}

TEST_F(SplitFreeListAllocatorTest, ResizeSmallLargerAcrossThreshold) {
  // First, allocate several trailing block.
  constexpr Layout layout1 = Layout::Of<std::byte[kThreshold / 2]>();
  ptrs_[0] = allocator_->Allocate(layout1);
  ASSERT_NE(ptrs_[0], nullptr);

  ptrs_[1] = allocator_->Allocate(layout1);
  ASSERT_NE(ptrs_[1], nullptr);

  // Next allocate the memory to be resized.
  constexpr Layout old_layout = Layout::Of<std::byte[kThreshold / 4]>();
  ptrs_[2] = allocator_->Allocate(old_layout);
  EXPECT_NE(ptrs_[2], nullptr);

  // Now free the trailing blocks.
  allocator_->Deallocate(ptrs_[0], layout1);
  ptrs_[0] = nullptr;
  allocator_->Deallocate(ptrs_[1], layout1);
  ptrs_[1] = nullptr;

  // Growing succeeds, and the pointer is unchanged even though it is now
  // above the threshold.
  constexpr Layout new_layout = Layout::Of<std::byte[kThreshold]>();
  EXPECT_TRUE(allocator_->Resize(ptrs_[2], old_layout, new_layout.size()));
  ASSERT_NE(ptrs_[2], nullptr);
  UseMemory(ptrs_[2], new_layout.size());
}

// Fuzz tests.

constexpr size_t kFuzzMaxRequests = 256;
constexpr size_t kFuzzMaxAllocations = 128;

template <typename BlockType, size_t kCapacity>
class SplitFreeListAllocatorFuzzTest
    : public test::AllocatorTestHarness<kFuzzMaxAllocations> {
 private:
  Allocator* Init() override { return &(*allocator_); }
  SplitFreeListAllocatorWithBuffer<BlockType, kCapacity, kCapacity / 8>
      allocator_;
};

constexpr size_t kFuzzU16MaxSize = 2048;
void NeverCrashesU16(const Vector<test::AllocatorRequest>& requests) {
  SplitFreeListAllocatorFuzzTest<Block<uint16_t>, kFuzzU16MaxSize> fuzzer;
  fuzzer.HandleRequests(requests);
}
FUZZ_TEST(SplitFreeListAllocatorFuzzTest, NeverCrashesU16)
    .WithDomains(
        test::ArbitraryAllocatorRequests<kFuzzMaxRequests, kFuzzU16MaxSize>());

constexpr size_t kFuzzU32MaxSize = 2048;
void NeverCrashesU32(const Vector<test::AllocatorRequest>& requests) {
  SplitFreeListAllocatorFuzzTest<Block<uint32_t>, kFuzzU32MaxSize> fuzzer;
  fuzzer.HandleRequests(requests);
}
FUZZ_TEST(SplitFreeListAllocatorFuzzTest, NeverCrashesU32)
    .WithDomains(
        test::ArbitraryAllocatorRequests<kFuzzMaxRequests, kFuzzU32MaxSize>());

}  // namespace
}  // namespace pw::allocator
