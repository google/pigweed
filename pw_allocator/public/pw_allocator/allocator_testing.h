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
#pragma once

#include <array>
#include <cstddef>

#include "gtest/gtest.h"
#include "pw_allocator/allocator.h"
#include "pw_allocator/block.h"
#include "pw_allocator/simple_allocator.h"
#include "pw_bytes/span.h"

namespace pw::allocator::test {

/// Simple memory allocator for testing.
///
/// This allocator records the most recent parameters passed to the `Allocator`
/// interface methods, and returns them via accessors.
class AllocatorForTest : public Allocator {
 public:
  constexpr AllocatorForTest() = default;
  ~AllocatorForTest() override;

  size_t allocate_size() const { return allocate_size_; }
  void* deallocate_ptr() const { return deallocate_ptr_; }
  size_t deallocate_size() const { return deallocate_size_; }
  void* resize_ptr() const { return resize_ptr_; }
  size_t resize_old_size() const { return resize_old_size_; }
  size_t resize_new_size() const { return resize_new_size_; }

  /// Provides memory for the allocator to allocate from.
  Status Init(ByteSpan bytes);

  /// Allocates all the memory from this object.
  void Exhaust();

  /// Resets the recorded parameters to an initial state.
  void ResetParameters();

  /// This frees all memory associated with this allocator.
  void DeallocateAll();

 private:
  using BlockType = Block<>;

  /// @copydoc Allocator::Query
  Status DoQuery(const void* ptr, Layout layout) const override;

  /// @copydoc Allocator::Allocate
  void* DoAllocate(Layout layout) override;

  /// @copydoc Allocator::Deallocate
  void DoDeallocate(void* ptr, Layout layout) override;

  /// @copydoc Allocator::Resize
  bool DoResize(void* ptr, Layout layout, size_t new_size) override;

  SimpleAllocator allocator_;
  size_t allocate_size_ = 0;
  void* deallocate_ptr_ = nullptr;
  size_t deallocate_size_ = 0;
  void* resize_ptr_ = nullptr;
  size_t resize_old_size_ = 0;
  size_t resize_new_size_ = 0;
};

/// Wraps a default-constructed type a buffer holding a region of memory.
///
/// Although the type is arbitrary, the intended purpose of of this class is to
/// provide allocators with memory to use when testing.
///
/// This class uses composition instead of inheritance in order to allow the
/// wrapped type's destructor to reference the memory without risk of a
/// use-after-free. As a result, the specific methods of the wrapped type
/// are not directly accesible. Instead, they can be accessed using the `*` and
/// `->` operators, e.g.
///
/// @code{.cpp}
/// WithBuffer<MyAllocator, 256> allocator;
/// allocator->MethodSpecificToMyAllocator();
/// @endcode
///
/// Note that this class does NOT initialize the allocator, since initialization
/// is not specified as part of the `Allocator` interface and may vary from
/// allocator to allocator. As a result, typical usgae includes deriving a class
/// that initializes the wrapped allocator with the buffer in a constructor. See
/// `AllocatorForTestWithBuffer` below for an example.
///
/// @tparam   T             The wrapped object.
/// @tparam   kBufferSize   The size of the backing memory, in bytes.
/// @tparam   AlignType     Buffer memory will be aligned to this type's
///                         alignment boundary.
template <typename T, size_t kBufferSize, typename AlignType = uint8_t>
class WithBuffer {
 public:
  static constexpr size_t kCapacity = kBufferSize;

  std::byte* data() { return buffer_.data(); }
  size_t size() const { return buffer_.size(); }

  T& operator*() { return obj_; }
  T* operator->() { return &obj_; }

 private:
  alignas(AlignType) std::array<std::byte, kBufferSize> buffer_;
  T obj_;
};

/// An `AllocatorForTest` that is automatically initialized on construction.
template <size_t kBufferSize>
class AllocatorForTestWithBuffer
    : public WithBuffer<AllocatorForTest, kBufferSize> {
 public:
  AllocatorForTestWithBuffer() {
    EXPECT_EQ((*this)->Init(ByteSpan(this->data(), this->size())), OkStatus());
  }
};

}  // namespace pw::allocator::test
