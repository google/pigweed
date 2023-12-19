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

#include "pw_allocator/allocator.h"
#include "pw_allocator/allocator_metric_proxy.h"
#include "pw_allocator/metrics.h"
#include "pw_metric/metric.h"
#include "pw_status/status.h"
#include "pw_tokenizer/tokenize.h"

namespace pw::allocator {

/// This class simply dispatches between a primary and secondary allocator. Any
/// attempt to allocate memory will first be handled by the primary allocator.
/// If it cannot allocate memory, e.g. because it is out of memory, the
/// secondary alloator will try to allocate memory instead.
template <typename MetricsType>
class FallbackAllocatorImpl : public Allocator,
                              public WithMetrics<MetricsType> {
 public:
  using metrics_type = MetricsType;

  constexpr FallbackAllocatorImpl() : secondary_(kSecondary) {}

  metrics_type& metric_group() override { return secondary_.metric_group(); }
  const metrics_type& metric_group() const override {
    return secondary_.metric_group();
  }

  /// Sets the primary and secondary allocators.
  ///
  /// It is an error to call any method without calling this method first.
  void Init(Allocator& primary, Allocator& secondary) {
    primary_ = &primary;
    secondary_.Init(secondary);
  }

 private:
  static constexpr metric::Token kSecondary = PW_TOKENIZE_STRING("fallback");

  /// @copydoc Allocator::Query
  Status DoQuery(const void* ptr, Layout layout) const override {
    auto status = primary_->Query(ptr, layout);
    return status.ok() ? status : secondary_.Query(ptr, layout);
  }

  /// @copydoc Allocator::Allocate
  void* DoAllocate(Layout layout) override {
    void* ptr = primary_->Allocate(layout);
    return ptr != nullptr ? ptr : secondary_.Allocate(layout);
  }

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, Layout layout) override {
    if (primary_->Query(ptr, layout).ok()) {
      primary_->Deallocate(ptr, layout);
    } else {
      secondary_.Deallocate(ptr, layout);
    }
  }

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, Layout layout, size_t new_size) override {
    return primary_->Query(ptr, layout).ok()
               ? primary_->Resize(ptr, layout, new_size)
               : secondary_.Resize(ptr, layout, new_size);
  }

  Allocator* primary_ = nullptr;
  AllocatorMetricProxyImpl<metrics_type> secondary_;
};

/// Fallback allocator that uses the default metrics implementation.
///
/// Depending on the value of the `pw_allocator_COLLECT_METRICS` build argument,
/// the `internal::DefaultMetrics` type is an alias for either the real or stub
/// metrics implementation.
using FallbackAllocator = FallbackAllocatorImpl<internal::DefaultMetrics>;

}  // namespace pw::allocator
