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

#include <cstddef>

#include "pw_allocator/allocator.h"
#include "pw_allocator/testing.h"
#include "pw_unit_test/framework.h"

namespace {

using AllocatorForTest = ::pw::allocator::test::AllocatorForTest<256>;

TEST(UniquePtrTest, DefaultInitializationIsNullptr) {
  pw::UniquePtr<int> empty;
  EXPECT_EQ(empty.get(), nullptr);
}

TEST(UniquePtrTest, OperatorEqNullptrOnEmptyUniquePtrSucceeds) {
  pw::UniquePtr<int> empty;
  EXPECT_TRUE(empty == nullptr);
  EXPECT_FALSE(empty != nullptr);
}

TEST(UniquePtrTest, OperatorEqNullptrAfterMakeUniqueFails) {
  AllocatorForTest allocator;
  pw::UniquePtr<int> ptr = allocator.MakeUnique<int>(5);
  EXPECT_TRUE(ptr != nullptr);
  EXPECT_FALSE(ptr == nullptr);
}

TEST(UniquePtrTest, OperatorEqNullptrAfterMakeUniqueNullptrTypeFails) {
  AllocatorForTest allocator;
  pw::UniquePtr<std::nullptr_t> ptr =
      allocator.MakeUnique<std::nullptr_t>(nullptr);
  EXPECT_TRUE(ptr != nullptr);
  EXPECT_FALSE(ptr == nullptr);
  EXPECT_TRUE(*ptr == nullptr);
  EXPECT_FALSE(*ptr != nullptr);
}

TEST(UniquePtrTest, MakeUniqueForwardsConstructorArguments) {
  class MoveOnly {
   public:
    MoveOnly(int value) : value_(value) {}
    MoveOnly(MoveOnly&) = delete;
    MoveOnly(MoveOnly&&) {}
    int Value() const { return value_; }

   private:
    int value_;
  };

  class BuiltWithMoveOnly {
   public:
    BuiltWithMoveOnly() = delete;
    BuiltWithMoveOnly(MoveOnly&& mo) : value_(mo.Value()) {}
    int Value() const { return value_; }

   private:
    int value_;
  };

  AllocatorForTest allocator;
  MoveOnly mo(6);
  pw::UniquePtr<BuiltWithMoveOnly> ptr =
      allocator.MakeUnique<BuiltWithMoveOnly>(std::move(mo));
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(ptr->Value(), 6);
}

TEST(UniquePtrTest, MoveConstructsFromSubClassAndFreesTotalSize) {
  struct Base {};
  struct LargerSub : public Base {
    std::array<std::byte, 128> mem;
  };
  AllocatorForTest allocator;
  pw::UniquePtr<LargerSub> ptr = allocator.MakeUnique<LargerSub>();
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(allocator.allocate_size(), 128ul);
  pw::UniquePtr<Base> base_ptr(std::move(ptr));

  EXPECT_EQ(allocator.deallocate_size(), 0ul);
  // The size that is deallocated here should be the size of the larger
  // subclass, not the size of the smaller base class.
  base_ptr.Reset();
  EXPECT_EQ(allocator.deallocate_size(), 128ul);
}

TEST(UniquePtrTest, MoveAssignsFromSubClassAndFreesTotalSize) {
  struct Base {};
  struct LargerSub : public Base {
    std::array<std::byte, 128> mem;
  };
  AllocatorForTest allocator;
  pw::UniquePtr<LargerSub> ptr = allocator.MakeUnique<LargerSub>();
  ASSERT_NE(ptr, nullptr);
  EXPECT_EQ(allocator.allocate_size(), 128ul);
  pw::UniquePtr<Base> base_ptr = std::move(ptr);

  EXPECT_EQ(allocator.deallocate_size(), 0ul);
  // The size that is deallocated here should be the size of the larger
  // subclass, not the size of the smaller base class.
  base_ptr.Reset();
  EXPECT_EQ(allocator.deallocate_size(), 128ul);
}

TEST(UniquePtrTest, MoveAssignsToExistingDeallocates) {
  AllocatorForTest allocator;
  pw::UniquePtr<size_t> size1 = allocator.MakeUnique<size_t>(1);
  ASSERT_NE(size1, nullptr);
  EXPECT_EQ(*size1, 1U);

  pw::UniquePtr<size_t> size2 = allocator.MakeUnique<size_t>(2);
  ASSERT_NE(size1, nullptr);
  EXPECT_EQ(*size2, 2U);

  EXPECT_EQ(allocator.deallocate_size(), 0U);
  size1 = std::move(size2);
  EXPECT_EQ(allocator.deallocate_size(), sizeof(size_t));
  EXPECT_EQ(*size1, 2U);
}

TEST(UniquePtrTest, DestructorDestroysAndFrees) {
  int count = 0;
  class DestructorCounter {
   public:
    DestructorCounter(int& count) : count_(&count) {}
    ~DestructorCounter() { (*count_)++; }

   private:
    int* count_;
  };
  AllocatorForTest allocator;
  pw::UniquePtr<DestructorCounter> ptr =
      allocator.MakeUnique<DestructorCounter>(count);
  ASSERT_NE(ptr, nullptr);

  EXPECT_EQ(count, 0);
  EXPECT_EQ(allocator.deallocate_size(), 0ul);
  ptr.Reset();  // Reset the UniquePtr, destroying its contents.
  EXPECT_EQ(count, 1);
  EXPECT_EQ(allocator.deallocate_size(), sizeof(DestructorCounter));
}

TEST(UniquePtrTest, CanRelease) {
  struct Size final {
    size_t value;
  };
  Size* size_ptr = nullptr;
  AllocatorForTest allocator;
  {
    pw::UniquePtr<Size> ptr = allocator.MakeUnique<Size>(Size{.value = 1});
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr.deallocator(), &allocator);
    size_ptr = ptr.Release();

    // Allocator pointer parameter is optional. Re-releasing returns null.
    EXPECT_EQ(ptr.Release(), nullptr);
  }

  // Deallocate should not be called, even though UniquePtr goes out of scope.
  EXPECT_EQ(allocator.deallocate_size(), 0U);
  allocator.Delete(size_ptr);
  EXPECT_EQ(allocator.deallocate_size(), sizeof(Size));
}

}  // namespace
