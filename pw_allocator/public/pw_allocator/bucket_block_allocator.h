// Copyright 2024 The Pigweed Authors
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
#include <cstdint>

#include "pw_allocator/bucket_allocator.h"
#include "pw_allocator/config.h"

namespace pw::allocator {

/// Legacy alias for `BucketAllocator`.
///
/// Consumers should migrate away from using this type and use `BucketAllocator`
/// instead.
///
/// The first template parameter was formerly an `OffsetType` that was passed to
/// the `BlockAllocator`. Since the free blocks' usable space is interpreted as
/// intrusive items, they must have a pointer-compatible alignment. As a result,
/// the first template parameter is ignored.
template <typename OffsetType = uintptr_t,
          size_t kMinBucketChunkSize = 32,
          size_t kNumBuckets = 5>
using BucketBlockAllocator PW_ALLOCATOR_DEPRECATED =
    BucketAllocator<BucketBlock<OffsetType>, kMinBucketChunkSize, kNumBuckets>;

}  // namespace pw::allocator
