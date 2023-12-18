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
#include <cstring>

#include "FreeRTOS.h"
#include "pw_malloc/malloc.h"
#include "pw_preprocessor/compiler.h"

PW_EXTERN_C_START

// Define the global heap variables.
void pw_MallocInit([[maybe_unused]] uint8_t* heap_low_addr,
                   [[maybe_unused]] uint8_t* heap_high_addr) {}

// Wrapper functions for malloc, free, realloc and calloc.
// With linker options "-Wl --wrap=<function name>", linker will link
// "__wrap_<function name>" with "<function_name>", and calling
// "<function name>" will call "__wrap_<function name>" instead
void* __wrap_malloc(size_t size) { return pvPortMalloc(size); }

void __wrap_free(void* ptr) { vPortFree(ptr); }

void* __wrap_realloc(void* ptr, size_t size) {
  if (size == 0) {
    vPortFree(ptr);
    return nullptr;
  }

  void* p = pvPortMalloc(size);
  if (p) {
    if (ptr != nullptr) {
      memcpy(p, ptr, size);
      vPortFree(ptr);
    }
  }
  return p;
}

void* __wrap_calloc(size_t num, size_t size) {
  void* p = pvPortMalloc(num * size);
  if (p) {
    memset(p, 0, num * size);
  }
  return p;
}

void* __wrap__malloc_r(struct _reent*, size_t size) {
  return pvPortMalloc(size);
}

void __wrap__free_r(struct _reent*, void* ptr) { vPortFree(ptr); }

void* __wrap__realloc_r(struct _reent*, void* ptr, size_t size) {
  return __wrap_realloc(ptr, size);
}

void* __wrap__calloc_r(struct _reent*, size_t num, size_t size) {
  return __wrap_calloc(num, size);
}

PW_EXTERN_C_END
