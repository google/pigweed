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

#include "pw_allocator/bucket.h"

#include <new>

#include "pw_assert/check.h"

namespace pw::allocator::internal {

Bucket::Bucket(size_t chunk_size) : chunk_size_(chunk_size) {
  PW_CHECK_UINT_GE(chunk_size_, sizeof(std::byte*));
}

Bucket& Bucket::operator=(Bucket&& other) {
  chunks_ = std::exchange(other.chunks_, nullptr);
  chunk_size_ =
      std::exchange(other.chunk_size_, std::numeric_limits<size_t>::max());
  return *this;
}

void Bucket::Init(span<Bucket> buckets, size_t min_chunk_size) {
  size_t chunk_size = min_chunk_size;
  for (auto& bucket : buckets) {
    bucket = Bucket(chunk_size);
    PW_CHECK_MUL(chunk_size, 2, &chunk_size);
  }
}

size_t Bucket::count() const {
  size_t n = 0;
  Visit([&n](const std::byte*) { ++n; });
  return n;
}

void Bucket::Add(std::byte* ptr) {
  std::byte** next = std::launder(reinterpret_cast<std::byte**>(ptr));
  *next = chunks_;
  chunks_ = ptr;
}

void Bucket::Visit(const Function<void(const std::byte*)>& visitor) const {
  std::byte** next = nullptr;
  for (std::byte* chunk = chunks_; chunk != nullptr; chunk = *next) {
    visitor(chunk);
    next = std::launder(reinterpret_cast<std::byte**>(chunk));
  }
}

std::byte* Bucket::Remove() {
  return RemoveIf([](void*) { return true; });
}

std::byte* Bucket::RemoveIf(const Function<bool(void*)>& cond) {
  std::byte** prev = &chunks_;
  std::byte** next = nullptr;
  for (std::byte* chunk = chunks_; chunk != nullptr; chunk = *next) {
    next = std::launder(reinterpret_cast<std::byte**>(chunk));
    if (cond(chunk)) {
      *prev = *next;
      *next = nullptr;
      return chunk;
    }
    prev = next;
  }
  return nullptr;
}

}  // namespace pw::allocator::internal
