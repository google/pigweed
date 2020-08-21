// Copyright 2020 The Pigweed Authors
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

#include <span>

#include "pw_allocator/freelist_heap.h"
#include "pw_boot_armv7m/boot.h"
#include "pw_malloc/malloc.h"
#include "pw_preprocessor/util.h"

namespace {
std::aligned_storage_t<sizeof(pw::allocator::FreeListHeapBuffer<>),
                       alignof(pw::allocator::FreeListHeapBuffer<>)>
    buf;
std::span<std::byte> pw_allocator_freelist_raw_heap;
}  // namespace
pw::allocator::FreeListHeapBuffer<>* pw_freelist_heap;

#if __cplusplus
extern "C" {
#endif
// Define the global heap variables.
void pw_MallocInit() {
  // pw_boot_heap_low_addr and pw_boot_heap_high_addr specifies the heap region
  // from the linker script in "pw_boot_armv7m".
  std::span<std::byte> pw_allocator_freelist_raw_heap =
      std::span(reinterpret_cast<std::byte*>(&pw_boot_heap_low_addr),
                &pw_boot_heap_high_addr - &pw_boot_heap_low_addr);
  pw_freelist_heap = new (&buf)
      pw::allocator::FreeListHeapBuffer(pw_allocator_freelist_raw_heap);
}

// Wrapper functions for malloc, free, realloc and calloc.
// With linker options "-Wl --wrap=<function name>", linker will link
// "__wrap_<function name>" with "<function_name>", and calling
// "<function name>" will call "__wrap_<function name>" instead
// Linker options are set in a config in "pw_malloc:pw_malloc_config".
void* __wrap_malloc(size_t size) { return pw_freelist_heap->Allocate(size); }

void __wrap_free(void* ptr) { pw_freelist_heap->Free(ptr); }

void* __wrap_realloc(void* ptr, size_t size) {
  return pw_freelist_heap->Realloc(ptr, size);
}

void* __wrap_calloc(size_t num, size_t size) {
  return pw_freelist_heap->Calloc(num, size);
}

void* __wrap__malloc_r(struct _reent* r, size_t size) {
  PW_UNUSED(r);
  return pw_freelist_heap->Allocate(size);
}

void __wrap__free_r(struct _reent* r, void* ptr) {
  PW_UNUSED(r);
  pw_freelist_heap->Free(ptr);
}

void* __wrap__realloc_r(struct _reent* r, void* ptr, size_t size) {
  PW_UNUSED(r);
  return pw_freelist_heap->Realloc(ptr, size);
}

void* __wrap__calloc_r(struct _reent* r, size_t num, size_t size) {
  PW_UNUSED(r);
  return pw_freelist_heap->Calloc(num, size);
}
#if __cplusplus
}
#endif
