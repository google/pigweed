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

#include "pw_allocator/split_free_list_allocator.h"

#include "pw_assert/check.h"

namespace pw::allocator {

void BaseSplitFreeListAllocator::CrashOnAllocated(void* allocated) {
  PW_DCHECK(false,
            "The block at %p was still in use when its allocator was "
            "destroyed. All memory allocated by an allocator must be released "
            "before the allocator goes out of scope.",
            allocated);
}

}  // namespace pw::allocator
