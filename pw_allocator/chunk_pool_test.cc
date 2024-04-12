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

#include "pw_allocator/chunk_pool.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include "pw_allocator/testing.h"
#include "pw_unit_test/framework.h"

namespace pw::allocator {
namespace {

struct U64 {
  std::byte bytes[8];
};

// ChunkPool unit tests.

TEST(ChunkPoolTest, Capabilities) {
  std::array<std::byte, 256> buffer;
  ChunkPool pool(buffer, Layout::Of<U64>());
  EXPECT_EQ(pool.capabilities(), ChunkPool::kCapabilities);
}

TEST(ChunkPoolTest, AllocateDeallocate) {
  std::array<std::byte, 256> buffer;
  ChunkPool pool(buffer, Layout::Of<U64>());

  void* ptr = pool.Allocate();
  ASSERT_NE(ptr, nullptr);
  pool.Deallocate(ptr);
}

TEST(ChunkPoolTest, ExhaustTwice) {
  constexpr size_t kNumU64s = 32;
  constexpr size_t kBufferSize = sizeof(U64) * kNumU64s;
  std::array<std::byte, kBufferSize> buffer;
  ChunkPool pool(buffer, Layout::Of<U64>());

  // Allocate everything.
  std::array<void*, kNumU64s> ptrs;
  for (auto& ptr : ptrs) {
    ptr = pool.Allocate();
    ASSERT_NE(ptr, nullptr);
  }

  // At this point, the pool is empty.
  EXPECT_EQ(pool.Allocate(), nullptr);

  // Now refill the pool, and show it can be emptied again.
  for (auto& ptr : ptrs) {
    pool.Deallocate(ptr);
    ptr = nullptr;
  }
  for (auto& ptr : ptrs) {
    ptr = pool.Allocate();
    ASSERT_NE(ptr, nullptr);
  }

  // Release everything.
  for (auto& ptr : ptrs) {
    pool.Deallocate(ptr);
    ptr = nullptr;
  }
}

}  // namespace
}  // namespace pw::allocator
