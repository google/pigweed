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

#include "pw_allocator/metrics.h"

#include <algorithm>
#include <cstddef>

#include "pw_assert/check.h"

namespace pw::allocator::internal {

static uint32_t ClampU32(size_t size) {
  return static_cast<uint32_t>(std::min(
      size, static_cast<size_t>(std::numeric_limits<uint32_t>::max())));
}

void Metrics::Init(size_t capacity) {
  children().clear();
  metrics().clear();
  total_bytes_.Set(capacity);
  allocated_bytes_.Set(0);
  peak_allocated_bytes_.Set(0);
  cumulative_allocated_bytes_.Set(0);
  num_allocations_.Set(0);
  num_deallocations_.Set(0);
  num_resizes_.Set(0);
  num_reallocations_.Set(0);
  Add(total_bytes_);
  Add(allocated_bytes_);
  Add(peak_allocated_bytes_);
  Add(cumulative_allocated_bytes_);
  Add(num_allocations_);
  Add(num_deallocations_);
  Add(num_resizes_);
  Add(num_reallocations_);
  Add(num_failures_);
}

void Metrics::RecordAllocationImpl(uint32_t new_size) {
  allocated_bytes_.Increment(new_size);
  uint32_t allocated_bytes = allocated_bytes_.value();
  if (peak_allocated_bytes_.value() < allocated_bytes) {
    peak_allocated_bytes_.Set(allocated_bytes);
  }
  cumulative_allocated_bytes_.Increment(new_size);
}

void Metrics::RecordAllocation(size_t new_size) {
  RecordAllocationImpl(ClampU32(new_size));
  num_allocations_.Increment();
}

void Metrics::RecordDeallocationImpl(uint32_t old_size) {
  allocated_bytes_.Decrement(old_size);
}

void Metrics::RecordDeallocation(size_t old_size) {
  RecordDeallocationImpl(ClampU32(old_size));
  num_deallocations_.Increment();
}

void Metrics::RecordResizeImpl(uint32_t old_size, uint32_t new_size) {
  allocated_bytes_.Decrement(old_size);
  allocated_bytes_.Increment(new_size);
  if (old_size < new_size) {
    uint32_t allocated_bytes = allocated_bytes_.value();
    if (peak_allocated_bytes_.value() < allocated_bytes) {
      peak_allocated_bytes_.Set(allocated_bytes);
    }
    cumulative_allocated_bytes_.Increment(new_size - old_size);
  }
}

void Metrics::RecordResize(size_t old_size, size_t new_size) {
  RecordResizeImpl(ClampU32(old_size), ClampU32(new_size));
  num_resizes_.Increment();
}

void Metrics::RecordReallocation(size_t old_size, size_t new_size, bool moved) {
  if (moved) {
    RecordAllocationImpl(ClampU32(new_size));
    RecordDeallocationImpl(ClampU32(old_size));
  } else {
    RecordResizeImpl(ClampU32(old_size), ClampU32(new_size));
  }
  num_reallocations_.Increment();
}

void Metrics::RecordFailure() { num_failures_.Increment(); }

}  // namespace pw::allocator::internal
