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
#include <cstdio>
#include <cstring>
#include <type_traits>

#include "pw_allocator/allocator.h"
#include "pw_assert/assert.h"
#include "pw_bloat/bloat_this_binary.h"
#include "pw_bytes/span.h"

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
  class Foo final {
   public:
    explicit Foo(const char* name) {
      std::snprintf(name_.data(), name_.size(), "%s", name);
    }
    const char* name() const { return name_.data(); }

   private:
    std::array<char, 16> name_;
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

  SizeReporter() { pw::bloat::BloatThisBinary(); }
  ~SizeReporter() = default;

  ByteSpan buffer() { return buffer_; }

  /// Exercises an allocator as part of a size report.
  ///
  /// @param[in]  allocator   The allocator to exercise. Will be ignored if not
  ///                         derived from `Allocator`.
  template <typename Derived>
  void MeasureAllocator(Derived* allocator) {
    void* ptr = nullptr;
    Layout layout = Layout::Of<Foo>();

    // Measure `Allocate`.
    if constexpr (std::is_base_of_v<Allocator, Derived>) {
      ptr = allocator->Allocate(layout);
    } else {
      ptr = buffer_.data();
    }
    if (ptr != nullptr) {
    }

    // Measure `Reallocate`.
    if constexpr (std::is_base_of_v<Allocator, Derived>) {
      allocator->Resize(ptr, layout, sizeof(Bar));
    }

    // Measure `Reallocate`.
    if constexpr (std::is_base_of_v<Allocator, Derived>) {
      ptr = allocator->Reallocate(ptr, layout, sizeof(Baz));
    }

    // Measure `GetLayout`.
    if constexpr (std::is_base_of_v<Allocator, Derived>) {
      Result<Layout> result = allocator->GetLayout(ptr);
      if (result.ok()) {
        layout = result.value();
      } else {
        layout = Layout::Of<Bar>();
      }
    }

    // Measure `Query`.
    if constexpr (std::is_base_of_v<Allocator, Derived>) {
      Status status = allocator->Query(ptr, layout);
      PW_ASSERT(ptr == nullptr || status.ok() || status.IsUnimplemented());
    }

    // Measure `Deallocate`.
    if constexpr (std::is_base_of_v<Allocator, Derived>) {
      allocator->Deallocate(ptr, layout);
    }

    // Measure `New`.
    Foo* foo = nullptr;
    if constexpr (std::is_base_of_v<Allocator, Derived>) {
      foo = allocator->template New<Foo>("foo");
    } else {
      foo = new (buffer_.data()) Foo("foo");
    }

    // Measure `Delete`.
    if constexpr (std::is_base_of_v<Allocator, Derived>) {
      allocator->template Delete(foo);
    } else {
      std::destroy_at(foo);
    }

    // Measure `MakeUnique`.
    if constexpr (std::is_base_of_v<Allocator, Derived>) {
      UniquePtr<Foo> unique_foo = allocator->template MakeUnique<Foo>("foo");
      PW_ASSERT(unique_foo != nullptr);
    } else {
      PW_ASSERT(buffer_.data() != nullptr);
    }
  }

 private:
  std::array<std::byte, 256> buffer_;
};

}  // namespace pw::allocator
