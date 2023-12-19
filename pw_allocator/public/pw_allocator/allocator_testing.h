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
#include "pw_allocator/allocator_metric_proxy.h"
#include "pw_allocator/block.h"
#include "pw_allocator/buffer.h"
#include "pw_allocator/metrics.h"
#include "pw_allocator/simple_allocator.h"
#include "pw_bytes/span.h"
#include "pw_status/status.h"
#include "pw_tokenizer/tokenize.h"
#include "pw_unit_test/framework.h"

namespace pw::allocator::test {
namespace internal {

using pw::allocator::internal::Metrics;

/// Simple memory allocator for testing.
///
/// This allocator records the most recent parameters passed to the `Allocator`
/// interface methods, and returns them via accessors.
class AllocatorForTestImpl : public AllocatorWithMetrics<Metrics> {
 public:
  using metrics_type = Metrics;

  AllocatorForTestImpl() : proxy_(PW_TOKENIZE_STRING_EXPR("test")) {}
  ~AllocatorForTestImpl() override;

  metrics_type& metric_group() override { return proxy_.metric_group(); }
  const metrics_type& metric_group() const override {
    return proxy_.metric_group();
  }

  size_t allocate_size() const { return allocate_size_; }
  void* deallocate_ptr() const { return deallocate_ptr_; }
  size_t deallocate_size() const { return deallocate_size_; }
  void* resize_ptr() const { return resize_ptr_; }
  size_t resize_old_size() const { return resize_old_size_; }
  size_t resize_new_size() const { return resize_new_size_; }

  /// Provides memory for the allocator to allocate from.
  ///
  /// If this method is called, then `Reset` MUST be called before the object is
  /// destroyed.
  Status Init(ByteSpan bytes);

  /// Allocates all the memory from this object.
  void Exhaust();

  /// Resets the recorded parameters to an initial state.
  void ResetParameters();

  /// Resets the object to an initial state.
  ///
  /// This frees all memory associated with this allocator and disassociates the
  /// allocator from its memory region. If `Init` is called, then this method
  /// MUST be called before the object is destroyed.
  void Reset();

 private:
  using BlockType = Block<>;

  /// @copydoc Allocator::Query
  Status DoQuery(const void* ptr, Layout layout) const override;

  /// @copydoc Allocator::Allocate
  void* DoAllocate(Layout layout) override;

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, Layout layout) override;

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, Layout layout, size_t new_size) override;

  SimpleAllocator allocator_;
  AllocatorMetricProxyImpl<metrics_type> proxy_;

  bool initialized_ = false;
  size_t allocate_size_ = 0;
  void* deallocate_ptr_ = nullptr;
  size_t deallocate_size_ = 0;
  void* resize_ptr_ = nullptr;
  size_t resize_old_size_ = 0;
  size_t resize_new_size_ = 0;
};

}  // namespace internal

/// An `AllocatorForTest` that is automatically initialized on construction.
template <size_t kBufferSize>
class AllocatorForTest
    : public WithBuffer<internal::AllocatorForTestImpl, kBufferSize> {
 public:
  using allocator_type = internal::AllocatorForTestImpl;
  using metrics_type = allocator_type::metrics_type;

  AllocatorForTest() {
    EXPECT_EQ((*this)->Init(ByteSpan(this->data(), this->size())), OkStatus());
  }
  ~AllocatorForTest() { (*this)->Reset(); }

  /// Helper method to get a pointer to underlying allocator.
  AllocatorWithMetrics<metrics_type>* get() { return &**this; }
};

}  // namespace pw::allocator::test
