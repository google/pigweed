// Copyright 2020 The Pigweed Authors
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

#include "pw_allocator/bucket_allocator.h"
#include "pw_assert/assert.h"
#include "pw_bytes/span.h"
#include "pw_preprocessor/compiler.h"

namespace pw::allocator {

/// Legacy interface to BucketBlockAllocator.
///
/// This interface is deprecated, and is only maintained for compatibility
/// reasons. New projects should use ``BucketBlockAllocator``.
template <size_t kNumBuckets = 6>
class FreeListHeapBuffer {
 public:
  explicit FreeListHeapBuffer(ByteSpan region) : allocator_(region) {}

  void* Allocate(size_t size) { return allocator_.Allocate(Layout(size)); }

  void Free(void* ptr) { allocator_.Deallocate(ptr); }

  void* Realloc(void* ptr, size_t size) {
    return allocator_.Reallocate(ptr, Layout(size));
  }

  void* Calloc(size_t num, size_t size) {
    PW_ASSERT(!PW_MUL_OVERFLOW(num, size, &size));
    void* ptr = allocator_.Allocate(Layout(size));
    if (ptr == nullptr) {
      return nullptr;
    }
    std::memset(ptr, 0, size);
    return ptr;
  }

 private:
  static constexpr size_t kMinChunkSize = 16;
  BucketAllocator<kMinChunkSize, kNumBuckets> allocator_;
};

}  // namespace pw::allocator
