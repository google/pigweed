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
#include "pw_metric/metric.h"
#include "pw_status/status.h"
#include "pw_tokenizer/tokenize.h"

namespace pw::allocator {

/// This class simply dispatches between a primary and secondary allocator. Any
/// attempt to allocate memory will first be handled by the primary allocator.
/// If it cannot allocate memory, e.g. because it is out of memory, the
/// secondary alloator will try to allocate memory instead.
class FallbackAllocator : public Allocator {
 public:
  using MetricsType = typename AllocatorMetricProxy::MetricsType;

  constexpr FallbackAllocator() : secondary_(kSecondary) {}

  MetricsType& secondary_metric_group() { return secondary_.metric_group(); }
  const MetricsType& secondary_metric_group() const {
    return secondary_.metric_group();
  }

  uint32_t used() const { return secondary_.used(); }
  uint32_t peak() const { return secondary_.peak(); }
  uint32_t count() const { return secondary_.count(); }

  /// Sets the primary and secondary allocators.
  ///
  /// It is an error to call any method without calling this method first.
  void Init(Allocator& primary, Allocator& secondary);

 private:
  static constexpr metric::Token kSecondary = PW_TOKENIZE_STRING("fallback");

  /// @copydoc Allocator::Query
  Status DoQuery(const void* ptr, Layout layout) const override;

  /// @copydoc Allocator::Allocate
  void* DoAllocate(Layout layout) override;

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, Layout layout) override;

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, Layout layout, size_t new_size) override;

  Allocator* primary_ = nullptr;
  AllocatorMetricProxy secondary_;
};

}  // namespace pw::allocator
