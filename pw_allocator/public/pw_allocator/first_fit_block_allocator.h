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

#include "pw_allocator/config.h"
#include "pw_allocator/first_fit.h"

namespace pw::allocator {

/// Alias providing the legacy name for a first fit allocator with a threshold.
template <typename OffsetType = uintptr_t>
using FirstFitBlockAllocator PW_ALLOCATOR_DEPRECATED =
    FirstFitAllocator<FirstFitBlock<uintptr_t>>;

}  // namespace pw::allocator
