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

#ifndef SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SLAB_ALLOCATOR_H_
#define SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SLAB_ALLOCATOR_H_

#include "pw_bluetooth_sapphire/internal/host/common/byte_buffer.h"

namespace bt {

// NOTE: Tweak these as needed.
constexpr size_t kSmallBufferSize = 64;
constexpr size_t kLargeBufferSize = 2048;

constexpr size_t kMaxNumSlabs = 100;
constexpr size_t kSlabSize = 32767;

// Returns a slab-allocated byte buffer with |size| bytes of capacity. The
// underlying allocation occupies |kSmallBufferSize| or |kLargeBufferSize| bytes
// of memory, unless:
//  * |size| is 0, which returns a zero-sized byte buffer with no underlying
//  slab allocation.
//  * |size| exceeds |kLargeBufferSize|, which falls back to the system
//  allocator.
//    NOTE: In this case, if allocation fails, panic.
//
// Returns nullptr for failures to allocate.
[[nodiscard]] MutableByteBufferPtr NewBuffer(size_t size);

}  // namespace bt

#endif  // SRC_CONNECTIVITY_BLUETOOTH_CORE_BT_HOST_COMMON_SLAB_ALLOCATOR_H_
