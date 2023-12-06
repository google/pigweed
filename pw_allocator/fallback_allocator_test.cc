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

#include "pw_allocator/fallback_allocator.h"

#include "pw_allocator/allocator_testing.h"
#include "pw_status/status.h"
#include "pw_unit_test/framework.h"

namespace pw::allocator {
namespace {

// Test fixtures.

class FallbackAllocatorTest : public ::testing::Test {
 protected:
  void SetUp() override { allocator.Init(*primary, *secondary); }

  test::AllocatorForTestWithBuffer<128> primary;
  test::AllocatorForTestWithBuffer<128> secondary;
  FallbackAllocator allocator;
};

// Unit tests.

TEST_F(FallbackAllocatorTest, QueryValidPrimary) {
  Layout layout = Layout::Of<uint32_t>();
  void* ptr = primary->Allocate(layout);
  EXPECT_TRUE(primary->Query(ptr, layout).ok());
  EXPECT_EQ(secondary->Query(ptr, layout), Status::OutOfRange());
  EXPECT_TRUE(allocator.Query(ptr, layout).ok());
}

TEST_F(FallbackAllocatorTest, QueryValidSecondary) {
  Layout layout = Layout::Of<uint32_t>();
  void* ptr = secondary->Allocate(layout);
  EXPECT_FALSE(primary->Query(ptr, layout).ok());
  EXPECT_TRUE(secondary->Query(ptr, layout).ok());
  EXPECT_TRUE(allocator.Query(ptr, layout).ok());
}

TEST_F(FallbackAllocatorTest, QueryInvalidPtr) {
  std::array<std::byte, 128> buffer = {};
  test::AllocatorForTest other;
  ASSERT_EQ(other.Init(buffer), OkStatus());
  Layout layout = Layout::Of<uint32_t>();
  void* ptr = other.Allocate(layout);
  EXPECT_FALSE(primary->Query(ptr, layout).ok());
  EXPECT_FALSE(secondary->Query(ptr, layout).ok());
  EXPECT_FALSE(allocator.Query(ptr, layout).ok());
  other.Reset();
}

TEST_F(FallbackAllocatorTest, AllocateFromPrimary) {
  Layout layout = Layout::Of<uint32_t>();
  void* ptr = allocator.Allocate(layout);
  EXPECT_NE(ptr, nullptr);
  EXPECT_EQ(primary->allocate_size(), layout.size());
  EXPECT_EQ(secondary->allocate_size(), 0U);
}

TEST_F(FallbackAllocatorTest, AllocateFromSecondary) {
  primary->Exhaust();
  Layout layout = Layout::Of<uint32_t>();
  void* ptr = allocator.Allocate(layout);
  EXPECT_NE(ptr, nullptr);
  EXPECT_EQ(primary->allocate_size(), layout.size());
  EXPECT_EQ(secondary->allocate_size(), layout.size());
}

TEST_F(FallbackAllocatorTest, AllocateFailure) {
  Layout layout = Layout::Of<uint32_t[0x10000]>();
  void* ptr = allocator.Allocate(layout);
  EXPECT_EQ(ptr, nullptr);
  EXPECT_EQ(primary->allocate_size(), layout.size());
  EXPECT_EQ(secondary->allocate_size(), layout.size());
}

TEST_F(FallbackAllocatorTest, DeallocateUsingPrimary) {
  Layout layout = Layout::Of<uint32_t>();
  void* ptr = allocator.Allocate(layout);
  ASSERT_NE(ptr, nullptr);
  allocator.Deallocate(ptr, layout);
  EXPECT_EQ(primary->deallocate_ptr(), ptr);
  EXPECT_EQ(primary->deallocate_size(), layout.size());
  EXPECT_EQ(secondary->deallocate_ptr(), nullptr);
  EXPECT_EQ(secondary->deallocate_size(), 0U);
}

TEST_F(FallbackAllocatorTest, DeallocateUsingSecondary) {
  primary->Exhaust();
  Layout layout = Layout::Of<uint32_t>();
  void* ptr = allocator.Allocate(layout);
  ASSERT_NE(ptr, nullptr);
  allocator.Deallocate(ptr, layout);
  EXPECT_EQ(primary->deallocate_ptr(), nullptr);
  EXPECT_EQ(primary->deallocate_size(), 0U);
  EXPECT_EQ(secondary->deallocate_ptr(), ptr);
  EXPECT_EQ(secondary->deallocate_size(), layout.size());
}

TEST_F(FallbackAllocatorTest, ResizePrimary) {
  Layout old_layout = Layout::Of<uint32_t>();
  void* ptr = allocator.Allocate(old_layout);
  ASSERT_NE(ptr, nullptr);

  size_t new_size = sizeof(uint32_t[3]);
  EXPECT_TRUE(allocator.Resize(ptr, old_layout, new_size));
  EXPECT_EQ(primary->resize_ptr(), ptr);
  EXPECT_EQ(primary->resize_old_size(), old_layout.size());
  EXPECT_EQ(primary->resize_new_size(), new_size);

  // Secondary should not be touched.
  EXPECT_EQ(secondary->resize_ptr(), nullptr);
  EXPECT_EQ(secondary->resize_old_size(), 0U);
  EXPECT_EQ(secondary->resize_new_size(), 0U);
}

TEST_F(FallbackAllocatorTest, ResizePrimaryFailure) {
  Layout old_layout = Layout::Of<uint32_t>();
  void* ptr = allocator.Allocate(old_layout);
  ASSERT_NE(ptr, nullptr);
  primary->Exhaust();

  size_t new_size = sizeof(uint32_t[3]);
  EXPECT_FALSE(allocator.Resize(ptr, old_layout, new_size));
  EXPECT_EQ(primary->resize_ptr(), ptr);
  EXPECT_EQ(primary->resize_old_size(), old_layout.size());
  EXPECT_EQ(primary->resize_new_size(), new_size);

  // Secondary should not be touched.
  EXPECT_EQ(secondary->resize_ptr(), nullptr);
  EXPECT_EQ(secondary->resize_old_size(), 0U);
  EXPECT_EQ(secondary->resize_new_size(), 0U);
}

TEST_F(FallbackAllocatorTest, ResizeSecondary) {
  primary->Exhaust();
  Layout old_layout = Layout::Of<uint32_t>();
  void* ptr = allocator.Allocate(old_layout);
  ASSERT_NE(ptr, nullptr);

  size_t new_size = sizeof(uint32_t[3]);
  EXPECT_TRUE(allocator.Resize(ptr, old_layout, new_size));
  EXPECT_EQ(secondary->resize_ptr(), ptr);
  EXPECT_EQ(secondary->resize_old_size(), old_layout.size());
  EXPECT_EQ(secondary->resize_new_size(), new_size);

  // Primary should not be touched.
  EXPECT_EQ(primary->resize_ptr(), nullptr);
  EXPECT_EQ(primary->resize_old_size(), 0U);
  EXPECT_EQ(primary->resize_new_size(), 0U);
}

TEST_F(FallbackAllocatorTest, ResizeSecondaryFailure) {
  primary->Exhaust();
  Layout old_layout = Layout::Of<uint32_t>();
  void* ptr = allocator.Allocate(old_layout);
  ASSERT_NE(ptr, nullptr);
  secondary->Exhaust();

  size_t new_size = sizeof(uint32_t[3]);
  EXPECT_FALSE(allocator.Resize(ptr, old_layout, new_size));
  EXPECT_EQ(secondary->resize_ptr(), ptr);
  EXPECT_EQ(secondary->resize_old_size(), old_layout.size());
  EXPECT_EQ(secondary->resize_new_size(), new_size);

  // Primary should not be touched.
  EXPECT_EQ(primary->resize_ptr(), nullptr);
  EXPECT_EQ(primary->resize_old_size(), 0U);
  EXPECT_EQ(primary->resize_new_size(), 0U);
}

TEST_F(FallbackAllocatorTest, ReallocateSameAllocator) {
  Layout old_layout = Layout::Of<uint32_t>();
  void* ptr1 = allocator.Allocate(old_layout);
  ASSERT_NE(ptr1, nullptr);

  // Claim subsequent memeory to force reallocation.
  void* ptr2 = allocator.Allocate(old_layout);
  ASSERT_NE(ptr2, nullptr);

  size_t new_size = sizeof(uint32_t[3]);
  void* new_ptr = allocator.Reallocate(ptr1, old_layout, new_size);
  EXPECT_NE(new_ptr, nullptr);
  EXPECT_EQ(primary->deallocate_ptr(), ptr1);
  EXPECT_EQ(primary->deallocate_size(), old_layout.size());
  EXPECT_EQ(primary->allocate_size(), new_size);
}

TEST_F(FallbackAllocatorTest, ReallocateDifferentAllocator) {
  Layout old_layout = Layout::Of<uint32_t>();
  void* ptr = allocator.Allocate(old_layout);
  primary->Exhaust();

  size_t new_size = sizeof(uint32_t[3]);
  void* new_ptr = allocator.Reallocate(ptr, old_layout, new_size);
  EXPECT_NE(new_ptr, nullptr);
  EXPECT_EQ(primary->deallocate_ptr(), ptr);
  EXPECT_EQ(primary->deallocate_size(), old_layout.size());
  EXPECT_EQ(secondary->allocate_size(), new_size);
}

}  // namespace
}  // namespace pw::allocator
