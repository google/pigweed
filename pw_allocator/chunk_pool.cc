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

#include "pw_allocator/chunk_pool.h"

#include "pw_allocator/buffer.h"
#include "pw_assert/check.h"

namespace pw::allocator {

static Layout EnsurePointerLayout(const Layout& layout) {
  return Layout(std::max(layout.size(), sizeof(void*)),
                std::max(layout.alignment(), alignof(void*)));
}

ChunkPool::ChunkPool(ByteSpan region, const Layout& layout)
    : Pool(kCapabilities, layout),
      allocated_layout_(EnsurePointerLayout(layout)) {
  Result<ByteSpan> result =
      GetAlignedSubspan(region, allocated_layout_.alignment());
  PW_CHECK_OK(result.status());
  start_ = reinterpret_cast<uintptr_t>(region.data());
  end_ = start_ + region.size();
  region = result.value();
  next_ = region.data();
  std::byte* current = next_;
  std::byte* end = current + region.size();
  std::byte** next = &current;
  while (current < end) {
    next = std::launder(reinterpret_cast<std::byte**>(current));
    current += allocated_layout_.size();
    *next = current;
  }
  *next = nullptr;
}

void* ChunkPool::DoAllocate() {
  if (next_ == nullptr) {
    return nullptr;
  }
  std::byte* ptr = next_;
  next_ = *(std::launder(reinterpret_cast<std::byte**>(next_)));
  return ptr;
}

void ChunkPool::DoDeallocate(void* ptr) {
  if (ptr == nullptr) {
    return;
  }
  std::byte** next = std::launder(reinterpret_cast<std::byte**>(ptr));
  *next = next_;
  next_ = reinterpret_cast<std::byte*>(ptr);
}

Status ChunkPool::DoQuery(const void* ptr) const {
  auto addr = reinterpret_cast<uintptr_t>(ptr);
  if (addr < start_ || end_ <= addr) {
    return Status::OutOfRange();
  }
  if ((addr - start_) % allocated_layout_.size() != 0) {
    return Status::OutOfRange();
  }
  return OkStatus();
}

}  // namespace pw::allocator
