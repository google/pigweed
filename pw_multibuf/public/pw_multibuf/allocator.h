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

#include <optional>

#include "pw_async2/dispatcher.h"
#include "pw_multibuf/multibuf.h"
#include "pw_result/result.h"
#include "pw_sync/interrupt_spin_lock.h"

namespace pw::multibuf {

namespace internal {
class AllocationWaiter;
}  // namespace internal

class MultiBufAllocationFuture;

/// Interface for allocating ``MultiBuf`` objects.
///
/// A ``MultiBufAllocator`` differs from a regular ``pw::allocator::Allocator``
/// in that they may provide support for:
/// - Asynchronous allocation.
/// - Non-contiguous buffer allocation.
/// - Internal header/footer reservation.
/// - Size-range allocation.
///
/// In order to accomplish this, they return ``MultiBuf`` objects rather than
/// arbitrary pieces of memory.
///
/// Additionally, ``MultiBufAllocator`` implementations may choose to store
/// their allocation metadata separately from the data itself. This allows for
/// things like allocation headers to be kept out of restricted DMA-capable or
/// shared-memory regions.
///
/// NOTE: ``MultiBufAllocator``s *must* outlive any futures created from them.
class MultiBufAllocator {
 public:
  MultiBufAllocator() = default;

  /// ```MultiBufAllocator`` is not copyable or movable.
  MultiBufAllocator(MultiBufAllocator&) = delete;
  MultiBufAllocator& operator=(MultiBufAllocator&) = delete;
  MultiBufAllocator(MultiBufAllocator&&) = delete;
  MultiBufAllocator& operator=(MultiBufAllocator&&) = delete;

  virtual ~MultiBufAllocator() {}

  ////////////////
  // -- Sync -- //
  ////////////////

  /// Attempts to allocate a ``MultiBuf`` of exactly ``size`` bytes.
  ///
  /// Memory allocated by an arbitrary ``MultiBufAllocator`` does not provide
  /// any alignment requirments, preferring instead to allow the allocator
  /// maximum flexibility for placing regions (especially discontiguous
  /// regions).
  ///
  /// @retval ``MultiBuf`` if the allocation was successful.
  /// @retval ``nullopt_t`` if the memory is not currently available.
  std::optional<MultiBuf> Allocate(size_t size);

  /// Attempts to allocate a ``MultiBuf`` of at least ``min_size`` bytes and at
  /// most ``desired_size`` bytes.
  ///
  /// Memory allocated by an arbitrary ``MultiBufAllocator`` does not provide
  /// any alignment requirments, preferring instead to allow the allocator
  /// maximum flexibility for placing regions (especially discontiguous
  /// regions).
  ///
  /// @retval ``MultiBuf`` if the allocation was successful.
  /// @retval ``nullopt_t`` if the memory is not currently available.
  std::optional<MultiBuf> Allocate(size_t min_size, size_t desired_size);

  /// Attempts to allocate a contiguous ``MultiBuf`` of exactly ``size``
  /// bytes.
  ///
  /// Memory allocated by an arbitrary ``MultiBufAllocator`` does not provide
  /// any alignment requirments, preferring instead to allow the allocator
  /// maximum flexibility for placing regions (especially discontiguous
  /// regions).
  ///
  /// @retval ``MultiBuf`` with a single ``Chunk`` if the allocation was
  /// successful.
  /// @retval ``nullopt_t`` if the memory is not currently available.
  std::optional<MultiBuf> AllocateContiguous(size_t size);

  /// Attempts to allocate a contiguous ``MultiBuf`` of at least ``min_size``
  /// bytes and at most ``desired_size`` bytes.
  ///
  /// Memory allocated by an arbitrary ``MultiBufAllocator`` does not provide
  /// any alignment requirments, preferring instead to allow the allocator
  /// maximum flexibility for placing regions (especially discontiguous
  /// regions).
  ///
  /// @retval ``MultiBuf`` with a single ``Chunk`` if the allocation was
  /// successful.
  /// @retval ``nullopt_t`` if the memory is not currently available.
  std::optional<MultiBuf> AllocateContiguous(size_t min_size,
                                             size_t desired_size);

  /////////////////
  // -- Async -- //
  /////////////////

  /// Asynchronously allocates a ``MultiBuf`` of exactly ``size`` bytes.
  ///
  /// Memory allocated by an arbitrary ``MultiBufAllocator`` does not provide
  /// any alignment requirments, preferring instead to allow the allocator
  /// maximum flexibility for placing regions (especially discontiguous
  /// regions).
  ///
  /// @retval A ``MultiBufAllocationFuture`` which will yield a ``MultiBuf``
  /// when one is available.
  MultiBufAllocationFuture AllocateAsync(size_t size);

  /// Asynchronously allocates a ``MultiBuf`` of at least
  /// ``min_size`` bytes and at most ``desired_size` bytes.
  ///
  /// Memory allocated by an arbitrary ``MultiBufAllocator`` does not provide
  /// any alignment requirments, preferring instead to allow the allocator
  /// maximum flexibility for placing regions (especially discontiguous
  /// regions).
  ///
  /// @retval A ``MultiBufAllocationFuture`` which will yield a ``MultiBuf``
  /// when one is available.
  MultiBufAllocationFuture AllocateAsync(size_t min_size, size_t desired_size);

  /// Asynchronously allocates a contiguous ``MultiBuf`` of exactly ``size``
  /// bytes.
  ///
  /// Memory allocated by an arbitrary ``MultiBufAllocator`` does not provide
  /// any alignment requirments, preferring instead to allow the allocator
  /// maximum flexibility for placing regions (especially discontiguous
  /// regions).
  ///
  /// @retval A ``MultiBufAllocationFuture`` which will yield an ``MultiBuf``
  /// consisting of a single ``Chunk`` when one is available.
  MultiBufAllocationFuture AllocateContiguousAsync(size_t size);

  /// Asynchronously allocates an ``OwnedChunk`` of at least
  /// ``min_size`` bytes and at most ``desired_size`` bytes.
  ///
  /// @retval A ``MultiBufAllocationFuture`` which will yield an ``MultiBuf``
  /// consisting of a single ``Chunk`` when one is available.
  MultiBufAllocationFuture AllocateContiguousAsync(size_t min_size,
                                                   size_t desired_size);

 protected:
  /// Awakens callers asynchronously waiting for allocations of at most
  /// ``size_available`` bytes or at most ``contiguous_size_available``
  /// contiguous bytes.
  ///
  /// This function should be invoked by implementations of
  /// ``MultiBufAllocator`` when more memory becomes available to allocate.
  void MoreMemoryAvailable(size_t size_available,
                           size_t contiguous_size_available);

 private:
  friend class MultiBufAllocationFuture;
  friend class internal::AllocationWaiter;

  /// Attempts to allocate a ``MultiBuf`` of at least ``min_size`` bytes and at
  /// most ``desired_size`` bytes.
  ///
  /// @returns @rst
  ///
  /// .. pw-status-codes::
  ///
  ///    OK: Returns the buffer if the allocation was successful.
  ///
  ///    RESOURCE_EXHAUSTED: Insufficient memory is available currently.
  ///
  ///    OUT_OF_RANGE: This amount of memory will not become possible to
  ///    allocate in the future, or this allocator is unable to signal via
  ///    ``MoreMemoryAvailable`` (this will result in asynchronous allocations
  ///    failing immediately on OOM).
  ///
  /// @endrst
  virtual pw::Result<MultiBuf> DoAllocate(size_t min_size,
                                          size_t desired_size,
                                          bool needs_contiguous) = 0;

  void AddWaiter(internal::AllocationWaiter*) PW_LOCKS_EXCLUDED(lock_);
  void AddWaiterLocked(internal::AllocationWaiter*)
      PW_EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void RemoveWaiter(internal::AllocationWaiter*) PW_LOCKS_EXCLUDED(lock_);
  void RemoveWaiterLocked(internal::AllocationWaiter*)
      PW_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  sync::InterruptSpinLock lock_;
  internal::AllocationWaiter* first_waiter_ PW_GUARDED_BY(lock_) = nullptr;
};

namespace internal {

/// An object used to receive notifications once a certain amount of memory
/// becomes available within a ``MultiBufAllocator``.
///
/// When attempting to allocate memory from a ``MultiBufAllocator``,
/// allocations may fail due to currently-insufficient memory.
/// If this happens, the caller may want to wait for the memory to become
/// available in the future.
///
/// AllocationWaiter stores the requirements of the allocation along with
/// a ``Waker`` object. When the associated ``MultiBufAllocator`` regains
/// sufficient memory in order to allow the allocation to proceed
/// (triggered by a call to ``MoreMemoryAvailable``) the ``MultiBufAllocator``
/// will wake any ``AllocationWaiter`` that may now be able to succeed.
class AllocationWaiter {
 public:
  /// Creates a new ``AllocationWaiter`` which waits for ``min_size`` bytes to
  /// come available in ``allocator``.
  AllocationWaiter(MultiBufAllocator& allocator,
                   size_t min_size,
                   size_t desired_size,
                   bool needs_contiguous)
      : allocator_(&allocator),
        waker_(),
        next_(nullptr),
        min_size_(min_size),
        desired_size_(desired_size),
        needs_contiguous_(needs_contiguous) {}

  AllocationWaiter(AllocationWaiter&&);
  AllocationWaiter& operator=(AllocationWaiter&&);
  ~AllocationWaiter();

  /// Returns whether or not an allocation should be attempted.
  ///
  /// Allocations should be attempted when the waiter is first created, then
  /// once each time the ``AllocationWaiter`` is notified that sufficient
  /// bytes have become available.
  bool ShouldAttemptAllocate() const { return waker_.IsEmpty(); }

  /// Un-registers the current ``Waker``.
  void ClearWaker() { waker_.Clear(); }

  /// Sets a new ``Waker`` to receive wakeups when this ``AllocationWaiter`` is
  /// awoken.
  void SetWaker(async2::Waker&& waker) { waker_ = std::move(waker); }

  /// Returns the ``allocator`` associated with this ``AllocationWaiter``.
  MultiBufAllocator& allocator() { return *allocator_; }

  /// Returns the ``min_size`` this ``AllocationWaiter`` is waiting for.
  size_t min_size() const { return min_size_; }

  /// Returns the ``desired_size`` this ``AllocationWaiter`` is hoping for.
  size_t desired_size() const { return desired_size_; }

  /// Return whether this ``AllocationWaker`` is waiting for contiguous
  /// sections of memory only.
  size_t needs_contiguous() const { return needs_contiguous_; }

 private:
  friend class ::pw::multibuf::MultiBufAllocator;

  // The allocator this waiter is tied to.
  //
  // This must only be mutated in either the constructor, move constructor,
  // or move assignment, and only after removing this waiter from the
  // ``allocator_``'s list so that the ``allocator_`` does not try to access the
  // waiter.
  MultiBufAllocator* allocator_;

  // The waker to wake when a suitably-sized allocation becomes available.
  // The ``Waker`` class is thread-safe, so mutations to this value can occur
  // without additional synchronization.
  async2::Waker waker_;

  // Pointer to the next ``AllocationWaiter`` waiting for an allocation from
  // ``allocator_``.
  AllocationWaiter* next_ PW_GUARDED_BY(allocator_->lock_);

  // The properties of the kind of allocation being waited for.
  //
  // These values must only be mutated in either the constructor, move
  // constructor, or move assignment, and only after removing this waiter from
  // the ``allocator_``'s list so that the ``allocator_`` does not try to access
  // them while they are being mutated.
  size_t min_size_;
  size_t desired_size_;
  bool needs_contiguous_;
};

}  // namespace internal

/// An object that asynchronously yields a ``MultiBuf`` when ``Pend``ed.
///
/// See ``pw::async2`` for details on ``Pend`` and how it is used to build
/// asynchronous tasks.
class MultiBufAllocationFuture {
 public:
  MultiBufAllocationFuture(MultiBufAllocator& allocator,
                           size_t min_size,
                           size_t desired_size,
                           bool needs_contiguous)
      : waiter_(allocator, min_size, desired_size, needs_contiguous) {}
  async2::Poll<std::optional<MultiBuf>> Pend(async2::Context& cx);
  size_t min_size() const { return waiter_.min_size(); }
  size_t desired_size() const { return waiter_.desired_size(); }
  bool needs_contiguous() const { return waiter_.needs_contiguous(); }

 private:
  friend class MultiBufAllocator;
  internal::AllocationWaiter waiter_;

  async2::Poll<std::optional<MultiBuf>> TryAllocate();
};

}  // namespace pw::multibuf
