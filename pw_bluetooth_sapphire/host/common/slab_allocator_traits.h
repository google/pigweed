// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SLAB_ALLOCATOR_TRAITS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SLAB_ALLOCATOR_TRAITS_H_

#include <memory>

#include <fbl/slab_allocator.h>

namespace bt {
namespace common {

namespace internal {
constexpr size_t kSlabOverhead = 16;
}  // namespace internal

// SlabAllocatorTraits is a simple alias over fbl::StaticSlabAllocatorTraits
// which enforces the use of std::unique_ptr.
template <typename T, size_t ObjectSize, size_t NumBuffers>
using SlabAllocatorTraits =
    fbl::StaticSlabAllocatorTraits<std::unique_ptr<T>,
                                   ObjectSize * NumBuffers +
                                       internal::kSlabOverhead>;

}  // namespace common
}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SLAB_ALLOCATOR_TRAITS_H_
