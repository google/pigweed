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

#include "pw_allocator/libc_allocator.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "pw_assert/check.h"
#include "pw_bytes/alignment.h"

namespace pw::allocator {

void* LibCAllocator::DoAllocate(size_t size, size_t alignment) {
  // TODO: b/301930507 - `aligned_alloc` is not portable. Return null for larger
  // allocations for now.
  return alignment <= alignof(std::max_align_t) ? std::malloc(size) : nullptr;
}

void LibCAllocator::DoDeallocate(void* ptr, size_t, size_t) { std::free(ptr); }

bool LibCAllocator::DoResize(void*, size_t old_size, size_t, size_t new_size) {
  // `realloc` may move memory, even when shrinking. Only return true if no
  // change is needed.
  return old_size == new_size;
}

void* LibCAllocator::DoReallocate(void* ptr,
                                  size_t,
                                  size_t old_alignment,
                                  size_t new_size) {
  // TODO: b/301930507 - `aligned_alloc` is not portable. Return null for larger
  // allocations for now.
  return old_alignment <= alignof(std::max_align_t)
             ? std::realloc(ptr, new_size)
             : nullptr;
}

}  // namespace pw::allocator
