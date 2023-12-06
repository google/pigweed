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
#include <variant>

#include "lib/stdcompat/bit.h"
#include "pw_allocator/allocator.h"
#include "pw_allocator/block.h"
#include "pw_allocator/simple_allocator.h"
#include "pw_bytes/span.h"
#include "pw_containers/vector.h"
#include "pw_random/random.h"
#include "pw_unit_test/framework.h"

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
  ///
  /// If this method is called, then `Reset` MUST be called before the object is
  /// destroyed.
  Status Init(ByteSpan bytes);

  /// Allocates all the memory from this object.
  void Exhaust();

  /// Resets the recorded parameters to an initial state.
  void ResetParameters();

  /// Resets the object to an initial state.
  ///
  /// This frees all memory associated with this allocator and disassociates the
  /// allocator from its memory region. If `Init` is called, then this method
  /// MUST be called before the object is destroyed.
  void Reset();

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
  bool initialized_ = false;
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
  ~AllocatorForTestWithBuffer() { (*this)->Reset(); }
};

/// Represents a request to allocate some memory.
struct AllocationRequest {
  size_t size = 0;
  size_t alignment = 1;
};

/// Represents a request to free some allocated memory.
struct DeallocationRequest {
  size_t index = 0;
};

/// Represents a request to reallocate allocated memory with a new size.
struct ReallocationRequest {
  size_t index = 0;
  size_t new_size = 0;
};

using AllocatorRequest =
    std::variant<AllocationRequest, DeallocationRequest, ReallocationRequest>;

/// Associates an `Allocator` with a vector to store allocated pointers.
///
/// This class facilitates performing allocations from generated
/// `AllocatorRequest`s, enabling the creation of performance, stress, and fuzz
/// tests for various allocators.
///
/// This class lacks a public constructor, and so cannot be used directly.
/// Instead callers should use `WithAllocations`, which is templated on the
/// size of the vector used to store allocated pointers.
class AllocatorTestHarnessGeneric {
 public:
  /// Since this object has references passed to it that are typically owned by
  /// an object of a derived type, the destructor MUST NOT touch those
  /// references. Instead, it is the callers and/or the derived classes
  /// responsibility to call `Reset` before the object is destroyed, if desired.
  virtual ~AllocatorTestHarnessGeneric() = default;

  /// Generates and handles a sequence of allocation requests.
  ///
  /// This method will use the given PRNG to generate a `num_requests` and pass
  /// each in turn to `HandleRequest`. It will call `Reset` before returning.
  void GenerateRequests(random::RandomGenerator& prng,
                        size_t max_size,
                        size_t num_requests);

  /// Handles a sequence of allocation requests.
  ///
  /// This method is useful for processing externally generated requests, e.g.
  /// from FuzzTest. It will call `Reset` before returning.
  void HandleRequests(const Vector<AllocatorRequest>& requests);

  /// Handles an allocator request.
  ///
  /// This method is stateful, and modifies the vector of allocated pointers.
  /// It will call `Init` if it has not yet been called.
  ///
  /// If the request is an allocation request:
  /// * If the vector of previous allocations is full, ignores the request.
  /// * Otherwise, allocates memory and stores the pointer in the vector.
  ///
  /// If the request is a deallocation request:
  /// * If the vector of previous allocations is empty, ignores the request.
  /// * Otherwise, removes a pointer from the vector and deallocates it.
  ///
  /// If the request is a reallocation request:
  /// * If the vector of previous allocations is empty, reallocates a `nullptr`.
  /// * Otherwise, removes a pointer from the vector and reallocates it.
  void HandleRequest(const AllocatorRequest& request);

  /// Deallocates any pointers stored in the vector of allocated pointers.
  void Reset();

 protected:
  /// Associates a pointer to memory with the `Layout` used to allocate it.
  struct Allocation {
    void* ptr;
    Layout layout;
  };

  constexpr AllocatorTestHarnessGeneric(Vector<Allocation>& allocations)
      : allocations_(allocations) {}

 private:
  virtual Allocator* Init() = 0;

  /// Adds a pointer to the vector of allocated pointers.
  ///
  /// The `ptr` must not be null, and the vector of allocated pointers must not
  /// be full. To aid in detecting memory corruptions and in debugging, the
  /// pointed-at memory will be filled with as much of the following sequence as
  /// will fit:
  /// * The request number.
  /// * The request size.
  /// * The byte "0x5a", repeating.
  void AddAllocation(void* ptr, Layout layout);

  /// Removes and returns a previously allocated pointer.
  ///
  /// The vector of allocated pointers must not be empty.
  Allocation RemoveAllocation(size_t index);

  /// An allocator used to manage memory.
  Allocator* allocator_ = nullptr;

  /// A vector of allocated pointers.
  Vector<Allocation>& allocations_;

  /// The number of requests this object has handled.
  uint8_t num_requests_ = 0;
};

/// Associates an `Allocator` with a vector to store allocated pointers.
///
/// This class differes from its base class only in that it uses its template
/// parameter to explicitly size the vector used to store allocated pointers.
///
/// This class does NOT implement `WithAllocationsGeneric::Init`. It must be
/// extended further with a method that provides an initialized allocator.
template <size_t kMaxConcurrentAllocations>
class AllocatorTestHarness : public AllocatorTestHarnessGeneric {
 public:
  constexpr AllocatorTestHarness()
      : AllocatorTestHarnessGeneric(allocations_) {}

 private:
  Vector<Allocation, kMaxConcurrentAllocations> allocations_;
};

}  // namespace pw::allocator::test
