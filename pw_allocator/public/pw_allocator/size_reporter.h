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

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "pw_assert/assert.h"
#include "pw_bloat/bloat_this_binary.h"
#include "pw_bytes/span.h"

#ifndef PW_ALLOCATOR_SIZE_REPORTER_BASE
#include "pw_allocator/allocator.h"
#include "pw_allocator/null_allocator.h"
#endif  // PW_ALLOCATOR_SIZE_REPORTER_BASE

namespace pw::allocator {

/// Utility class for generating allocator size reports.
///
/// The `pw_bloat` module can be used to compare the size of binaries. This
/// class facilitates creating binaries with and without a given allocator type.
///
/// To create a size report:
///   1. Make a copy of //pw_allocator/size_report/base.cc
///   2. Instantiate your allocator and pass it to `MeasureAllocator`.
///   3. Create build target(s) for your binary, and a `pw_size_diff` target
///      that compares it to "$dir_pw_allocator/size_report:base".
class SizeReporter final {
 public:
  /// Nested type used for exercising an allocator.
  struct Foo final {
    std::array<std::byte, 16> buffer;
  };

  /// Nested type used for exercising an allocator.
  struct Bar {
    Foo foo;
    size_t number;
  };

  /// Nested type used for exercising an allocator.
  struct Baz {
    Foo foo;
    uint16_t id;
  };

  void SetBaseline() {
    pw::bloat::BloatThisBinary();
    ByteSpan bytes = buffer();
    Foo* foo = new (bytes.data()) Foo();
    PW_ASSERT(foo != nullptr);
    std::destroy_at(foo);
#ifndef PW_ALLOCATOR_SIZE_REPORTER_BASE
    PW_ASSERT(allocator_.Allocate(Layout::Of<Foo>()) == nullptr);
#endif  // PW_ALLOCATOR_SIZE_REPORTER_BASE
  }

  ByteSpan buffer() { return buffer_; }

#ifndef PW_ALLOCATOR_SIZE_REPORTER_BASE
  /// Exercises an allocator as part of a size report.
  ///
  /// @param[in]  allocator   The allocator to exercise. Will be ignored if not
  ///                         derived from `Allocator`.
  void Measure(Allocator& allocator) {
    // Measure `Layout::Of`.
    Layout layout = Layout::Of<Foo>();

    // Measure `Allocate`.
    void* ptr = allocator.Allocate(layout);

    // Measure `Reallocate`.
    allocator.Resize(ptr, sizeof(Bar));

    // Measure `Reallocate`.
    ptr = allocator.Reallocate(ptr, Layout::Of<Baz>());

    // Measure `GetLayout`.
    Result<Layout> result = allocator.GetLayout(ptr);
    layout = result.ok() ? result.value() : Layout::Of<Bar>();

    // Measure `Deallocate`.
    allocator.Deallocate(ptr);

    // Measure `New`.
    Foo* foo = allocator.template New<Foo>();

    // Measure `Delete`.
    allocator.template Delete(foo);

    // Measure `MakeUnique`.
    UniquePtr<Foo> unique_foo = allocator.template MakeUnique<Foo>();
    PW_ASSERT(ptr == nullptr || unique_foo != nullptr);
  }
#endif  // PW_ALLOCATOR_SIZE_REPORTER_BASE

 private:
#ifndef PW_ALLOCATOR_SIZE_REPORTER_BASE
  // Include a NullAllocator to bake in the costs of the base `Allocator`.
  NullAllocator allocator_;
#endif  // PW_ALLOCATOR_SIZE_REPORTER_BASE
  std::array<std::byte, 128> buffer_;
};

}  // namespace pw::allocator
