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

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "pw_allocator/allocator.h"
#include "pw_allocator/block/detailed_block.h"
#include "pw_allocator/bucket/fast_sorted.h"
#include "pw_bytes/span.h"

namespace pw::allocator::size_report {

/// Default block type to use for tests.
using BlockType = DetailedBlock<uint32_t, GenericFastSortedItem>;

/// Type used for exercising an allocator.
struct Foo final {
  std::array<std::byte, 16> buffer;
};

/// Type used for exercising an allocator.
struct Bar {
  Foo foo;
  size_t number;

  Bar(size_t number_) : number(number_) {
    std::memset(foo.buffer.data(), 0, foo.buffer.size());
  }
};

/// Type used for exercising an allocator.
struct Baz {
  Foo foo;
  uint16_t id;
};

/// Returns a view of a statically allocated array of bytes.
ByteSpan GetBuffer();

/// Measures the size of common functions and data without any allocators.
///
/// @param[in]  mask        A bitmap that can be passed to `PW_BLOAT_COND` and
///                         `PW_BLOAT_EXPR`. See those macros for details.
int SetBaseline(uint32_t mask);

/// Exercises an allocator as part of a size report.
///
/// @param[in]  allocator   The allocator to exercise.
/// @param[in]  mask        A bitmap that can be passed to `PW_BLOAT_COND` and
///                         `PW_BLOAT_EXPR`. See those macros for details.
int MeasureAllocator(Allocator& allocator, uint32_t mask);

}  // namespace pw::allocator::size_report
