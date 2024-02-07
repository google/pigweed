// Copyright 2020 The Pigweed Authors
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

#include "pw_allocator/block.h"

#include "pw_assert/check.h"

namespace pw::allocator {
namespace internal {

// TODO: b/234875269 - Add stack tracing to locate which call to the heap
// operation caused the corruption in the methods below.

void CrashMisaligned(uintptr_t addr) {
  PW_DCHECK(false,
            "The block at address %p is not aligned.",
            reinterpret_cast<void*>(addr));
}

void CrashNextMismatched(uintptr_t addr, uintptr_t next_prev) {
  PW_DCHECK(false,
            "The 'prev' field in the next block (%p) does not match the "
            "address of the current block (%p).",
            reinterpret_cast<void*>(next_prev),
            reinterpret_cast<void*>(addr));
}

void CrashPrevMismatched(uintptr_t addr, uintptr_t prev_next) {
  PW_DCHECK(false,
            "The 'next' field in the previous block (%p) does not match "
            "the address of the current block (%p).",
            reinterpret_cast<void*>(prev_next),
            reinterpret_cast<void*>(addr));
}

void CrashPoisonCorrupted(uintptr_t addr) {
  PW_DCHECK(false,
            "The poisoned pattern in the block at %p is corrupted.",
            reinterpret_cast<void*>(addr));
}

}  // namespace internal
}  // namespace pw::allocator
