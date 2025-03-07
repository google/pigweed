// Copyright 2025 The Pigweed Authors
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

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "pw_allocator/layout.h"

namespace pw {

// Forward declaration to prevent including "allocator.h", which includes this
// file.
class Allocator;

namespace allocator::internal {

/// Object metadata used to track ref counts of shared and weak pointers.
///
/// This control block is associated with an object, and will destroy it when
/// no shared pointers remain. It will deallocate any associated memory when
/// no weak or shared pointers remain.
class ControlBlock final {
 public:
  enum class Action {
    /// On return from a `Decrement` method, no action is needed.
    kNone,

    /// On return from a `Decrement` method, no shared pointers remain and the
    /// associated object can be destroyed. Weak pointers remain, so the control
    /// block memory should not be deallocated.
    kExpire,

    /// On return from a `Decrement` method, no shared or weak pointers remain
    /// and the associated memory can be deallocated.
    kFree,
  };

  /// Factory method for allocating memory with an attached control block.
  ///
  /// If allocation, this call will return null.
  ///
  /// @param  allocator   Allocator to use to provide memory.
  /// @param  layout      Layout of an object to allocate and associate with a
  ///                     control block.
  /// @param  size        Number of objects if allocating an array, or 1.
  static ControlBlock* Create(Allocator* allocator, Layout layout, size_t size);

  /// Returns the allocator that allocated the control block.
  Allocator* allocator() const { return allocator_; }

  /// Returns a pointer to the memory location of the associated object.
  void* data() const { return data_; }

  /// Returns the number of underlying objects if the shared pointer is to an
  /// array type, otherwise returns 1.
  size_t size() const { return size_; }

  /// Returns the number of active shared pointers to the associated object.
  ///
  /// Although the count cannot be negative, this is returned as a signed
  /// integer in order to more closely align with `std::shared_ptr::use_count()`
  /// and `std::weak_ptr::use_count`.
  ///
  /// This method is thread-safe.
  int32_t num_shared() const noexcept;

  /// If the count of shared pointers is nonzero, increments the count of
  /// shared pointers atomically and returns true; otherwise returns false.
  ///
  /// This method is thread-safe.
  bool IncrementShared();

  /// If the count of either weak or shared pointers is nonzero, increments the
  /// count of weak pointers atomically and returns true; otherwise returns
  /// false.
  ///
  /// This method is thread-safe.
  bool IncrementWeak();

  /// Atomically decrements the count of shared pointers associated with this
  /// control block.
  ///
  /// This method is thread-safe.
  Action DecrementShared();

  /// Atomically decrements the count of weak pointers associated with this
  /// control block.
  ///
  /// This method is thread-safe.
  Action DecrementWeak();

 private:
  /// Creates a new control block with an initial shared pointer count of 1.
  constexpr ControlBlock(Allocator* allocator, void* data, size_t size)
      : allocator_(allocator),
        data_(data),
        size_(size),
        num_weak_and_shared_(Pack(1, 1)) {}

  /// Returns a count of weak pointers from a packed value.
  static constexpr uint16_t UnpackWeak(uint32_t num_weak_and_shared) {
    return num_weak_and_shared >> 16;
  }

  /// Returns a count of shared pointers from a packed value.
  static constexpr uint16_t UnpackShared(uint32_t num_weak_and_shared) {
    return num_weak_and_shared & 0xFFFF;
  }

  /// Returns a packed value that combines a count of weak pointers and a count
  /// of shared pointers.
  static constexpr uint32_t Pack(uint16_t num_weak, uint16_t num_shared) {
    return (static_cast<uint32_t>(num_weak) << 16) | (num_shared & 0xFFFF);
  }

  Allocator* allocator_;
  void* data_;
  size_t size_;

  // Combines 2 16-bit counts for weak and shared pointers, respectively.
  mutable std::atomic_uint32_t num_weak_and_shared_;
};

}  // namespace allocator::internal
}  // namespace pw
