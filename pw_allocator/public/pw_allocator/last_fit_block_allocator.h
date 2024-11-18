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
#pragma once

#include "pw_allocator/config.h"
#include "pw_allocator/first_fit.h"

namespace pw::allocator {

/// Alias for a default block type that is compatible with
/// `LastFitBlockAllocator`.
template <typename OffsetType>
using LastFitBlock = FirstFitBlock<OffsetType>;

/// Legacy last fit allocator.
///
/// New usages should prefer to use `FirstFitAllocator` directly.
///
/// This allocator sets the base type's threshold to value to the maximum value,
/// ensuring that all allocations come from the end of the region.
template <typename OffsetType = uintptr_t>
class PW_ALLOCATOR_DEPRECATED LastFitBlockAllocator
    : public FirstFitAllocator<LastFitBlock<OffsetType>> {
 public:
  using BlockType = LastFitBlock<OffsetType>;

 private:
  using Base = FirstFitAllocator<BlockType>;

 public:
  constexpr LastFitBlockAllocator() {
    Base::set_threshold(std::numeric_limits<size_t>::max());
  }

  explicit LastFitBlockAllocator(ByteSpan region)
      : Base(region, std::numeric_limits<size_t>::max()) {}
};

}  // namespace pw::allocator
