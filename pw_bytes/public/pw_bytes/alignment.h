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
#pragma once

#include <cstddef>

namespace pw {

// Returns the value rounded down to the nearest multiple of alignment.
constexpr size_t AlignDown(size_t value, size_t alignment) {
  return (value / alignment) * alignment;
}

// Returns the value rounded up to the nearest multiple of alignment.
constexpr size_t AlignUp(size_t value, size_t alignment) {
  return (value + alignment - 1) / alignment * alignment;
}

// Returns the number of padding bytes required to align the provided length.
constexpr size_t Padding(size_t length, size_t alignment) {
  return AlignUp(length, alignment) - length;
}

}  // namespace pw
