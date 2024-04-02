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

#include "pw_multibuf/simple_allocator.h"

#include <algorithm>
#include <mutex>

#include "pw_assert/check.h"

namespace pw::multibuf {
namespace internal {

LinkedRegionTracker::~LinkedRegionTracker() {
  // The ``LinkedRegionTracker`` *must* be removed from the parent allocator's
  // region list prior to being destroyed, as doing so requires holding a lock.
  //
  // The destructor is only called via ``Destroy()``'s invocation of
  // ``metadata_alloc_ref.Delete(this);``
  PW_DCHECK(unlisted());
}

void LinkedRegionTracker::Destroy() {
  SimpleAllocator::AvailableMemorySize available;
  {
    // N.B.: this lock *must* go out of scope before the call to
    // ``Delete(this)`` below in order to prevent referencing the ``parent_``
    // field after this tracker has been destroyed.
    std::lock_guard lock(parent_.lock_);
    unlist();
    available = parent_.GetAvailableMemorySize();
  }
  parent_.MoreMemoryAvailable(available.total, available.contiguous);
  parent_.metadata_alloc_.Delete(this);
}

void* LinkedRegionTracker::AllocateChunkClass() {
  return parent_.metadata_alloc_.Allocate(allocator::Layout::Of<Chunk>());
}

void LinkedRegionTracker::DeallocateChunkClass(void* ptr) {
  return parent_.metadata_alloc_.Deallocate(ptr);
}

}  // namespace internal

pw::Result<MultiBuf> SimpleAllocator::DoAllocate(size_t min_size,
                                                 size_t desired_size,
                                                 bool needs_contiguous) {
  if (min_size > data_area_.size()) {
    return Status::OutOfRange();
  }
  // NB: std::lock_guard is not used here in order to release the lock
  // prior to destroying ``buf`` below.
  lock_.lock();
  auto available_memory_size = GetAvailableMemorySize();
  size_t available = needs_contiguous ? available_memory_size.contiguous
                                      : available_memory_size.total;
  if (available < min_size) {
    lock_.unlock();
    return Status::ResourceExhausted();
  }
  size_t goal_size = std::min(desired_size, available);
  if (needs_contiguous) {
    auto out = InternalAllocateContiguous(goal_size);
    lock_.unlock();
    return out;
  }

  MultiBuf buf;
  Status status;
  size_t remaining_goal = goal_size;
  ForEachFreeBlock(
      [this, &buf, &status, remaining_goal](const FreeBlock& block)
          PW_EXCLUSIVE_LOCKS_REQUIRED(lock_) mutable {
            if (remaining_goal == 0) {
              return ControlFlow::Break;
            }
            size_t chunk_size = std::min(block.span.size(), remaining_goal);
            pw::Result<OwnedChunk> chunk = InsertRegion(
                {block.iter, ByteSpan(block.span.data(), chunk_size)});
            if (!chunk.ok()) {
              status = chunk.status();
              return ControlFlow::Break;
            }
            remaining_goal -= chunk->size();
            buf.PushFrontChunk(std::move(*chunk));
            return ControlFlow::Continue;
          });
  // Lock must be released prior to possibly free'ing the `buf` in the case
  // where `!status.ok()`. This is necessary so that the destructing chunks
  // can free their regions.
  lock_.unlock();
  if (!status.ok()) {
    return status;
  }
  return buf;
}

pw::Result<MultiBuf> SimpleAllocator::InternalAllocateContiguous(size_t size) {
  pw::Result<MultiBuf> buf = Status::ResourceExhausted();
  ForEachFreeBlock([this, &buf, size](const FreeBlock& block)
                       PW_EXCLUSIVE_LOCKS_REQUIRED(lock_) {
                         if (block.span.size() >= size) {
                           ByteSpan buf_span(block.span.data(), size);
                           buf = InsertRegion({block.iter, buf_span})
                                     .transform(MultiBuf::FromChunk);
                           return ControlFlow::Break;
                         }
                         return ControlFlow::Continue;
                       });
  return buf;
}

pw::Result<OwnedChunk> SimpleAllocator::InsertRegion(const FreeBlock& block) {
  internal::LinkedRegionTracker* new_region =
      metadata_alloc_.New<internal::LinkedRegionTracker>(*this, block.span);
  if (new_region == nullptr) {
    return Status::OutOfRange();
  }
  std::optional<OwnedChunk> chunk = new_region->CreateFirstChunk();
  if (!chunk.has_value()) {
    metadata_alloc_.Delete(new_region);
    return Status::OutOfRange();
  }
  regions_.insert_after(block.iter, *new_region);
  return std::move(*chunk);
}

SimpleAllocator::AvailableMemorySize SimpleAllocator::GetAvailableMemorySize() {
  size_t total = 0;
  size_t max_contiguous = 0;
  ForEachFreeBlock([&total, &max_contiguous](const FreeBlock& block) {
    total += block.span.size();
    if (block.span.size() > max_contiguous) {
      max_contiguous = block.span.size();
    }
    return ControlFlow::Continue;
  });

  AvailableMemorySize available;
  available.total = total;
  available.contiguous = max_contiguous;
  return available;
}

}  // namespace pw::multibuf
