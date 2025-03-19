// Copyright 2025 The Pigweed Authors
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

#include "pw_allocator/weak_ptr.h"

// TODO(b/402489948): Remove when portable atomics are provided by `pw_atomic`.
#if PW_ALLOCATOR_HAS_ATOMICS

#include <cstddef>

#include "pw_allocator/allocator.h"
#include "pw_allocator/internal/managed_ptr_testing.h"
#include "pw_allocator/shared_ptr.h"
#include "pw_unit_test/framework.h"

namespace {

using pw::allocator::test::Counter;
using WeakPtrTest = pw::allocator::test::ManagedPtrTest;

TEST_F(WeakPtrTest, DefaultInitializationIsExpired) {
  pw::WeakPtr<int> weak;
  EXPECT_EQ(weak.use_count(), 0);
  EXPECT_TRUE(weak.expired());
  EXPECT_EQ(weak.Lock(), nullptr);
}

TEST_F(WeakPtrTest, CanConstructMultipleFromSingleSharedPtr) {
  auto shared = allocator_.MakeShared<int>(42);
  pw::WeakPtr<int> weak1(shared);
  EXPECT_EQ(weak1.use_count(), 1);
  EXPECT_FALSE(weak1.expired());

  pw::WeakPtr<int> weak2(shared);
  EXPECT_EQ(weak1.use_count(), 1);
  EXPECT_EQ(weak2.use_count(), 1);
  EXPECT_FALSE(weak1.expired());
  EXPECT_FALSE(weak2.expired());
}

TEST_F(WeakPtrTest, CanLockWhenActive) {
  auto shared1 = allocator_.MakeShared<int>(42);
  pw::WeakPtr<int> weak(shared1);

  EXPECT_EQ(weak.use_count(), 1);
  EXPECT_FALSE(weak.expired());

  auto shared2 = weak.Lock();
  ASSERT_NE(shared2.get(), nullptr);
  EXPECT_EQ(*shared2, 42);
}

TEST_F(WeakPtrTest, CannotLockWhenExpired) {
  auto shared = allocator_.MakeShared<int>(42);
  pw::WeakPtr<int> weak(shared);

  shared.reset();
  EXPECT_EQ(weak.use_count(), 0);
  EXPECT_TRUE(weak.expired());

  auto shared2 = weak.Lock();
  EXPECT_EQ(shared2.get(), nullptr);
}

TEST_F(WeakPtrTest, CanCopyConstructWhenActive) {
  auto shared1 = allocator_.MakeShared<int>(42);
  pw::WeakPtr<int> weak1(shared1);
  pw::WeakPtr<int> weak2(weak1);

  EXPECT_EQ(weak1.use_count(), 1);
  EXPECT_EQ(weak2.use_count(), 1);
  EXPECT_FALSE(weak1.expired());
  EXPECT_FALSE(weak2.expired());

  auto shared2 = weak2.Lock();
  ASSERT_NE(shared2.get(), nullptr);
  EXPECT_EQ(*shared2, 42);
}

TEST_F(WeakPtrTest, CanCopyConstructWhenExpired) {
  auto shared1 = allocator_.MakeShared<int>(42);
  pw::WeakPtr<int> weak1(shared1);

  // resetting the shared pointer should delete the object, but not the control
  // block.
  size_t allocated = allocator_.allocate_size();
  shared1.reset();
  size_t deallocated = allocator_.deallocate_size();
  EXPECT_GT(allocated, deallocated);

  pw::WeakPtr<int> weak2(weak1);
  EXPECT_EQ(weak1.use_count(), 0);
  EXPECT_EQ(weak2.use_count(), 0);
  EXPECT_TRUE(weak1.expired());
  EXPECT_TRUE(weak2.expired());

  // Allocator should be untouched by copy-construction.
  EXPECT_EQ(allocator_.allocate_size(), allocated);
  EXPECT_EQ(allocator_.deallocate_size(), deallocated);
}

TEST_F(WeakPtrTest, CanCopyAssignWhenActive) {
  auto shared1 = allocator_.MakeShared<int>(42);
  pw::WeakPtr<int> weak;
  {
    pw::WeakPtr<int> tmp(shared1);
    weak = tmp;
  }

  EXPECT_EQ(weak.use_count(), 1);
  EXPECT_FALSE(weak.expired());

  auto shared2 = weak.Lock();
  ASSERT_NE(shared2.get(), nullptr);
  EXPECT_EQ(*shared2, 42);
}

TEST_F(WeakPtrTest, CanCopyAssignWhenExpired) {
  pw::WeakPtr<int> weak;

  // The shared pointer should delete the object when it goes out of scope, but
  // not the control block.
  size_t allocated = 0;
  {
    auto shared = allocator_.MakeShared<int>(42);
    allocated = allocator_.allocate_size();
    pw::WeakPtr<int> tmp(shared);
    weak = tmp;
  }
  size_t deallocated = allocator_.deallocate_size();
  EXPECT_GT(allocated, deallocated);

  EXPECT_EQ(weak.use_count(), 0);
  EXPECT_TRUE(weak.expired());

  // Allocator should be untouched by copy-assignment.
  EXPECT_EQ(allocator_.allocate_size(), allocated);
  EXPECT_EQ(allocator_.deallocate_size(), deallocated);
}

TEST_F(WeakPtrTest, CanMoveConstructWhenActive) {
  auto shared1 = allocator_.MakeShared<int>(42);
  pw::WeakPtr<int> weak1(shared1);
  pw::WeakPtr<int> weak2(std::move(weak1));

  EXPECT_EQ(weak2.use_count(), 1);
  EXPECT_FALSE(weak2.expired());

  auto shared2 = weak2.Lock();
  ASSERT_NE(shared2.get(), nullptr);
  EXPECT_EQ(*shared2, 42);
}

TEST_F(WeakPtrTest, CanMoveConstructWhenExpired) {
  auto shared1 = allocator_.MakeShared<int>(42);
  pw::WeakPtr<int> weak1(shared1);

  // Restting the shared pointer should delete the object, but not the control
  // block.
  size_t allocated = allocator_.allocate_size();
  shared1.reset();
  size_t deallocated = allocator_.deallocate_size();
  EXPECT_GT(allocated, deallocated);

  pw::WeakPtr<int> weak2(std::move(weak1));
  EXPECT_EQ(weak2.use_count(), 0);
  EXPECT_TRUE(weak2.expired());

  // Allocator should be untouched by move-construction.
  EXPECT_EQ(allocator_.allocate_size(), allocated);
  EXPECT_EQ(allocator_.deallocate_size(), deallocated);
}

TEST_F(WeakPtrTest, CanMoveAssignWhenActive) {
  auto shared1 = allocator_.MakeShared<int>(42);
  pw::WeakPtr<int> weak;
  {
    pw::WeakPtr<int> tmp(shared1);
    weak = std::move(tmp);
  }

  EXPECT_EQ(weak.use_count(), 1);
  EXPECT_FALSE(weak.expired());

  auto shared2 = weak.Lock();
  ASSERT_NE(shared2.get(), nullptr);
  EXPECT_EQ(*shared2, 42);
}

TEST_F(WeakPtrTest, CanMoveAssignWhenExpired) {
  pw::WeakPtr<int> weak;

  // The shared pointer should delete the object when it goes out of scope, but
  // not the control block.
  size_t allocated = 0;
  {
    auto shared = allocator_.MakeShared<int>(42);
    allocated = allocator_.allocate_size();
    pw::WeakPtr<int> tmp(shared);
    weak = std::move(tmp);
  }
  size_t deallocated = allocator_.deallocate_size();
  EXPECT_GT(allocated, deallocated);

  EXPECT_EQ(weak.use_count(), 0);
  EXPECT_TRUE(weak.expired());

  // Allocator should be untouched by move-assignment.
  EXPECT_EQ(allocator_.allocate_size(), allocated);
  EXPECT_EQ(allocator_.deallocate_size(), deallocated);
}

TEST_F(WeakPtrTest, DestructorFreesControlBlockExactlyOnce) {
  pw::WeakPtr<int> weak1;

  // The shared pointer should delete the object when it goes out of scope, but
  // not the control block.
  size_t allocated = 0;
  {
    auto shared = allocator_.MakeShared<int>(42);
    allocated = allocator_.allocate_size();
    pw::WeakPtr<int> tmp(shared);
    weak1 = std::move(tmp);
  }
  size_t deallocated = allocator_.deallocate_size();
  EXPECT_GT(allocated, deallocated);

  {
    pw::WeakPtr<int> weak2 = weak1;
    weak1.reset();

    // Allocator should be untouched; there is still one weak pointer remaining.
    EXPECT_EQ(allocator_.deallocate_size(), deallocated);
  }

  // Last weak pointer has fallen out of scope and the control block is free.
  EXPECT_NE(allocator_.deallocate_size(), deallocated);
}

TEST_F(WeakPtrTest, OwnerBeforeProvidesPartialOrder) {
  // Intentionally mix weak and shared types.
  pw::WeakPtr<int> weak1 = allocator_.MakeShared<int>(111);
  auto shared2 = allocator_.MakeShared<int>(222);
  pw::WeakPtr<int> weak2 = shared2;
  pw::WeakPtr<int> shared3 = weak2.Lock();
  pw::WeakPtr<int> weak4 = allocator_.MakeShared<int>(444);

  // Remain agnostic to allocation order.
  bool ascending = weak1.owner_before(weak2);

  // Reflexive.
  EXPECT_FALSE(weak1.owner_before(weak1));
  EXPECT_FALSE(weak2.owner_before(shared3));
  EXPECT_FALSE(shared3.owner_before(weak2));

  // Symmetric.
  EXPECT_NE(weak2.owner_before(weak1), ascending);
  EXPECT_NE(shared3.owner_before(weak1), ascending);

  // Transitive.
  EXPECT_EQ(weak1.owner_before(shared3), ascending);
  EXPECT_EQ(shared3.owner_before(weak4), ascending);
  EXPECT_EQ(weak1.owner_before(weak4), ascending);
}

TEST_F(WeakPtrTest, CanswapWhenNeitherAreExpired) {
  auto shared1 = allocator_.MakeShared<Counter>(111u);
  auto shared2 = allocator_.MakeShared<Counter>(222u);
  pw::WeakPtr<Counter> weak1(shared1);
  pw::WeakPtr<Counter> weak2(shared2);

  weak1.swap(weak2);
  EXPECT_EQ(weak1.Lock()->value(), 222u);
  EXPECT_EQ(weak2.Lock()->value(), 111u);
}

TEST_F(WeakPtrTest, CanswapWhenOneIsExpired) {
  auto shared1 = allocator_.MakeShared<Counter>(111u);
  auto shared2 = allocator_.MakeShared<Counter>(222u);
  pw::WeakPtr<Counter> weak1(shared1);
  pw::WeakPtr<Counter> weak2(shared2);
  shared2.reset();

  // weak2 is expired.
  weak1.swap(weak2);
  EXPECT_EQ(weak2.Lock()->value(), 111u);
  EXPECT_TRUE(weak1.expired());

  // weak1 is expired.
  weak1.swap(weak2);
  EXPECT_EQ(weak1.Lock()->value(), 111u);
  EXPECT_TRUE(weak2.expired());
}

TEST_F(WeakPtrTest, CanswapWhenBothAreExpired) {
  auto shared1 = allocator_.MakeShared<Counter>(111u);
  auto shared2 = allocator_.MakeShared<Counter>(222u);
  pw::WeakPtr<Counter> weak1(shared1);
  pw::WeakPtr<Counter> weak2(shared2);
  shared1.reset();
  shared2.reset();

  weak1.swap(weak2);
  EXPECT_TRUE(weak1.expired());
  EXPECT_TRUE(weak2.expired());
}

}  // namespace

// TODO(b/402489948): Remove when portable atomics are provided by `pw_atomic`.
#endif  // PW_ALLOCATOR_HAS_ATOMICS
