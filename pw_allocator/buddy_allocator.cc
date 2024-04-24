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

#include "pw_allocator/buddy_allocator.h"

#include <cstring>

#include "pw_allocator/buffer.h"
#include "pw_assert/check.h"

namespace pw::allocator::internal {

GenericBuddyAllocator::GenericBuddyAllocator(span<Bucket> buckets,
                                             size_t min_chunk_size,
                                             ByteSpan region)
    : buckets_(buckets) {
  Result<ByteSpan> result = GetAlignedSubspan(region, min_chunk_size);
  PW_CHECK_OK(result.status());
  region_ = result.value();
  PW_CHECK_INT_GE(region_.size(), min_chunk_size);

  Bucket::Init(buckets, min_chunk_size);

  // Build up the available memory by successively freeing (and thus merging)
  // minimum sized chunks.
  std::memset(region_.data(), 0, region_.size());
  for (region = region_; region.size() >= min_chunk_size;
       region = region.subspan(min_chunk_size)) {
    Deallocate(region.data());
  }
}

void GenericBuddyAllocator::CrashIfAllocated() {
  size_t total_free = 0;
  for (auto& bucket : buckets_) {
    size_t bucket_size;
    PW_CHECK_MUL(bucket.chunk_size(), bucket.count(), &bucket_size);
    PW_CHECK_ADD(total_free, bucket_size, &total_free);
  }
  PW_CHECK_INT_EQ(region_.size(),
                  total_free,
                  "%zu bytes were still in use when an allocator was "
                  "destroyed. All memory allocated by an allocator must be "
                  "released before the allocator goes out of scope.",
                  region_.size() - total_free);
  region_ = ByteSpan();
}

void* GenericBuddyAllocator::Allocate(Layout layout) {
  if (layout.alignment() > buckets_[0].chunk_size()) {
    return nullptr;
  }

  std::byte* chunk = nullptr;
  size_t chunk_size = 0;
  size_t index = 0;
  for (auto& bucket : buckets_) {
    chunk_size = bucket.chunk_size();
    if (chunk_size < layout.size()) {
      ++index;
      continue;
    }
    layout = Layout(chunk_size, layout.alignment());

    // Check if this bucket has chunks available.
    chunk = bucket.Remove();
    if (chunk != nullptr) {
      break;
    }

    // No chunk available, allocate one from the next bucket and split it.
    void* ptr = Allocate(layout.Extend(chunk_size));
    if (ptr == nullptr) {
      break;
    }
    chunk = std::launder(reinterpret_cast<std::byte*>(ptr));
    bucket.Add(chunk + chunk_size);
    break;
  }
  if (chunk == nullptr) {
    return nullptr;
  }
  // Store the bucket index in the byte *before* the usable space. Use the last
  // byte for the first chunk.
  if (chunk == region_.data()) {
    region_[region_.size() - 1] = std::byte(index);
  } else {
    *(chunk - 1) = std::byte(index);
  }
  return chunk;
}

void GenericBuddyAllocator::Deallocate(void* ptr) {
  if (ptr == nullptr) {
    return;
  }
  PW_CHECK_OK(Query(ptr));
  auto* chunk = std::launder(reinterpret_cast<std::byte*>(ptr));
  std::byte index =
      ptr == region_.data() ? region_[region_.size() - 1] : *(chunk - 1);
  size_t chunk_size = buckets_[size_t(index)].chunk_size();

  Bucket* bucket = nullptr;
  PW_CHECK_INT_GT(buckets_.size(), 0);
  for (auto& current : span(buckets_.data(), buckets_.size() - 1)) {
    if (current.chunk_size() < chunk_size) {
      continue;
    }
    bucket = &current;

    // Determine the expected address of this chunk's buddy by determining if
    // it would be first or second in a merged chunk of the next larger size.
    std::byte* buddy = chunk;
    if ((chunk - region_.data()) % (chunk_size * 2) == 0) {
      buddy += current.chunk_size();
    } else {
      buddy -= current.chunk_size();
    }

    // Look for the buddy chunk in the previous bucket. If found, remove it from
    // that bucket, and repeat the whole process with the merged chunk.
    void* match =
        current.RemoveIf([buddy](void* other) { return buddy == other; });
    if (match == nullptr) {
      break;
    }
    chunk = std::min(chunk, buddy);
    chunk_size *= 2;
    bucket = nullptr;
  }
  if (bucket == nullptr) {
    bucket = &buckets_.back();
  }

  // Add the (possibly merged) chunk to the correct bucket of free chunks.
  bucket->Add(chunk);
}

Status GenericBuddyAllocator::Query(const void* ptr) const {
  if (ptr < region_.data()) {
    return Status::OutOfRange();
  }
  size_t min_chunk_size = buckets_.front().chunk_size();
  size_t offset = reinterpret_cast<uintptr_t>(ptr) -
                  reinterpret_cast<uintptr_t>(region_.data());
  if (region_.size() <= offset || offset % min_chunk_size != 0) {
    return Status::OutOfRange();
  }
  return OkStatus();
}

}  // namespace pw::allocator::internal
