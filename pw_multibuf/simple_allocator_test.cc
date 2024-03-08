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

#include "pw_multibuf/simple_allocator.h"

#include "gtest/gtest.h"
#include "pw_allocator/allocator_testing.h"
#include "pw_allocator/null_allocator.h"

namespace pw::multibuf {
namespace {

using ::pw::allocator::test::AllocatorForTest;

constexpr size_t kArbitraryBufferSize = 1024;
constexpr size_t kArbitraryMetaSize = 1024;

TEST(SimpleAllocator, AllocateWholeDataAreaSizeSucceeds) {
  std::array<std::byte, kArbitraryBufferSize> data_area;
  AllocatorForTest<kArbitraryMetaSize> meta_alloc;
  SimpleAllocator simple_allocator(data_area, meta_alloc);
  std::optional<MultiBuf> buf = simple_allocator.Allocate(kArbitraryBufferSize);
  ASSERT_TRUE(buf.has_value());
  EXPECT_EQ(buf->size(), kArbitraryBufferSize);
}

TEST(SimpleAllocator, AllocateContiguousWholeDataAreaSizeSucceeds) {
  std::array<std::byte, kArbitraryBufferSize> data_area;
  AllocatorForTest<kArbitraryMetaSize> meta_alloc;
  SimpleAllocator simple_allocator(data_area, meta_alloc);
  std::optional<MultiBuf> buf =
      simple_allocator.AllocateContiguous(kArbitraryBufferSize);
  ASSERT_TRUE(buf.has_value());
  EXPECT_EQ(buf->Chunks().size(), 1U);
  EXPECT_EQ(buf->size(), kArbitraryBufferSize);
}

TEST(SimpleAllocator, AllocateContiguousHalfDataAreaSizeTwiceSucceeds) {
  std::array<std::byte, kArbitraryBufferSize> data_area;
  AllocatorForTest<kArbitraryMetaSize> meta_alloc;
  SimpleAllocator simple_allocator(data_area, meta_alloc);
  std::optional<MultiBuf> buf =
      simple_allocator.AllocateContiguous(kArbitraryBufferSize / 2);
  ASSERT_TRUE(buf.has_value());
  EXPECT_EQ(buf->Chunks().size(), 1U);
  EXPECT_EQ(buf->size(), kArbitraryBufferSize / 2);

  std::optional<MultiBuf> buf2 =
      simple_allocator.AllocateContiguous(kArbitraryBufferSize / 2);
  ASSERT_TRUE(buf2.has_value());
  EXPECT_EQ(buf2->Chunks().size(), 1U);
  EXPECT_EQ(buf2->size(), kArbitraryBufferSize / 2);
}

TEST(SimpleAllocator, AllocateTooLargeReturnsNullopt) {
  std::array<std::byte, kArbitraryBufferSize> data_area;
  AllocatorForTest<kArbitraryMetaSize> meta_alloc;
  SimpleAllocator simple_allocator(data_area, meta_alloc);
  std::optional<MultiBuf> buf =
      simple_allocator.Allocate(kArbitraryBufferSize + 1);
  EXPECT_FALSE(buf.has_value());
  std::optional<MultiBuf> contiguous_buf =
      simple_allocator.Allocate(kArbitraryBufferSize + 1);
  EXPECT_FALSE(contiguous_buf.has_value());
}

TEST(SimpleAllocator, AllocateZeroWithNoMetadataOrDataReturnsEmptyMultiBuf) {
  std::array<std::byte, 0> data_area;
  pw::allocator::NullAllocator meta_alloc;
  SimpleAllocator simple_allocator(data_area, meta_alloc);
  std::optional<MultiBuf> buf = simple_allocator.Allocate(0);
  ASSERT_TRUE(buf.has_value());
  EXPECT_EQ(buf->size(), 0U);
}

TEST(SimpleAllocator, AllocateWithNoMetadataRoomReturnsNullopt) {
  std::array<std::byte, kArbitraryBufferSize> data_area;
  pw::allocator::NullAllocator meta_alloc;
  SimpleAllocator simple_allocator(data_area, meta_alloc);
  std::optional<MultiBuf> buf = simple_allocator.Allocate(1);
  EXPECT_FALSE(buf.has_value());
}

TEST(SimpleAllocator, SecondLargeAllocationFailsUntilFirstAllocationReleased) {
  std::array<std::byte, kArbitraryBufferSize> data_area;
  AllocatorForTest<kArbitraryMetaSize> meta_alloc;
  SimpleAllocator simple_allocator(data_area, meta_alloc);
  const size_t alloc_size = kArbitraryBufferSize / 2 + 1;
  std::optional<MultiBuf> buf = simple_allocator.Allocate(alloc_size);
  ASSERT_TRUE(buf.has_value());
  EXPECT_EQ(buf->size(), alloc_size);
  EXPECT_FALSE(simple_allocator.Allocate(alloc_size).has_value());
  // Release the first buffer
  buf = std::nullopt;
  EXPECT_TRUE(simple_allocator.Allocate(alloc_size).has_value());
}

TEST(SimpleAllocator, AllocateSkipsMiddleAllocations) {
  std::array<std::byte, kArbitraryBufferSize> data_area;
  AllocatorForTest<kArbitraryMetaSize> meta_alloc;
  SimpleAllocator simple_allocator(data_area, meta_alloc);
  const size_t alloc_size = kArbitraryBufferSize / 3;
  std::optional<MultiBuf> buf1 = simple_allocator.Allocate(alloc_size);
  std::optional<MultiBuf> buf2 = simple_allocator.Allocate(alloc_size);
  std::optional<MultiBuf> buf3 = simple_allocator.Allocate(alloc_size);
  EXPECT_TRUE(buf1.has_value());
  EXPECT_TRUE(buf2.has_value());
  EXPECT_TRUE(buf3.has_value());
  buf1 = std::nullopt;
  buf3 = std::nullopt;
  // Now `buf2` holds the middle third of data_area
  std::optional<MultiBuf> split = simple_allocator.Allocate(alloc_size * 2);
  ASSERT_TRUE(split.has_value());
  EXPECT_EQ(split->size(), alloc_size * 2);
  EXPECT_EQ(split->Chunks().size(), 2U);
}

TEST(SimpleAllocator, FailedAllocationDoesNotHoldOntoChunks) {
  std::array<std::byte, kArbitraryBufferSize> data_area;
  AllocatorForTest<kArbitraryMetaSize> meta_alloc;
  SimpleAllocator simple_allocator(data_area, meta_alloc);
  const size_t alloc_size = kArbitraryBufferSize / 2;
  std::optional<MultiBuf> buf1 = simple_allocator.Allocate(alloc_size);
  std::optional<MultiBuf> buf2 = simple_allocator.Allocate(alloc_size);
  EXPECT_TRUE(buf1.has_value());
  EXPECT_TRUE(buf2.has_value());
  buf1 = std::nullopt;
  // When this allocation is attempted, it will initially create a chunk for
  // the first empty region prior to failing.
  EXPECT_FALSE(simple_allocator.Allocate(kArbitraryBufferSize).has_value());
  buf2 = std::nullopt;
  // Ensure that all chunk holds are released by attempting an allocation.
  EXPECT_TRUE(simple_allocator.Allocate(kArbitraryBufferSize).has_value());
}

}  // namespace
}  // namespace pw::multibuf
