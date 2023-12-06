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

#include "pw_allocator/simple_allocator.h"

#include "pw_allocator/allocator_testing.h"
#include "pw_unit_test/framework.h"

namespace pw::allocator {

// Size of the memory region to use in the tests below.
constexpr size_t kCapacity = 256;

// An `SimpleAllocator` that is automatically initialized on construction.
class SimpleAllocatorWithBuffer
    : public test::WithBuffer<SimpleAllocator, kCapacity> {
 public:
  SimpleAllocatorWithBuffer() {
    EXPECT_EQ((*this)->Init(ByteSpan(this->data(), this->size())), OkStatus());
  }
};

// This is not meant to be a rigorous unit test of individual behaviors, as much
// as simply a way to demonstrate and exercise the simple allocator.
TEST(SimpleAllocatorTest, AllocateResizeDeallocate) {
  SimpleAllocatorWithBuffer allocator;

  // Can allocate usable memory.
  constexpr size_t kSize1 = kCapacity / 4;
  constexpr Layout layout1 = Layout::Of<std::byte[kSize1]>();
  auto* ptr = static_cast<std::byte*>(allocator->Allocate(layout1));
  ASSERT_NE(ptr, nullptr);
  memset(ptr, 0x5A, kSize1);

  // Can shrink memory. Contents are preserved.
  constexpr size_t kSize2 = kCapacity / 8;
  constexpr Layout layout2 = Layout::Of<std::byte[kSize2]>();
  EXPECT_TRUE(allocator->Resize(ptr, layout1, layout2.size()));
  for (size_t i = 0; i < kSize2; ++i) {
    EXPECT_EQ(size_t(ptr[i]), 0x5Au);
  }

  // Can grow memory. Contents are preserved.
  constexpr size_t kSize3 = kCapacity / 2;
  constexpr Layout layout3 = Layout::Of<std::byte[kSize3]>();
  EXPECT_TRUE(allocator->Resize(ptr, layout2, layout3.size()));
  for (size_t i = 0; i < kSize2; ++i) {
    EXPECT_EQ(size_t(ptr[i]), 0x5Au);
  }

  // Can free memory.
  allocator->Deallocate(ptr, layout3);
}

}  // namespace pw::allocator
