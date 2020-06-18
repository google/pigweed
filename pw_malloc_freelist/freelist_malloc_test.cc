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

#include "pw_malloc_freelist/freelist_malloc.h"

#include <span>

#include "gtest/gtest.h"
#include "pw_allocator/freelist_heap.h"

namespace pw::allocator {

TEST(FreeListMalloc, ReplacingMalloc) {
  constexpr size_t kAllocSize = 256;
  constexpr size_t kReallocSize = 512;
  constexpr size_t kCallocNum = 4;
  constexpr size_t kCallocSize = 64;
  constexpr size_t zero = 0;

  void* ptr1 = malloc(kAllocSize);
  const FreeListHeap::HeapStats& freelist_heap_stats =
      pw_freelist_heap->heap_stats();
  ASSERT_NE(ptr1, nullptr);
  EXPECT_EQ(freelist_heap_stats.bytes_allocated, kAllocSize);
  EXPECT_EQ(freelist_heap_stats.cumulative_allocated, kAllocSize);
  EXPECT_EQ(freelist_heap_stats.cumulative_freed, zero);
  void* ptr2 = realloc(ptr1, kReallocSize);
  ASSERT_NE(ptr2, nullptr);
  EXPECT_EQ(freelist_heap_stats.bytes_allocated, kReallocSize);
  EXPECT_EQ(freelist_heap_stats.cumulative_allocated,
            kAllocSize + kReallocSize);
  EXPECT_EQ(freelist_heap_stats.cumulative_freed, kAllocSize);
  void* ptr3 = calloc(kCallocNum, kCallocSize);
  ASSERT_NE(ptr3, nullptr);
  EXPECT_EQ(freelist_heap_stats.bytes_allocated,
            kReallocSize + kCallocNum * kCallocSize);
  EXPECT_EQ(freelist_heap_stats.cumulative_allocated,
            kAllocSize + kReallocSize + kCallocNum * kCallocSize);
  EXPECT_EQ(freelist_heap_stats.cumulative_freed, kAllocSize);
  free(ptr2);
  EXPECT_EQ(freelist_heap_stats.bytes_allocated, kCallocNum * kCallocSize);
  EXPECT_EQ(freelist_heap_stats.cumulative_allocated,
            kAllocSize + kReallocSize + kCallocNum * kCallocSize);
  EXPECT_EQ(freelist_heap_stats.cumulative_freed, kAllocSize + kReallocSize);
  free(ptr3);
  EXPECT_EQ(freelist_heap_stats.bytes_allocated, zero);
  EXPECT_EQ(freelist_heap_stats.cumulative_allocated,
            kAllocSize + kReallocSize + kCallocNum * kCallocSize);
  EXPECT_EQ(freelist_heap_stats.cumulative_freed,
            kAllocSize + kReallocSize + kCallocNum * kCallocSize);
}

}  // namespace pw::allocator
