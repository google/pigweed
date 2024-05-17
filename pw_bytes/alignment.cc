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

#include "pw_bytes/alignment.h"

#include <cstdint>

namespace pw {

ByteSpan GetAlignedSubspan(ByteSpan bytes, size_t alignment) {
  if (bytes.data() == nullptr) {
    return ByteSpan();
  }
  auto unaligned_start = reinterpret_cast<uintptr_t>(bytes.data());
  auto aligned_start = AlignUp(unaligned_start, alignment);
  auto unaligned_end = unaligned_start + bytes.size();
  auto aligned_end = AlignDown(unaligned_end, alignment);
  if (aligned_end <= aligned_start) {
    return ByteSpan();
  }
  return bytes.subspan(aligned_start - unaligned_start,
                       aligned_end - aligned_start);
}

}  // namespace pw
