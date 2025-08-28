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

/// @submodule{pw_allocator,block_impl}

/// A compact block implementation.
///
/// Like its base class, this block is allocatable with a fixed alignment. It is
/// fairly compact in terms of code size, and each block uses 8 bytes of
/// overhead. It can address up to 2^32 bytes of memory.
class SmallBlock : public SmallBlockBase<SmallBlock, uint32_t, 0> {
 private:
  friend SmallBlockBase<SmallBlock, uint32_t, 0>;
  constexpr explicit SmallBlock(size_t outer_size)
      : SmallBlockBase(outer_size) {}
};

/// @}

}  // namespace pw::allocator
