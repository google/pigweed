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
#pragma once

#include "pw_allocator/block_allocator.h"

namespace pw::allocator {

/// Alias for `DualFirstFitBlockAllocator`.
///
/// Previously this was a distinct class, but has been replaced by
/// `BlockAllocator` using the dual-first-fit allocation strategy. This type is
/// deprecated and will eventually be removed. Consumers should migrate to
/// `DualFirstFitBlockAllocator`.
using SplitFreeListAllocator = DualFirstFitBlockAllocator<>;

}  // namespace pw::allocator
