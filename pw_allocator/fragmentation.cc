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

#include "pw_allocator/fragmentation.h"

namespace pw::allocator {
namespace {

/// Adds the second number to the first, and return whether it overflowed.
size_t AddTo(size_t& accumulate, size_t value) {
  accumulate += value;
  return accumulate < value ? 1 : 0;
}

}  // namespace

void Fragmentation::AddFragment(size_t size) {
  constexpr size_t kShift = sizeof(size_t) * 4;
  constexpr size_t kMask = (size_t(1) << kShift) - 1;
  size_t hi = size >> kShift;
  size_t lo = size & kMask;
  size_t crossterm = hi * lo;
  hi = hi * hi;
  lo = lo * lo;
  hi += (AddTo(crossterm, crossterm)) << kShift;
  hi += (crossterm >> kShift) + AddTo(lo, (crossterm & kMask) << kShift);
  hi += AddTo(sum_of_squares.lo, lo);
  sum_of_squares.hi += hi;
  sum += size;
}

}  // namespace pw::allocator
