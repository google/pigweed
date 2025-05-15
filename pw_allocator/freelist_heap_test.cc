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

#include "pw_allocator/freelist_heap.h"

#include "lib/stdcompat/bit.h"
#include "pw_allocator/block/testing.h"
#include "pw_bytes/alignment.h"
#include "pw_unit_test/framework.h"

namespace {

// Test fixtures.

using ::pw::allocator::FreeListHeapBuffer;
using ::pw::allocator::test::GetAlignedOffsetAfter;

class FreeListHeapBufferTest : public ::testing::Test {
 protected:
  using BlockType = ::pw::allocator::DetailedBlock<>;

  static constexpr size_t kN = 2048;

  alignas(BlockType::kAlignment) std::array<std::byte, kN> buffer_;
};

// Unit tests.

TEST_F(FreeListHeapBufferTest, CanAllocate) {
  FreeListHeapBuffer<> allocator(buffer_);

  void* ptr = allocator.Allocate(kN / 4);
  ASSERT_NE(ptr, nullptr);

  // The returned memory should be within the allocator's memory...
  EXPECT_GE(ptr, buffer_.data());
  EXPECT_LT(ptr, buffer_.data() + buffer_.size());

  // ...and should be usable.
  std::memset(ptr, 0xff, kN / 4);

  // All pointers must be freed before the allocator goes out of scope.
  allocator.Free(ptr);
}

TEST_F(FreeListHeapBufferTest, AllocationsDontOverlap) {
  FreeListHeapBuffer<> allocator(buffer_);

  void* ptr1 = allocator.Allocate(kN / 4);
  ASSERT_NE(ptr1, nullptr);
  uintptr_t ptr1_start = cpp20::bit_cast<uintptr_t>(ptr1);
  uintptr_t ptr1_end = ptr1_start + (kN / 4);

  void* ptr2 = allocator.Allocate(kN / 4);
  ASSERT_NE(ptr2, nullptr);
  uintptr_t ptr2_start = cpp20::bit_cast<uintptr_t>(ptr2);
  uintptr_t ptr2_end = ptr2_start + (kN / 4);

  if (ptr1 < ptr2) {
    EXPECT_LT(ptr1_end, ptr2_start);
  } else {
    EXPECT_LT(ptr2_end, ptr1_start);
  }

  // All pointers must be freed before the allocator goes out of scope.
  allocator.Free(ptr1);
  allocator.Free(ptr2);
}

TEST_F(FreeListHeapBufferTest, CanFreeAndRealloc) {
  FreeListHeapBuffer<> allocator(buffer_);

  void* ptr1 = allocator.Allocate(kN / 4);
  allocator.Free(ptr1);

  // There's not really a nice way to test that Free works, apart from to try
  // and get that value back again.
  void* ptr2 = allocator.Allocate(kN / 4);

  EXPECT_EQ(ptr1, ptr2);

  // All pointers must be freed before the allocator goes out of scope.
  allocator.Free(ptr2);
}

TEST_F(FreeListHeapBufferTest, ReturnsNullWhenAllocationTooLarge) {
  FreeListHeapBuffer<> allocator(buffer_);

  EXPECT_EQ(allocator.Allocate(kN), nullptr);
}

TEST_F(FreeListHeapBufferTest, ReturnsNullWhenFull) {
  size_t offset = GetAlignedOffsetAfter(
      buffer_.data(), alignof(std::max_align_t), BlockType::kBlockOverhead);
  auto buffer = pw::ByteSpan(buffer_).subspan(offset);
  size_t inner_size = buffer.size() - BlockType::kBlockOverhead;

  FreeListHeapBuffer<> allocator(buffer);
  void* ptr1 = allocator.Allocate(inner_size);
  ASSERT_NE(ptr1, nullptr);

  void* ptr2 = allocator.Allocate(1);
  EXPECT_EQ(ptr2, nullptr);

  // All pointers must be freed before the allocator goes out of scope.
  allocator.Free(ptr1);
}

TEST_F(FreeListHeapBufferTest, ReturnedPointersAreAligned) {
  FreeListHeapBuffer<> allocator(buffer_);

  void* ptr1 = allocator.Allocate(1);

  // Should be aligned to native pointer alignment
  uintptr_t ptr1_start = cpp20::bit_cast<uintptr_t>(ptr1);
  size_t alignment = alignof(void*);

  EXPECT_EQ(ptr1_start % alignment, static_cast<size_t>(0));

  void* ptr2 = allocator.Allocate(1);
  uintptr_t ptr2_start = cpp20::bit_cast<uintptr_t>(ptr2);

  EXPECT_EQ(ptr2_start % alignment, static_cast<size_t>(0));

  // All pointers must be freed before the allocator goes out of scope.
  allocator.Free(ptr1);
  allocator.Free(ptr2);
}

TEST_F(FreeListHeapBufferTest, CanRealloc) {
  FreeListHeapBuffer<> allocator(buffer_);

  void* ptr1 = allocator.Allocate(kN / 4);
  ASSERT_NE(ptr1, nullptr);

  void* ptr2 = allocator.Realloc(ptr1, (kN * 3) / 8);
  ASSERT_NE(ptr2, nullptr);

  // All pointers must be freed before the allocator goes out of scope.
  allocator.Free(ptr2);
}

TEST_F(FreeListHeapBufferTest, ReallocHasSameContent) {
  FreeListHeapBuffer<> allocator(buffer_);

  size_t val1 = 42;
  void* ptr1 = allocator.Allocate(sizeof(size_t));
  ASSERT_NE(ptr1, nullptr);
  std::memcpy(ptr1, &val1, sizeof(size_t));

  size_t val2;
  void* ptr2 = allocator.Realloc(ptr1, sizeof(size_t) * 2);
  ASSERT_NE(ptr2, nullptr);
  std::memcpy(&val2, ptr2, sizeof(size_t));

  // Verify that data inside the allocated and reallocated block are the same.
  EXPECT_EQ(val1, val2);

  // All pointers must be freed before the allocator goes out of scope.
  allocator.Free(ptr2);
}

TEST_F(FreeListHeapBufferTest, ReallocSmallerSize) {
  FreeListHeapBuffer<> allocator(buffer_);

  void* ptr1 = allocator.Allocate(kN / 4);
  ASSERT_NE(ptr1, nullptr);

  // For smaller sizes, Realloc will not shrink the block.
  void* ptr2 = allocator.Realloc(ptr1, kN / 8);
  EXPECT_EQ(ptr1, ptr2);

  // All pointers must be freed before the allocator goes out of scope.
  allocator.Free(ptr2);
}

TEST_F(FreeListHeapBufferTest, ReallocTooLarge) {
  FreeListHeapBuffer<> allocator(buffer_);

  void* ptr1 = allocator.Allocate(kN / 4);
  ASSERT_NE(ptr1, nullptr);

  // Realloc() will not invalidate the original pointer if it fails
  void* ptr2 = allocator.Realloc(ptr1, kN * 2);
  EXPECT_EQ(ptr2, nullptr);

  // All pointers must be freed before the allocator goes out of scope.
  allocator.Free(ptr1);
}

TEST_F(FreeListHeapBufferTest, CanCalloc) {
  constexpr size_t kNum = 4;
  constexpr size_t kSize = 128;
  constexpr std::array<std::byte, kNum * kSize> kZero = {std::byte(0)};

  FreeListHeapBuffer<> allocator(buffer_);

  void* ptr1 = allocator.Calloc(kNum, kSize);
  ASSERT_NE(ptr1, nullptr);

  // Calloc'd content is zero.
  EXPECT_EQ(std::memcmp(ptr1, kZero.data(), kZero.size()), 0);

  // All pointers must be freed before the allocator goes out of scope.
  allocator.Free(ptr1);
}

TEST_F(FreeListHeapBufferTest, CanCallocWeirdSize) {
  constexpr size_t kNum = 4;
  constexpr size_t kSize = 128;
  constexpr std::array<std::byte, kNum * kSize> kZero = {std::byte(0)};

  FreeListHeapBuffer<> allocator(buffer_);

  void* ptr1 = allocator.Calloc(kNum, kSize);
  ASSERT_NE(ptr1, nullptr);

  // Calloc'd content is zero.
  EXPECT_EQ(std::memcmp(ptr1, kZero.data(), kZero.size()), 0);

  // All pointers must be freed before the allocator goes out of scope.
  allocator.Free(ptr1);
}

TEST_F(FreeListHeapBufferTest, CallocTooLarge) {
  FreeListHeapBuffer<> allocator(buffer_);

  void* ptr1 = allocator.Calloc(1, kN + 1);
  EXPECT_EQ(ptr1, nullptr);

  // All pointers must be freed before the allocator goes out of scope.
  allocator.Free(ptr1);
}

}  // namespace
