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

#include "pw_allocator/block_allocator_testing.h"

#include <cstdint>

#include "lib/stdcompat/bit.h"
#include "pw_allocator/layout.h"
#include "pw_assert/check.h"
#include "pw_bytes/alignment.h"
#include "pw_status/status.h"

namespace pw::allocator::test {

// Test fixtures.

void BlockAllocatorTestBase::SetUp() { ptrs_.fill(nullptr); }

void BlockAllocatorTestBase::Store(size_t index, void* ptr) {
  PW_CHECK_UINT_LT(index, kNumPtrs, "index is out of bounds");
  PW_CHECK(ptr == nullptr || ptrs_[index] == nullptr,
           "assigning pointer would clobber existing allocation");
  ptrs_[index] = ptr;
}

void* BlockAllocatorTestBase::Fetch(size_t index) {
  return index < kNumPtrs ? ptrs_[index] : nullptr;
}

void BlockAllocatorTestBase::Swap(size_t i, size_t j) {
  std::swap(ptrs_[i], ptrs_[j]);
}

void BlockAllocatorTestBase::UseMemory(void* ptr, size_t size) {
  std::memset(ptr, 0x5a, size);
}

// Unit tests.

void BlockAllocatorTestBase::GetCapacity(size_t expected) {
  Allocator& allocator = GetGenericAllocator();
  EXPECT_EQ(allocator.GetCapacity(), expected);
}

void BlockAllocatorTestBase::AllocateLarge() {
  Allocator& allocator = GetGenericAllocator();
  constexpr Layout layout = Layout::Of<std::byte[kLargeInnerSize]>();
  Store(0, allocator.Allocate(layout));
  ASSERT_NE(Fetch(0), nullptr);
  ByteSpan bytes = GetBytes();
  EXPECT_GE(Fetch(0), bytes.data());
  EXPECT_LE(Fetch(0), bytes.data() + bytes.size());
  UseMemory(Fetch(0), layout.size());
}

void BlockAllocatorTestBase::AllocateSmall() {
  Allocator& allocator = GetGenericAllocator();
  constexpr Layout layout = Layout::Of<std::byte[kSmallInnerSize]>();
  Store(0, allocator.Allocate(layout));
  ASSERT_NE(Fetch(0), nullptr);
  ByteSpan bytes = GetBytes();
  EXPECT_GE(Fetch(0), bytes.data());
  EXPECT_LE(Fetch(0), bytes.data() + bytes.size());
  UseMemory(Fetch(0), layout.size());
}

void BlockAllocatorTestBase::AllocateTooLarge() {
  Allocator& allocator = GetGenericAllocator();
  Store(0, allocator.Allocate(Layout::Of<std::byte[kCapacity * 2]>()));
  EXPECT_EQ(Fetch(0), nullptr);
}

void BlockAllocatorTestBase::DeallocateNull() {
  Allocator& allocator = GetGenericAllocator();
  allocator.Deallocate(nullptr);
}

void BlockAllocatorTestBase::DeallocateShuffled() {
  Allocator& allocator = GetGenericAllocator();
  constexpr Layout layout = Layout::Of<std::byte[kSmallInnerSize]>();
  for (size_t i = 0; i < kNumPtrs; ++i) {
    Store(i, allocator.Allocate(layout));
    if (Fetch(i) == nullptr) {
      break;
    }
  }

  // Mix up the order of allocations.
  for (size_t i = 0; i < kNumPtrs; ++i) {
    if (i % 2 == 0 && i + 1 < kNumPtrs) {
      Swap(i, i + 1);
    }
    if (i % 3 == 0 && i + 2 < kNumPtrs) {
      Swap(i, i + 2);
    }
  }

  // Deallocate everything.
  for (size_t i = 0; i < kNumPtrs; ++i) {
    allocator.Deallocate(Fetch(i));
    Store(i, nullptr);
  }
}

void BlockAllocatorTestBase::ResizeNull() {
  Allocator& allocator = GetGenericAllocator();
  size_t new_size = 1;
  EXPECT_FALSE(allocator.Resize(nullptr, new_size));
}

void BlockAllocatorTestBase::ResizeLargeSame() {
  Allocator& allocator = GetGenericAllocator({
      {kLargeOuterSize, Preallocation::kUsed},
      {Preallocation::kSizeRemaining, Preallocation::kUsed},
  });
  size_t new_size = kLargeInnerSize;
  ASSERT_TRUE(allocator.Resize(Fetch(0), new_size));
  UseMemory(Fetch(0), kLargeInnerSize);
}

void BlockAllocatorTestBase::ResizeLargeSmaller() {
  Allocator& allocator = GetGenericAllocator({
      {kLargeOuterSize, Preallocation::kUsed},
      {Preallocation::kSizeRemaining, Preallocation::kUsed},
  });
  size_t new_size = kSmallInnerSize;
  ASSERT_TRUE(allocator.Resize(Fetch(0), new_size));
  UseMemory(Fetch(0), kSmallInnerSize);
}

void BlockAllocatorTestBase::ResizeLargeLarger() {
  Allocator& allocator = GetGenericAllocator({
      {kLargeOuterSize, Preallocation::kUsed},
      {kLargeOuterSize, Preallocation::kFree},
      {Preallocation::kSizeRemaining, Preallocation::kUsed},
  });
  size_t new_size = kLargeInnerSize * 2;
  ASSERT_TRUE(allocator.Resize(Fetch(0), new_size));
  UseMemory(Fetch(0), kLargeInnerSize * 2);
}

void BlockAllocatorTestBase::ResizeLargeLargerFailure() {
  Allocator& allocator = GetGenericAllocator({
      {kLargeOuterSize, Preallocation::kUsed},
      {Preallocation::kSizeRemaining, Preallocation::kUsed},
  });
  // Memory after ptr is already allocated, so `Resize` should fail.
  size_t new_size = kLargeInnerSize * 2;
  EXPECT_FALSE(allocator.Resize(Fetch(0), new_size));
}

void BlockAllocatorTestBase::ResizeSmallSame() {
  Allocator& allocator = GetGenericAllocator({
      {kSmallOuterSize, Preallocation::kUsed},
      {Preallocation::kSizeRemaining, Preallocation::kUsed},
  });
  size_t new_size = kSmallInnerSize;
  ASSERT_TRUE(allocator.Resize(Fetch(0), new_size));
  UseMemory(Fetch(0), kSmallInnerSize);
}

void BlockAllocatorTestBase::ResizeSmallSmaller() {
  Allocator& allocator = GetGenericAllocator({
      {kSmallOuterSize, Preallocation::kUsed},
      {Preallocation::kSizeRemaining, Preallocation::kUsed},
  });
  size_t new_size = kSmallInnerSize / 2;
  ASSERT_TRUE(allocator.Resize(Fetch(0), new_size));
  UseMemory(Fetch(0), kSmallInnerSize / 2);
}

void BlockAllocatorTestBase::ResizeSmallLarger() {
  Allocator& allocator = GetGenericAllocator({
      {kSmallOuterSize, Preallocation::kUsed},
      {kSmallOuterSize, Preallocation::kFree},
      {Preallocation::kSizeRemaining, Preallocation::kUsed},
  });
  size_t new_size = kSmallInnerSize * 2;
  ASSERT_TRUE(allocator.Resize(Fetch(0), new_size));
  UseMemory(Fetch(0), kSmallInnerSize * 2);
}

void BlockAllocatorTestBase::ResizeSmallLargerFailure() {
  Allocator& allocator = GetGenericAllocator({
      {kSmallOuterSize, Preallocation::kUsed},
      {Preallocation::kSizeRemaining, Preallocation::kUsed},
  });
  // Memory after ptr is already allocated, so `Resize` should fail.
  size_t new_size = kSmallInnerSize * 2 + kDefaultBlockOverhead;
  EXPECT_FALSE(allocator.Resize(Fetch(0), new_size));
}

}  // namespace pw::allocator::test
