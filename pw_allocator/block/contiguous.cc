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

#include "pw_allocator/block/contiguous.h"

#include "pw_assert/check.h"

namespace pw::allocator::internal {

// TODO: b/234875269 - Add stack tracing to locate which call to the heap
// operation caused the corruption in the methods below.

void CrashNextMisaligned(uintptr_t addr, uintptr_t next) {
  PW_CRASH(
      "A block (%p) is corrupted: it has a 'next' field (%p) that is not "
      "properly aligned.",
      cpp20::bit_cast<void*>(addr),
      cpp20::bit_cast<void*>(next));
}

void CrashNextPrevMismatched(uintptr_t addr,
                             uintptr_t next,
                             uintptr_t next_prev) {
  PW_CRASH(
      "A block (%p) is corrupted: its 'next' field (%p) has a 'prev' field "
      "(%p) that does not match the block.",
      cpp20::bit_cast<void*>(addr),
      cpp20::bit_cast<void*>(next),
      cpp20::bit_cast<void*>(next_prev));
}

void CrashPrevMisaligned(uintptr_t addr, uintptr_t prev) {
  PW_CRASH(
      "A block (%p) is corrupted: it has a 'prev' field (%p) that is not "
      "properly aligned.",
      cpp20::bit_cast<void*>(addr),
      cpp20::bit_cast<void*>(prev));
}

void CrashPrevNextMismatched(uintptr_t addr,
                             uintptr_t prev,
                             uintptr_t prev_next) {
  PW_CRASH(
      "A block (%p) is corrupted: its 'prev' field (%p) has a 'next' field "
      "(%p) that does not match the block.",
      cpp20::bit_cast<void*>(addr),
      cpp20::bit_cast<void*>(prev),
      cpp20::bit_cast<void*>(prev_next));
}

}  // namespace pw::allocator::internal
