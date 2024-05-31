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
#pragma once

#include <cstdint>

#include "pw_allocator/metrics.h"
#include "pw_allocator/synchronized_allocator.h"
#include "pw_allocator/tracking_allocator.h"

namespace pw::malloc {

struct TestMetrics {
  PW_ALLOCATOR_METRICS_ENABLE(requested_bytes);
  PW_ALLOCATOR_METRICS_ENABLE(allocated_bytes);
  PW_ALLOCATOR_METRICS_ENABLE(cumulative_allocated_bytes);
};

}  // namespace pw::malloc

#define PW_MALLOC_METRICS_TYPE ::pw::malloc::TestMetrics

#define PW_MALLOC_BLOCK_OFFSET_TYPE uint16_t

#define PW_MALLOC_MIN_BUCKET_SIZE 64

#define PW_MALLOC_NUM_BUCKETS 4
