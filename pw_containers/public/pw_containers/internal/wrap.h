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

namespace pw::containers::internal {

/// Increments `index` by `count`, modulo `capacity`.
template <typename T>
constexpr void IncrementWithWrap(T& index, T count, T capacity) {
  index += count;
  // Note: branch is faster than mod (%) on common embedded architectures.
  if (index >= capacity) {
    index -= capacity;
  }
}

/// Decrements `index` by `count`, modulo `capacity`.
template <typename T>
constexpr void DecrementWithWrap(T& index, T count, T capacity) {
  if (index < count) {
    index += capacity;
  }
  index -= count;
}

}  // namespace pw::containers::internal
