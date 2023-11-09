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
#include "pw_allocator/allocator_testing.h"
#include "pw_bytes/alignment.h"

namespace pw::allocator {
namespace {

// Test fixtures.

class AllocatorTest : public ::testing::Test {
 protected:
  void SetUp() override { EXPECT_EQ(allocator.Init(buffer), OkStatus()); }
  void TearDown() override { allocator.DeallocateAll(); }

  test::AllocatorForTest allocator;

 private:
  std::array<std::byte, 256> buffer = {};
};

// Unit tests

TEST_F(AllocatorTest, ReallocateNull) {
  constexpr Layout old_layout = Layout::Of<uint32_t>();
  size_t new_size = old_layout.size();
  void* new_ptr = allocator.Reallocate(nullptr, old_layout, new_size);

  // Resize should fail and Reallocate should call Allocate.
  EXPECT_EQ(allocator.allocate_size(), new_size);

  // Deallocate should not be called.
  EXPECT_EQ(allocator.deallocate_ptr(), nullptr);
  EXPECT_EQ(allocator.deallocate_size(), 0U);

  // Overall, Reallocate should succeed.
  EXPECT_NE(new_ptr, nullptr);
}

TEST_F(AllocatorTest, ReallocateZeroNewSize) {
  constexpr Layout old_layout = Layout::Of<uint32_t[3]>();
  void* ptr = allocator.Allocate(old_layout);
  ASSERT_EQ(allocator.allocate_size(), old_layout.size());
  ASSERT_NE(ptr, nullptr);
  allocator.ResetParameters();

  size_t new_size = 0;
  void* new_ptr = allocator.Reallocate(ptr, old_layout, new_size);

  // Reallocate does not call Resize, Allocate, or Deallocate.
  EXPECT_EQ(allocator.resize_ptr(), nullptr);
  EXPECT_EQ(allocator.resize_old_size(), 0U);
  EXPECT_EQ(allocator.resize_new_size(), 0U);
  EXPECT_EQ(allocator.allocate_size(), 0U);
  EXPECT_EQ(allocator.deallocate_ptr(), nullptr);
  EXPECT_EQ(allocator.deallocate_size(), 0U);

  // Overall, Reallocate should fail.
  EXPECT_EQ(new_ptr, nullptr);
}

TEST_F(AllocatorTest, ReallocateSame) {
  constexpr Layout layout = Layout::Of<uint32_t[3]>();
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
  constexpr Layout old_layout = Layout::Of<uint32_t[3]>();
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
  constexpr Layout old_layout = Layout::Of<uint32_t>();
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

  size_t new_size = sizeof(uint32_t[3]);
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
