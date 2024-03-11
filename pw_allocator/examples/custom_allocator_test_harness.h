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

// DOCSTAG: [pw_allocator-examples-custom_allocator-test_harness]
#include <cstddef>

#include "examples/custom_allocator.h"
#include "pw_allocator/test_harness.h"
#include "pw_allocator/testing.h"

namespace examples {

class CustomAllocatorTestHarness
    : public pw::allocator::test::AllocatorTestHarness<64> {
 public:
  static constexpr size_t kCapacity = 0x1000;
  static constexpr size_t kThreshold = 0x800;

  CustomAllocatorTestHarness() : custom_(allocator_, kThreshold) {}
  ~CustomAllocatorTestHarness() override = default;

  pw::allocator::Allocator* Init() override { return &custom_; }

 private:
  pw::allocator::test::AllocatorForTest<kCapacity> allocator_;
  CustomAllocator custom_;
};

}  // namespace examples
