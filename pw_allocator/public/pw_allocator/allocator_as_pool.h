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

#include "pw_allocator/allocator.h"
#include "pw_allocator/layout.h"
#include "pw_allocator/pool.h"
#include "pw_status/status.h"

namespace pw::allocator {

/// Implementation of ``Pool`` that satisfies requests using an ``Allocator``.
class AllocatorAsPool : public Pool {
 public:
  /// Construct a ``Pool`` from an ``Allocator``.
  ///
  /// @param  allocator   The allocator used to create fixed-size allocations.
  /// @param  layout      The size and alignment of the memory to be returned
  ///                     from this pool.
  AllocatorAsPool(Allocator& allocator, const Layout& layout);

 private:
  /// @copydoc Pool::Allocate
  void* DoAllocate() override;

  /// @copydoc Pool::Deallocate
  void DoDeallocate(void* ptr) override;

  /// @copydoc Pool::Query
  Status DoQuery(const void* ptr) const override;

  Allocator& allocator_;
};

}  // namespace pw::allocator
