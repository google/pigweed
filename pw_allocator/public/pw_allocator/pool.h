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

#include <cstddef>
#include <cstdint>

#include "pw_allocator/capability.h"
#include "pw_allocator/deallocator.h"
#include "pw_allocator/layout.h"
#include "pw_bytes/span.h"
#include "pw_result/result.h"

namespace pw::allocator {

/// Abstract interface for fixed-layout memory allocation.
///
/// The interface makes no guarantees about its implementation. Consumers of the
/// generic interface must not make any assumptions around allocator behavior,
/// thread safety, or performance.
class Pool : public Deallocator {
 public:
  constexpr Pool(const Capabilities& capabilities, const Layout& layout)
      : Deallocator(capabilities), layout_(layout) {}

  const Layout& layout() const { return layout_; }

  /// Returns a chunk of memory with this object's fixed layout.
  ///
  /// Like `pw::allocator::Allocate`, returns null if memory is exhausted.
  ///
  /// @retval The allocated memory.
  void* Allocate() { return DoAllocate(); }

 private:
  /// Virtual `Allocate` function that can be overridden by derived classes.
  virtual void* DoAllocate() = 0;

  const Layout layout_;
};

}  // namespace pw::allocator
