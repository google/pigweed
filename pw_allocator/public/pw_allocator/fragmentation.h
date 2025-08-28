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

#include <cstddef>

namespace pw::allocator {

/// @submodule{pw_allocator,util}

// inclusive-language: disable

/// Information that can be used to represent the fragmentation of the block
/// allocator's memory region.
///
/// Fragmentation can be measured as 1 minus the normalized root-sum-square of
/// the free block inner sizes, i.e.
///
///   1 - (sqrt(sum(U[i]^2)) / sum(U[i])
///
/// where `U[i]` is the inner size of the i-th free block.
///
/// This metric has been described by Adam Sawicki
/// (https://asawicki.info/news_1757_a_metric_for_memory_fragmentation),
/// and used in esp8266/Arduino
/// (https://github.com/esp8266/Arduino/blob/master/cores/esp8266/Esp-frag.cpp),
/// among others.
///
/// This struct provides the sum-of-squares and the sum of the inner sizes of
/// free blocks. It is left to the caller to perform the square root and
/// division steps, perhaps on a host, AP, or other device with better floating
/// point support.
///
/// The sum-of-squares is stored as a pair of sizes, since it can overflow.
struct Fragmentation {
  /// Sum-of-squares of the inner sizes of free blocks.
  struct {
    size_t hi = 0;
    size_t lo = 0;
  } sum_of_squares;

  /// Sum of the inner sizes of free blocks.
  size_t sum = 0;

  void AddFragment(size_t inner_size);
};

// inclusive-language: enable

/// Perform the final steps of calculating the fragmentation metric.
///
/// This step includes manipulating floating point numbers, and as such it may
/// be desirable to avoid performing this step on device.
float CalculateFragmentation(const Fragmentation& fragmentation);

/// @}

}  // namespace pw::allocator
