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
#pragma once

#include <cstddef>
#include <cstdint>

#include "pw_allocator/block/small_block_base.h"

namespace pw::allocator {

/// A block implementation with only 4 bytes of overhead.
///
/// Like its base class, this block is allocatable with a fixed alignment. This
/// class's code size is slightly larger than that of `SmallBlock`, and it can
/// address only 2^18 - 4 bytes of memory.
class TinyBlock : public SmallBlockBase<TinyBlock, uint16_t, 2> {
 private:
  friend SmallBlockBase<TinyBlock, uint16_t, 2>;
  constexpr explicit TinyBlock(size_t outer_size)
      : SmallBlockBase(outer_size) {}
};

}  // namespace pw::allocator
