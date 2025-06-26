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

#include "pw_malloc/malloc.h"

#include <cstring>
#include <new>

#include "pw_allocator/allocator.h"
#include "pw_allocator/metrics.h"
#include "pw_allocator/synchronized_allocator.h"
#include "pw_allocator/tracking_allocator.h"
#include "pw_assert/check.h"
#include "pw_malloc/config.h"
#include "pw_metric/metric.h"
#include "pw_numeric/checked_arithmetic.h"
#include "pw_tokenizer/tokenize.h"

namespace {

using ::pw::allocator::Layout;
using ::pw::allocator::NoMetrics;
using ::pw::allocator::NoSync;
using ::pw::allocator::SynchronizedAllocator;
using ::pw::allocator::TrackingAllocator;
using ::pw::metric::Token;

const PW_MALLOC_METRICS_TYPE* system_metrics = nullptr;

/// Instantiates the system allocator, based on the module configuration.
///
/// This function must be a template to conditionally omit constexpr branches.
template <typename MetricsType, typename LockType>
pw::Allocator& WrapSystemAllocator() {
  pw::Allocator* system = pw::malloc::GetSystemAllocator();
  if constexpr (!std::is_same_v<MetricsType, NoMetrics>) {
    constexpr static Token kToken = PW_TOKENIZE_STRING("system allocator");
    static TrackingAllocator<MetricsType> tracker(kToken, *system);
    system = &tracker;
    system_metrics = &tracker.metrics();
  } else {
    static MetricsType no_metrics;
    system_metrics = &no_metrics;
  }
  if constexpr (!std::is_same_v<LockType, NoSync>) {
    static SynchronizedAllocator<LockType> synchronized(*system);
    system = &synchronized;
  }
  return *system;
}

pw::Allocator& SystemAllocator() {
  static pw::Allocator& system =
      WrapSystemAllocator<PW_MALLOC_METRICS_TYPE, PW_MALLOC_LOCK_TYPE>();
  return system;
}

}  // namespace

namespace pw::malloc {

void InitSystemAllocator(void* heap_low_addr, void* heap_high_addr) {
  auto* lo = std::launder(reinterpret_cast<std::byte*>(heap_low_addr));
  auto* hi = std::launder(reinterpret_cast<std::byte*>(heap_high_addr));
  InitSystemAllocator(pw::ByteSpan(lo, (hi - lo)));
}

const PW_MALLOC_METRICS_TYPE& GetSystemMetrics() {
  SystemAllocator();
  return *system_metrics;
}

}  // namespace pw::malloc

PW_EXTERN_C_START

void pw_MallocInit(uint8_t* heap_low_addr, uint8_t* heap_high_addr) {
  pw::malloc::InitSystemAllocator(heap_low_addr, heap_high_addr);
}

// Wrapper functions for malloc, free, realloc and calloc.
// With linker options "-Wl --wrap=<function name>", linker will link
// "__wrap_<function name>" with "<function_name>", and calling
// "<function name>" will call "__wrap_<function name>" instead
void* __wrap_malloc(size_t size) {
  return SystemAllocator().Allocate(Layout(size));
}

void __wrap_free(void* ptr) { SystemAllocator().Deallocate(ptr); }

void* __wrap_realloc(void* ptr, size_t size) {
  return SystemAllocator().Reallocate(ptr, Layout(size));
}

void* __wrap_calloc(size_t num, size_t size) {
  if (!pw::CheckedMul(num, size, size)) {
    return nullptr;
  }
  void* ptr = __wrap_malloc(size);
  if (ptr != nullptr) {
    std::memset(ptr, 0, size);
  }
  return ptr;
}

void* __wrap__malloc_r(struct _reent*, size_t size) {
  return __wrap_malloc(size);
}

void __wrap__free_r(struct _reent*, void* ptr) { __wrap_free(ptr); }

void* __wrap__realloc_r(struct _reent*, void* ptr, size_t size) {
  return __wrap_realloc(ptr, size);
}

void* __wrap__calloc_r(struct _reent*, size_t num, size_t size) {
  return __wrap_calloc(num, size);
}

PW_EXTERN_C_END
