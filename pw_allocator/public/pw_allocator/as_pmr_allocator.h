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

#if __has_include(<memory_resource>)
#include <memory_resource>
#elif __has_include(<experimental/memory_resource>)
#include <experimental/memory_resource>
#else
#error "<memory_resource> is required to use this header!"
#endif  // __has_include(<memory_resource>)

#include "pw_status/status_with_size.h"

namespace pw {

class Allocator;

namespace allocator {

#if __has_include(<memory_resource>)
using StdMemoryResource = ::std::pmr::memory_resource;
using StdPolymorphicAllocator = ::std::pmr::polymorphic_allocator<std::byte>;
#elif __has_include(<experimental/memory_resource>)
using StdMemoryResource = ::std::experimental::pmr::memory_resource;
using StdPolymorphicAllocator =
    ::std::experimental::pmr::polymorphic_allocator<std::byte>;
#endif  // __has_include(<memory_resource>)

class AsPmrAllocator;

namespace internal {

/// Implementation of C++'s abstract memory resource interface that uses an
/// Allocator.
///
/// NOTE! This class aborts if allocation fails.
///
/// See also https://en.cppreference.com/w/cpp/memory/memory_resource.
class MemoryResource final : public StdMemoryResource {
 public:
  constexpr MemoryResource() = default;

  Allocator& allocator() { return *allocator_; }

 private:
  friend class ::pw::allocator::AsPmrAllocator;
  void set_allocator(Allocator& allocator) { allocator_ = &allocator; }

  void* do_allocate(size_t bytes, size_t alignment) override;
  void do_deallocate(void* p, size_t bytes, size_t alignment) override;
  bool do_is_equal(const StdMemoryResource& other) const noexcept override;

  Allocator* allocator_ = nullptr;
};

}  // namespace internal

/// Implementation of C++'s abstract polymorphic allocator interface that uses
/// a pw::Allocator.
///
/// Note that despite is name, this is NOT a pw::Allocator itself. Instead, it
/// can be used in `std::pmr` containers, such as `std::pmr::vector`.
///
/// See also https://en.cppreference.com/w/cpp/memory/polymorphic_allocator.
class AsPmrAllocator final : public StdPolymorphicAllocator {
 public:
  using Base = StdPolymorphicAllocator;

  AsPmrAllocator(Allocator& allocator) : Base(&memory_resource_) {
    memory_resource_.set_allocator(allocator);
  }

  Allocator& allocator() { return memory_resource_.allocator(); }

 private:
  internal::MemoryResource memory_resource_;
};

}  // namespace allocator
}  // namespace pw
