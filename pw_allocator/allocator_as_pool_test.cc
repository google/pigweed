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

#include "pw_allocator/allocator_as_pool.h"

#include <cstdint>

#include "pw_allocator/testing.h"
#include "pw_unit_test/framework.h"

namespace pw::allocator {
namespace {

struct U64 {
  std::byte bytes[8];
};

TEST(AllocatorAsPoolTest, Capabilities) {
  test::AllocatorForTest<256> allocator;
  AllocatorAsPool pool(allocator, Layout::Of<U64>());
  EXPECT_EQ(pool.capabilities(), allocator.capabilities());
}

TEST(AllocatorAsPoolTest, AllocateDeallocate) {
  test::AllocatorForTest<256> allocator;
  AllocatorAsPool pool(allocator, Layout::Of<U64>());

  void* ptr = pool.Allocate();
  ASSERT_NE(ptr, nullptr);
  pool.Deallocate(ptr);
}

}  // namespace
}  // namespace pw::allocator
