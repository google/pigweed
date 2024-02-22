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
#include "pw_allocator/metrics.h"
#include "pw_allocator/simple_allocator.h"
#include "pw_allocator/tracking_allocator.h"
#include "pw_bytes/span.h"
#include "pw_result/result.h"
#include "pw_status/status.h"
#include "pw_tokenizer/tokenize.h"
#include "pw_unit_test/framework.h"

namespace pw::allocator {
namespace internal {

struct RecordedParameters {
  size_t allocate_size = 0;
  void* deallocate_ptr = nullptr;
  size_t deallocate_size = 0;
  void* resize_ptr = nullptr;
  size_t resize_old_size = 0;
  size_t resize_new_size = 0;
};

/// Simple memory allocator for testing.
///
/// This allocator records the most recent parameters passed to the `Allocator`
/// interface methods, and returns them via accessors.
class AllocatorForTestImpl : public Allocator {
 public:
  ~AllocatorForTestImpl() override;

  void Init(Allocator& allocator, RecordedParameters& params);

  /// Resets the recorded parameters to an initial state.
  void Reset();

 private:
  /// @copydoc Allocator::Allocate
  void* DoAllocate(Layout layout) override;

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, Layout layout) override;

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, Layout layout, size_t new_size) override;

  /// @copydoc Allocator::GetLayout
  Result<Layout> DoGetLayout(const void* ptr) const override;

  /// @copydoc Allocator::Query
  Status DoQuery(const void* ptr, Layout layout) const override;

  Allocator* allocator_ = nullptr;
  RecordedParameters* params_ = nullptr;
};

}  // namespace internal
namespace test {

using Metrics = pw::allocator::internal::Metrics;

// A token that can be used in tests.
constexpr pw::tokenizer::Token kToken = PW_TOKENIZE_STRING("test");

/// An `AllocatorForTest` that is automatically initialized on construction.
template <size_t kBufferSize>
class AllocatorForTest : public TrackingAllocatorImpl<Metrics> {
 public:
  using Base = TrackingAllocatorImpl<Metrics>;
  using BlockType = SimpleAllocator::BlockType;

  AllocatorForTest() : Base(kToken) {
    EXPECT_EQ(allocator_->Init(allocator_.as_bytes()), OkStatus());
    recorder_.Init(*allocator_, params_);
    Init(recorder_);
  }

  ~AllocatorForTest() override {
    recorder_.Reset();
    for (auto* block : allocator_->blocks()) {
      BlockType::Free(block);
    }
    allocator_->Reset();
  }

  size_t allocate_size() const { return params_.allocate_size; }
  void* deallocate_ptr() const { return params_.deallocate_ptr; }
  size_t deallocate_size() const { return params_.deallocate_size; }
  void* resize_ptr() const { return params_.resize_ptr; }
  size_t resize_old_size() const { return params_.resize_old_size; }
  size_t resize_new_size() const { return params_.resize_new_size; }

  /// Resets the recorded parameters to an initial state.
  void ResetParameters() { params_ = internal::RecordedParameters{}; }

  /// Allocates all the memory from this object.
  void Exhaust() {
    for (auto* block : allocator_->blocks()) {
      block->MarkUsed();
    }
  }

 private:
  internal::AllocatorForTestImpl recorder_;
  internal::RecordedParameters params_;
  WithBuffer<SimpleAllocator, kBufferSize> allocator_;
};

}  // namespace test
}  // namespace pw::allocator
