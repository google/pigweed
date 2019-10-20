// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "slab_allocator.h"

#include <memory>

#include "byte_buffer.h"
#include "slab_buffer.h"

namespace bt {

static_assert(kLargeBufferSize > kSmallBufferSize);

using SmallBufferTraits = SlabBufferTraits<kSmallBufferSize, kSlabSize / kSmallBufferSize>;
using LargeBufferTraits = SlabBufferTraits<kLargeBufferSize, kSlabSize / kLargeBufferSize>;

using SmallAllocator = fbl::SlabAllocator<SmallBufferTraits>;
using LargeAllocator = fbl::SlabAllocator<LargeBufferTraits>;

MutableByteBufferPtr NewSlabBuffer(size_t size) {
  if (size == 0) {
    // Don't return nullptr because that would indicate an error.
    return std::make_unique<DynamicByteBuffer>();
  }

  if (size <= kSmallBufferSize) {
    auto buffer = SmallAllocator::New(size);
    if (buffer) {
      return buffer;
    }
  }

  if (size > kLargeBufferSize) {
    return nullptr;
  }

  return LargeAllocator::New(size);
}

}  // namespace bt

DECLARE_STATIC_SLAB_ALLOCATOR_STORAGE(bt::LargeBufferTraits, bt::kMaxNumSlabs, true);
DECLARE_STATIC_SLAB_ALLOCATOR_STORAGE(bt::SmallBufferTraits, bt::kMaxNumSlabs, true);
