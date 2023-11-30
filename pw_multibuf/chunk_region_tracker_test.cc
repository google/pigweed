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

#include "pw_multibuf/chunk_region_tracker.h"

#include <cstddef>
#include <optional>

#include "gtest/gtest.h"
#include "pw_allocator/simple_allocator.h"
#include "pw_multibuf/chunk.h"
#include "pw_status/status.h"

namespace pw::multibuf {
namespace {

const size_t kArbitraryAllocatorSize = 1024;
const size_t kArbitraryChunkSize = 32;

TEST(HeaderChunkRegionTracker, AllocatesRegionAsChunk) {
  allocator::SimpleAllocator alloc{};
  std::array<std::byte, kArbitraryAllocatorSize> alloc_storage{};
  ASSERT_EQ(alloc.Init(alloc_storage), OkStatus());
  std::optional<OwnedChunk> chunk =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryChunkSize);
  ASSERT_TRUE(chunk.has_value());
}

TEST(HeaderChunkRegionTracker, AllocatedRegionAsChunkTooLarge) {
  allocator::SimpleAllocator alloc{};
  std::array<std::byte, kArbitraryAllocatorSize> alloc_storage{};
  ASSERT_EQ(alloc.Init(alloc_storage), OkStatus());
  std::optional<OwnedChunk> chunk =
      HeaderChunkRegionTracker::AllocateRegionAsChunk(&alloc,
                                                      kArbitraryAllocatorSize);
  ASSERT_FALSE(chunk.has_value());
}

TEST(HeaderChunkRegionTracker, AllocatesRegion) {
  allocator::SimpleAllocator alloc{};
  std::array<std::byte, kArbitraryAllocatorSize> alloc_storage{};
  ASSERT_EQ(alloc.Init(alloc_storage), OkStatus());
  auto tracker =
      HeaderChunkRegionTracker::AllocateRegion(&alloc, kArbitraryChunkSize);
  ASSERT_NE(tracker, nullptr);
}

TEST(HeaderChunkRegionTracker, AllocatedRegionTooLarge) {
  allocator::SimpleAllocator alloc{};
  std::array<std::byte, kArbitraryAllocatorSize> alloc_storage{};
  ASSERT_EQ(alloc.Init(alloc_storage), OkStatus());
  auto tracker =
      HeaderChunkRegionTracker::AllocateRegion(&alloc, kArbitraryAllocatorSize);
  ASSERT_EQ(tracker, nullptr);
}

}  // namespace
}  // namespace pw::multibuf
