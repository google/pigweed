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

#include <cstddef>
#include <variant>

#include "pw_allocator/allocator.h"
#include "pw_containers/vector.h"
#include "pw_random/random.h"

namespace pw::allocator::test {

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

/// Helper function to produce a valid alignment for a given `size` from an
/// arbitrary left shift amount.
size_t AlignmentFromLShift(size_t lshift, size_t size);

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
  size_t num_requests_ = 0;
};

/// Associates an `Allocator` with a vector to store allocated pointers.
///
/// This class differes from its base class only in that it uses its template
/// parameter to explicitly size the vector used to store allocated pointers.
///
/// This class does NOT implement `WithAllocationsGeneric::Init`. It must be
/// extended further with a method that provides an initialized allocator.
///
/// For example, one create a fuzzer for `MyAllocator` that verifies it never
/// crashes by adding the following class, function, and macro:
/// @code{.cpp}
///   constexpr size_t kMaxRequests = 256;
///   constexpr size_t kMaxAllocations = 128;
///   constexpr size_t kMaxSize = 2048;
///
///   class MyAllocatorFuzzer : public AllocatorTestHarness<kMaxAllocations> {
///    private:
///     Allocator* Init() override { return &allocator_; }
///     MyAllocator allocator_;
///   };
///
///   void MyAllocatorNeverCrashes(const Vector<AllocatorRequest>& requests) {
///     static MyAllocatorFuzzer fuzzer;
///     fuzzer.HandleRequests(requests);
///   }
///
///   FUZZ_TEST(MyAllocator, MyAllocatorNeverCrashes)
///     .WithDomains(ArbitraryAllocatorRequests<kMaxRequests, kMaxSize>());
/// @endcode
template <size_t kMaxConcurrentAllocations>
class AllocatorTestHarness : public AllocatorTestHarnessGeneric {
 public:
  constexpr AllocatorTestHarness()
      : AllocatorTestHarnessGeneric(allocations_) {}

 private:
  Vector<Allocation, kMaxConcurrentAllocations> allocations_;
};

}  // namespace pw::allocator::test
