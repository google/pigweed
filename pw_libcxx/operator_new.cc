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

#include <cstddef>
#include <cstdlib>
#include <new>

void* operator new(size_t __sz) { return malloc(__sz); }

void* operator new[](size_t __sz) { return malloc(__sz); }

void* operator new(size_t __sz, std::align_val_t alignment) {
  return aligned_alloc(static_cast<size_t>(alignment), __sz);
}

void* operator new[](size_t __sz, std::align_val_t alignment) {
  return aligned_alloc(static_cast<size_t>(alignment), __sz);
}

void* operator new(size_t __sz, const std::nothrow_t&) noexcept {
  return ::operator new(__sz);
}

void* operator new[](size_t __sz, const std::nothrow_t&) noexcept {
  return ::operator new[](__sz);
}

void* operator new(size_t __sz,
                   std::align_val_t alignment,
                   const std::nothrow_t&) noexcept {
  return ::operator new(__sz, alignment);
}

void* operator new[](size_t __sz,
                     std::align_val_t alignment,
                     const std::nothrow_t&) noexcept {
  return ::operator new[](__sz, alignment);
}
