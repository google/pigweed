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

#pragma once

#include <memory>

#include "pw_allocator/allocator_metric_proxy_for_test.h"
#include "pw_allocator/split_free_list_allocator.h"
#include "pw_multibuf/chunk.h"

namespace pw::multibuf::internal {

/// A basic ``Allocator`` implementation that reports the number and size of
/// allocations.
class TrackingAllocator : public pw::allocator::Allocator {
 public:
  /// Constructs a new ``TrackingAllocator`` which allocates from the provided
  /// region of memory.
  TrackingAllocator(ByteSpan span) : alloc_stats_(kFakeToken) {
    Status status = alloc_.Init(span, kFakeThreshold);
    EXPECT_EQ(status, OkStatus());
    alloc_stats_.Init(alloc_);
  }

  /// Returns the number of current allocations.
  size_t count() const { return alloc_stats_.count(); }

  /// Returns the combined size in bytes of all current allocations.
  size_t used() const { return alloc_stats_.used(); }

 protected:
  void* DoAllocate(allocator::Layout layout) override {
    return alloc_stats_.Allocate(layout);
  }
  bool DoResize(void* ptr,
                allocator::Layout old_layout,
                size_t new_size) override {
    return alloc_stats_.Resize(ptr, old_layout, new_size);
  }
  void DoDeallocate(void* ptr, allocator::Layout layout) override {
    alloc_stats_.Deallocate(ptr, layout);
  }

 private:
  const size_t kFakeThreshold = 0;
  const int32_t kFakeToken = 0;

  pw::allocator::SplitFreeListAllocator<> alloc_;
  pw::allocator::AllocatorMetricProxy alloc_stats_;
};

/// A ``TrackingAllocator`` which holds an internal buffer of size `num_buffer`
/// for its allocations.
template <auto num_bytes>
class TrackingAllocatorWithMemory : public pw::allocator::Allocator {
 public:
  TrackingAllocatorWithMemory() : mem_(), alloc_(mem_) {}
  size_t count() const { return alloc_.count(); }
  size_t used() const { return alloc_.used(); }
  void* DoAllocate(allocator::Layout layout) override {
    return alloc_.Allocate(layout);
  }
  bool DoResize(void* ptr,
                allocator::Layout old_layout,
                size_t new_size) override {
    return alloc_.Resize(ptr, old_layout, new_size);
  }
  void DoDeallocate(void* ptr, allocator::Layout layout) override {
    alloc_.Deallocate(ptr, layout);
  }

 private:
  std::array<std::byte, num_bytes> mem_;
  TrackingAllocator alloc_;
};

/// A ``ChunkRegionTracker`` which stores its ``Chunk`` and region metadata
/// in a ``pw::allocator::Allocator`` allocation alongside the data.
class HeaderChunkRegionTracker final : public ChunkRegionTracker {
 public:
  /// Allocates a new ``Chunk`` region of ``size`` bytes  in ``alloc``.
  ///
  /// The underlyiing allocation will also store the
  /// ``HeaderChunkRegionTracker`` itself.
  ///
  /// Returns the newly-created ``OwnedChunk`` if successful.
  static std::optional<OwnedChunk> AllocateRegionAsChunk(
      pw::allocator::Allocator* alloc, size_t size) {
    HeaderChunkRegionTracker* tracker = AllocateRegion(alloc, size);
    if (tracker == nullptr) {
      return std::nullopt;
    }
    std::optional<OwnedChunk> chunk = Chunk::CreateFirstForRegion(*tracker);
    if (!chunk.has_value()) {
      tracker->Destroy();
      return std::nullopt;
    }
    return chunk;
  }

  /// Allocates a new ``Chunk`` region of ``size`` bytes  in ``alloc``.
  ///
  /// The underlyiing allocation will also store the
  /// ``HeaderChunkRegionTracker`` itself.
  ///
  /// Returns a pointer to the newly-created ``HeaderChunkRegionTracker``
  /// or ``nullptr`` if the allocation failed.
  static HeaderChunkRegionTracker* AllocateRegion(
      pw::allocator::Allocator* alloc, size_t size) {
    auto layout =
        allocator::Layout::Of<HeaderChunkRegionTracker>().Extend(size);
    void* ptr = alloc->Allocate(layout);
    if (ptr == nullptr) {
      return nullptr;
    }
    std::byte* data =
        reinterpret_cast<std::byte*>(ptr) + sizeof(HeaderChunkRegionTracker);
    return new (ptr) HeaderChunkRegionTracker(ByteSpan(data, size), alloc);
  }

  ByteSpan Region() const final { return region_; }
  ~HeaderChunkRegionTracker() final {}

 protected:
  void Destroy() final {
    std::byte* ptr = reinterpret_cast<std::byte*>(this);
    auto layout = allocator::Layout::Of<HeaderChunkRegionTracker>().Extend(
        region_.size());
    auto alloc = alloc_;
    std::destroy_at(this);
    alloc->Deallocate(ptr, layout);
  }
  void* AllocateChunkClass() final {
    return alloc_->Allocate(pw::allocator::Layout::Of<Chunk>());
  }
  void DeallocateChunkClass(void* ptr) final {
    alloc_->Deallocate(ptr, pw::allocator::Layout::Of<Chunk>());
  }

 private:
  ByteSpan region_;
  pw::allocator::Allocator* alloc_;

  // NOTE: `region` must directly follow this `FakeChunkRegionTracker`
  // in memory allocated by allocated by `alloc`.
  HeaderChunkRegionTracker(ByteSpan region, pw::allocator::Allocator* alloc)
      : region_(region), alloc_(alloc) {}
};

}  // namespace pw::multibuf::internal
