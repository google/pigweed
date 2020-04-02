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

#include "gtest/gtest.h"
#include "pw_span/span.h"

namespace pw::allocator {

TEST(FreeListHeap, CanAllocate) {
  constexpr size_t N = 2048;
  constexpr size_t kAllocSize = 512;
  std::byte buf[N] = {std::byte(0)};

  FreeListHeapBuffer allocator(buf);

  void* data = allocator.Allocate(kAllocSize);

  ASSERT_NE(data, nullptr);

  // In this case, the allocator should be returning us the start of the chunk.
  EXPECT_EQ(data, &buf[0] + sizeof(Block));
}

TEST(FreeListHeap, AllocationsDontOverlap) {
  constexpr size_t N = 2048;
  constexpr size_t kAllocSize = 512;
  std::byte buf[N] = {std::byte(0)};

  FreeListHeapBuffer allocator(buf);

  void* d1 = allocator.Allocate(kAllocSize);
  void* d2 = allocator.Allocate(kAllocSize);

  ASSERT_NE(d1, nullptr);
  ASSERT_NE(d2, nullptr);

  uintptr_t d1_start = reinterpret_cast<uintptr_t>(d1);
  uintptr_t d1_end = d1_start + kAllocSize;
  uintptr_t d2_start = reinterpret_cast<uintptr_t>(d2);

  EXPECT_GT(d2_start, d1_end);
}

TEST(FreeListHeap, CanFreeAndRealloc) {
  // There's not really a nice way to test that Free works, apart from to try
  // and get that value back again.
  constexpr size_t N = 2048;
  constexpr size_t kAllocSize = 512;
  std::byte buf[N] = {std::byte(0)};

  FreeListHeapBuffer allocator(buf);

  void* data1 = allocator.Allocate(kAllocSize);
  allocator.Free(data1);
  void* data2 = allocator.Allocate(kAllocSize);

  EXPECT_EQ(data1, data2);
}

TEST(FreeListHeap, ReturnsNullWhenAllocationTooLarge) {
  constexpr size_t N = 2048;
  std::byte buf[N] = {std::byte(0)};

  FreeListHeapBuffer allocator(buf);

  void* data = allocator.Allocate(N);

  EXPECT_EQ(data, nullptr);
}

TEST(FreeListHeap, ReturnsNullWhenFull) {
  constexpr size_t N = 2048;
  std::byte buf[N] = {std::byte(0)};

  FreeListHeapBuffer allocator(buf);

  void* data1 = allocator.Allocate(N - sizeof(Block));
  EXPECT_NE(data1, nullptr);

  void* data2 = allocator.Allocate(1);
  EXPECT_EQ(data2, nullptr);
}

TEST(FreeListHeap, ReturnedPointersAreAligned) {
  constexpr size_t N = 2048;
  std::byte buf[N] = {std::byte(0)};

  FreeListHeapBuffer allocator(buf);

  void* data1 = allocator.Allocate(1);

  // Should be aligned to native pointer alignment
  uintptr_t data1_start = reinterpret_cast<uintptr_t>(data1);
  size_t alignment = alignof(void*);

  EXPECT_EQ(data1_start % alignment, static_cast<size_t>(0));

  void* data2 = allocator.Allocate(1);
  uintptr_t data2_start = reinterpret_cast<uintptr_t>(data2);

  EXPECT_EQ(data2_start % alignment, static_cast<size_t>(0));
}

TEST(FreeListHeap, CannotFreeNonOwnedPointer) {
  // This is a nasty one to test without looking at the internals of FreeList.
  // We can cheat; create a heap, allocate it all, and try and return something
  // random to it. Try allocating again, and check that we get nullptr back.
  constexpr size_t N = 2048;
  std::byte buf[N] = {std::byte(0)};

  FreeListHeapBuffer allocator(buf);

  void* data = allocator.Allocate(N - sizeof(Block));

  ASSERT_NE(data, nullptr);

  // Free some random address past the end
  allocator.Free(static_cast<std::byte*>(data) + N * 2);

  void* data_ahead = allocator.Allocate(1);
  EXPECT_EQ(data_ahead, nullptr);

  // And try before
  allocator.Free(static_cast<std::byte*>(data) - N);

  void* data_before = allocator.Allocate(1);
  EXPECT_EQ(data_before, nullptr);
}

}  // namespace pw::allocator
