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

#include "pw_allocator/allocator.h"

#include <cstddef>

#include "gtest/gtest.h"
#include "pw_allocator_private/allocator_testing.h"
#include "pw_bytes/alignment.h"

namespace pw::allocator {
namespace {

using test::FakeAllocator;

// Test fixtures.

struct AllocatorTest : ::testing::Test {
  FakeAllocator allocator;
  std::array<std::byte, 256> buffer;

  void SetUp() override { EXPECT_EQ(allocator.Initialize(buffer), OkStatus()); }
};

struct TestStruct {
  uint32_t a;
  uint32_t b;
  uint32_t c;
};

// Unit tests

TEST_F(AllocatorTest, ReallocateNull) {
  Layout old_layout = Layout::Of<uint32_t>();
  size_t new_size = old_layout.size();
  void* new_ptr = allocator.Reallocate(nullptr, old_layout, new_size);

  // Reallocate should call Resize.
  EXPECT_EQ(allocator.resize_ptr(), nullptr);
  EXPECT_EQ(allocator.resize_old_size(), old_layout.size());
  EXPECT_EQ(allocator.resize_new_size(), new_size);

  // Resize should fail and Reallocate should call Allocate.
  EXPECT_EQ(allocator.allocate_size(), new_size);

  // Deallocate should not be called.
  EXPECT_EQ(allocator.deallocate_ptr(), nullptr);
  EXPECT_EQ(allocator.deallocate_size(), 0U);

  // Overall, Reallocate should succeed.
  EXPECT_NE(new_ptr, nullptr);
}

TEST_F(AllocatorTest, ReallocateZeroNewSize) {
  Layout old_layout = Layout::Of<TestStruct>();
  void* ptr = allocator.Allocate(old_layout);
  ASSERT_EQ(allocator.allocate_size(), old_layout.size());
  ASSERT_NE(ptr, nullptr);

  size_t new_size = 0;
  void* new_ptr = allocator.Reallocate(ptr, old_layout, new_size);

  // Reallocate should call Resize.
  EXPECT_EQ(allocator.resize_ptr(), ptr);
  EXPECT_EQ(allocator.resize_old_size(), old_layout.size());
  EXPECT_EQ(allocator.resize_new_size(), new_size);

  // Resize should fail and Reallocate should call Allocate.
  EXPECT_EQ(allocator.allocate_size(), new_size);

  // Deallocate should not be called.
  EXPECT_EQ(allocator.deallocate_ptr(), nullptr);
  EXPECT_EQ(allocator.deallocate_size(), 0U);

  // Overall, Reallocate should fail.
  EXPECT_EQ(new_ptr, nullptr);
}

TEST_F(AllocatorTest, ReallocateSame) {
  Layout layout = Layout::Of<TestStruct>();
  void* ptr = allocator.Allocate(layout);
  ASSERT_EQ(allocator.allocate_size(), layout.size());
  ASSERT_NE(ptr, nullptr);
  allocator.ResetParameters();

  void* new_ptr = allocator.Reallocate(ptr, layout, layout.size());

  // Reallocate should call Resize.
  EXPECT_EQ(allocator.resize_ptr(), ptr);
  EXPECT_EQ(allocator.resize_old_size(), layout.size());
  EXPECT_EQ(allocator.resize_new_size(), layout.size());

  // Allocate should not be called.
  EXPECT_EQ(allocator.allocate_size(), 0U);

  // Deallocate should not be called.
  EXPECT_EQ(allocator.deallocate_ptr(), nullptr);
  EXPECT_EQ(allocator.deallocate_size(), 0U);

  // Overall, Reallocate should succeed.
  EXPECT_EQ(new_ptr, ptr);
}

TEST_F(AllocatorTest, ReallocateSmaller) {
  Layout old_layout = Layout::Of<TestStruct>();
  void* ptr = allocator.Allocate(old_layout);
  ASSERT_EQ(allocator.allocate_size(), old_layout.size());
  ASSERT_NE(ptr, nullptr);
  allocator.ResetParameters();

  size_t new_size = sizeof(uint32_t);
  void* new_ptr = allocator.Reallocate(ptr, old_layout, new_size);

  // Reallocate should call Resize.
  EXPECT_EQ(allocator.resize_ptr(), ptr);
  EXPECT_EQ(allocator.resize_old_size(), old_layout.size());
  EXPECT_EQ(allocator.resize_new_size(), new_size);

  // Allocate should not be called.
  EXPECT_EQ(allocator.allocate_size(), 0U);

  // Deallocate should not be called.
  EXPECT_EQ(allocator.deallocate_ptr(), nullptr);
  EXPECT_EQ(allocator.deallocate_size(), 0U);

  // Overall, Reallocate should succeed.
  EXPECT_EQ(new_ptr, ptr);
}

TEST_F(AllocatorTest, ReallocateLarger) {
  Layout old_layout = Layout::Of<uint32_t>();
  void* ptr = allocator.Allocate(old_layout);
  ASSERT_EQ(allocator.allocate_size(), old_layout.size());
  ASSERT_NE(ptr, nullptr);

  // The abstraction is a bit leaky here: This tests relies on the details of
  // `Resize` in order to get it to fail and fallback to
  // allocate/copy/deallocate. Allocate a second block, which should prevent the
  // first one from being able to grow.
  void* next = allocator.Allocate(old_layout);
  ASSERT_EQ(allocator.allocate_size(), old_layout.size());
  ASSERT_NE(next, nullptr);
  allocator.ResetParameters();

  size_t new_size = sizeof(TestStruct);
  void* new_ptr = allocator.Reallocate(ptr, old_layout, new_size);

  // Reallocate should call Resize.
  EXPECT_EQ(allocator.resize_ptr(), ptr);
  EXPECT_EQ(allocator.resize_old_size(), old_layout.size());
  EXPECT_EQ(allocator.resize_new_size(), new_size);

  // Resize should fail and Reallocate should call Allocate.
  EXPECT_EQ(allocator.allocate_size(), new_size);

  // Deallocate should also be called.
  EXPECT_EQ(allocator.deallocate_ptr(), ptr);
  EXPECT_EQ(allocator.deallocate_size(), old_layout.size());

  // Overall, Reallocate should succeed.
  EXPECT_NE(new_ptr, nullptr);
  EXPECT_NE(new_ptr, ptr);
}

}  // namespace
}  // namespace pw::allocator
