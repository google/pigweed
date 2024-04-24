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

#include <cstddef>

#include "pw_allocator/allocator.h"
#include "pw_allocator/block.h"
#include "pw_allocator/buffer.h"
#include "pw_allocator/first_fit_block_allocator.h"
#include "pw_allocator/metrics.h"
#include "pw_allocator/tracking_allocator.h"
#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_tokenizer/tokenize.h"
#include "pw_unit_test/framework.h"

namespace pw::allocator {
namespace test {

// A token that can be used in tests.
constexpr pw::tokenizer::Token kToken = PW_TOKENIZE_STRING("test");

/// An `AllocatorForTest` that is automatically initialized on construction.
template <size_t kBufferSize, typename MetricsType = internal::AllMetrics>
class AllocatorForTest : public Allocator {
 public:
  using AllocatorType = FirstFitBlockAllocator<uint32_t>;
  using BlockType = AllocatorType::BlockType;

  AllocatorForTest()
      : Allocator(AllocatorType::kCapabilities), tracker_(kToken, *allocator_) {
    ResetParameters();
    EXPECT_EQ(allocator_->Init(allocator_.as_bytes()), OkStatus());
  }

  ~AllocatorForTest() override {
    for (auto* block : allocator_->blocks()) {
      BlockType::Free(block);
    }
    allocator_->Reset();
  }

  const metric::Group& metric_group() const { return tracker_.metric_group(); }
  metric::Group& metric_group() { return tracker_.metric_group(); }

  const MetricsType& metrics() const { return tracker_.metrics(); }

  size_t allocate_size() const { return allocate_size_; }
  void* deallocate_ptr() const { return deallocate_ptr_; }
  size_t deallocate_size() const { return deallocate_size_; }
  void* resize_ptr() const { return resize_ptr_; }
  size_t resize_old_size() const { return resize_old_size_; }
  size_t resize_new_size() const { return resize_new_size_; }

  /// Resets the recorded parameters to an initial state.
  void ResetParameters() {
    allocate_size_ = 0;
    deallocate_ptr_ = nullptr;
    deallocate_size_ = 0;
    resize_ptr_ = nullptr;
    resize_old_size_ = 0;
    resize_new_size_ = 0;
  }

  /// Allocates all the memory from this object.
  void Exhaust() {
    for (auto* block : allocator_->blocks()) {
      block->MarkUsed();
    }
  }

 private:
  /// @copydoc Allocator::Allocate
  void* DoAllocate(Layout layout) override {
    allocate_size_ = layout.size();
    return tracker_.Allocate(layout);
  }

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr) override {
    Result<Layout> requested = GetRequestedLayout(tracker_, ptr);
    deallocate_ptr_ = ptr;
    deallocate_size_ = requested.ok() ? requested->size() : 0;
    tracker_.Deallocate(ptr);
  }

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, Layout) override { DoDeallocate(ptr); }

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, size_t new_size) override {
    Result<Layout> requested = GetRequestedLayout(tracker_, ptr);
    resize_ptr_ = ptr;
    resize_old_size_ = requested.ok() ? requested->size() : 0;
    resize_new_size_ = new_size;
    return tracker_.Resize(ptr, new_size);
  }

  /// @copydoc Allocator::GetCapacity
  StatusWithSize DoGetCapacity() const override {
    return tracker_.GetCapacity();
  }

  /// @copydoc Allocator::GetRequestedLayout
  Result<Layout> DoGetRequestedLayout(const void* ptr) const override {
    return GetRequestedLayout(tracker_, ptr);
  }

  /// @copydoc Allocator::GetUsableLayout
  Result<Layout> DoGetUsableLayout(const void* ptr) const override {
    return GetUsableLayout(tracker_, ptr);
  }

  /// @copydoc Allocator::GetAllocatedLayout
  Result<Layout> DoGetAllocatedLayout(const void* ptr) const override {
    return GetAllocatedLayout(tracker_, ptr);
  }

  /// @copydoc Allocator::Query
  Status DoQuery(const void* ptr) const override {
    return Query(tracker_, ptr);
  }

  WithBuffer<AllocatorType, kBufferSize> allocator_;
  TrackingAllocator<MetricsType> tracker_;
  size_t allocate_size_;
  void* deallocate_ptr_;
  size_t deallocate_size_;
  void* resize_ptr_;
  size_t resize_old_size_;
  size_t resize_new_size_;
};

}  // namespace test
}  // namespace pw::allocator
