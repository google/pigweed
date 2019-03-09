// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "slab_allocator.h"

#include <memory>

#include "byte_buffer.h"
#include "slab_buffer.h"

namespace btlib {
namespace common {

using SmallBufferTraits =
    SlabBufferTraits<kSmallBufferSize, kSlabSize / kSmallBufferSize>;
using LargeBufferTraits =
    SlabBufferTraits<kLargeBufferSize, kSlabSize / kLargeBufferSize>;

using SmallAllocator = fbl::SlabAllocator<SmallBufferTraits>;
using LargeAllocator = fbl::SlabAllocator<LargeBufferTraits>;

common::MutableByteBufferPtr NewSlabBuffer(size_t size) {
  if (size == 0)
    return std::make_unique<common::DynamicByteBuffer>();
  if (size <= kSmallBufferSize) {
    auto buffer = SmallAllocator::New(size);
    if (buffer)
      return buffer;
  }

  return LargeAllocator::New(size);
}

}  // namespace common
}  // namespace btlib

DECLARE_STATIC_SLAB_ALLOCATOR_STORAGE(::btlib::common::LargeBufferTraits,
                                      ::btlib::common::kMaxNumSlabs,
                                      true);
DECLARE_STATIC_SLAB_ALLOCATOR_STORAGE(::btlib::common::SmallBufferTraits,
                                      ::btlib::common::kMaxNumSlabs,
                                      true);
