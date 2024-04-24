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

#include "pw_allocator/buffer.h"

#include "pw_assert/check.h"
#include "pw_bytes/alignment.h"

namespace pw::allocator {

Result<ByteSpan> GetAlignedSubspan(ByteSpan bytes, size_t alignment) {
  if (bytes.data() == nullptr) {
    return Status::ResourceExhausted();
  }
  auto unaligned_start = reinterpret_cast<uintptr_t>(bytes.data());
  auto aligned_start = AlignUp(unaligned_start, alignment);
  auto unaligned_end = unaligned_start + bytes.size();
  auto aligned_end = AlignDown(unaligned_end, alignment);
  if (aligned_end <= aligned_start) {
    return Status::ResourceExhausted();
  }
  return bytes.subspan(aligned_start - unaligned_start,
                       aligned_end - aligned_start);
}

bool IsWithin(const void* ptr, size_t size, ConstByteSpan outer) {
  auto outer_start = reinterpret_cast<uintptr_t>(outer.data());
  auto start = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t end;
  PW_CHECK_ADD(start, size, &end);
  return outer_start <= start && end <= (outer_start + outer.size());
}

}  // namespace pw::allocator
