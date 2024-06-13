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

#include "pw_assert/check.h"

namespace pw::allocator::internal {

Bucket::Bucket() { Init(); }

Bucket::Bucket(size_t chunk_size) { Init(chunk_size); }

Bucket& Bucket::operator=(Bucket&& other) {
  Init(other.chunk_size_);
  if (!other.empty()) {
    sentinel_.next = other.sentinel_.next;
    sentinel_.prev = other.sentinel_.prev;
  }
  other.Init();
  return *this;
}

void Bucket::Init(size_t chunk_size) {
  PW_CHECK_UINT_GE(chunk_size, sizeof(Chunk));
  chunk_size_ = chunk_size;
  sentinel_.next = &sentinel_;
  sentinel_.prev = &sentinel_;
}

void Bucket::Init(span<Bucket> buckets, size_t min_chunk_size) {
  size_t chunk_size = min_chunk_size;
  for (auto& bucket : buckets) {
    bucket.Init(chunk_size);
    PW_CHECK_MUL(chunk_size, 2, &chunk_size);
  }
}

size_t Bucket::count() const {
  size_t n = 0;
  Visit([&n](const std::byte*) { ++n; });
  return n;
}

void Bucket::Add(std::byte* ptr) {
  auto* chunk = Chunk::FromBytes(ptr);
  chunk->prev = &sentinel_;
  chunk->next = sentinel_.next;
  sentinel_.next->prev = chunk;
  sentinel_.next = chunk;
}

void Bucket::Visit(const Function<void(const std::byte*)>& visitor) const {
  for (const Chunk* chunk = sentinel_.next; chunk != &sentinel_;
       chunk = chunk->next) {
    visitor(chunk->AsBytes());
  }
}

std::byte* Bucket::Remove() {
  return empty() ? nullptr : Remove(sentinel_.next);
}

std::byte* Bucket::RemoveIf(const Function<bool(const std::byte*)>& cond) {
  for (Chunk* chunk = sentinel_.next; chunk != &sentinel_;
       chunk = chunk->next) {
    if (cond(chunk->AsBytes())) {
      return Remove(chunk);
    }
  }
  return nullptr;
}

std::byte* Bucket::Remove(std::byte* ptr) {
  return Remove(Chunk::FromBytes(ptr));
}

std::byte* Bucket::Remove(Chunk* chunk) {
  chunk->prev->next = chunk->next;
  chunk->next->prev = chunk->prev;
  chunk->next = chunk;
  chunk->prev = chunk;
  return chunk->AsBytes();
}

}  // namespace pw::allocator::internal
