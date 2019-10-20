// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SLAB_ALLOCATOR_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SLAB_ALLOCATOR_H_

#include "src/connectivity/bluetooth/core/bt-host/common/byte_buffer.h"

namespace bt {

// NOTE: Tweak these as needed.
constexpr size_t kSmallBufferSize = 64;
constexpr size_t kLargeBufferSize = 2048;

constexpr size_t kMaxNumSlabs = 100;
constexpr size_t kSlabSize = 32767;

// Returns a slab-allocated byte buffer with |size| bytes of capacity. The underlying allocation
// occupies |kSmallBufferSize| or |kLargeBufferSize| bytes of memory, unless |size| is 0, which
// returns a zero-sized byte buffer with no underlying slab allocation. Returns nullptr for failures
// to allocate, including if |size| exceeds |kLargeBufferSize|.
[[nodiscard]] MutableByteBufferPtr NewSlabBuffer(size_t size);

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SLAB_ALLOCATOR_H_
