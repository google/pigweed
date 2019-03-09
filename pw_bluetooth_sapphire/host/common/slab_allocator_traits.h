// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SLAB_ALLOCATOR_TRAITS_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SLAB_ALLOCATOR_TRAITS_H_

#include <memory>

#include <fbl/slab_allocator.h>

namespace btlib {
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
}  // namespace btlib

namespace fbl {
namespace internal {

// SlabAllocatorPtrTraits specialization for std::unique_ptr.
template <typename T>
struct SlabAllocatorPtrTraits<std::unique_ptr<T>> {
  using ObjType = T;
  using PtrType = std::unique_ptr<T>;

  static constexpr bool IsManaged = true;
  static constexpr PtrType CreatePtr(ObjType* ptr) { return PtrType(ptr); }
};

}  // namespace internal
}  // namespace fbl

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SLAB_ALLOCATOR_TRAITS_H_
