// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/connectivity/bluetooth/core/bt-host/common/slab_allocator_traits.h"

#include "gtest/gtest.h"

namespace bt {
namespace common {

namespace {

constexpr size_t kBufferSize = 256;
constexpr size_t kNumBuffers = 2;

}  // namespace

// The static slab allocator needs to be declared in the global namespace and
// cannot refer to a traits type that's declared in an anonymous namespace. So
// we use the test namespace here.
namespace test {

class TestBuffer;

using TestTraits = SlabAllocatorTraits<TestBuffer, kBufferSize, kNumBuffers>;

class TestBuffer : public fbl::SlabAllocated<TestTraits> {
 public:
  char data[kBufferSize];
};

}  // namespace test

namespace {

using TestAllocator = fbl::SlabAllocator<test::TestTraits>;

// This tests that allocation/deallocation via std::unique_ptr works as
// expected.
TEST(SlabAllocatedBufferTest, Basic) {
  // Buffer has only one slab. This should return nullptr after two allocations.
  std::unique_ptr<test::TestBuffer> buffer0 = TestAllocator::New();
  std::unique_ptr<test::TestBuffer> buffer1 = TestAllocator::New();
  std::unique_ptr<test::TestBuffer> buffer2 = TestAllocator::New();

  EXPECT_TRUE(buffer0);
  EXPECT_TRUE(buffer1);
  EXPECT_FALSE(buffer2);

  // Free one of the allocated buffers and request a new one. This should return
  // the memory back to the allocator and the next allocation should succeed.
  buffer1 = nullptr;
  buffer2 = TestAllocator::New();
  EXPECT_TRUE(buffer2);
}

}  // namespace
}  // namespace common
}  // namespace bt

// Creates no more than one slab.
DECLARE_STATIC_SLAB_ALLOCATOR_STORAGE(bt::common::test::TestTraits, 1, true);
