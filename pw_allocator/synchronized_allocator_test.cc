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

#include "pw_allocator/synchronized_allocator.h"

#include <cstddef>
#include <cstdint>
#include <limits>

#include "pw_allocator/test_harness.h"
#include "pw_allocator/testing.h"
#include "pw_containers/vector.h"
#include "pw_random/xor_shift.h"
#include "pw_status/status_with_size.h"
#include "pw_sync/binary_semaphore.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/mutex.h"
#include "pw_thread/test_thread_context.h"
#include "pw_thread/thread.h"
#include "pw_thread/thread_core.h"
#include "pw_thread/yield.h"
#include "pw_unit_test/framework.h"

namespace {

// Test fixtures.

static constexpr size_t kCapacity = 8192;
static constexpr size_t kMaxSize = 512;
static constexpr size_t kNumAllocations = 128;
static constexpr size_t kSize = 64;
static constexpr size_t kAlignment = 4;
static constexpr size_t kBackgroundRequests = 8;

using ::pw::allocator::Layout;
using ::pw::allocator::SynchronizedAllocator;
using AllocatorForTest = ::pw::allocator::test::AllocatorForTest<kCapacity>;

struct Allocation {
  void* ptr;
  Layout layout;

  /// Fills a valid allocation with a pattern of data.
  void Paint() {
    if (ptr != nullptr) {
      auto* u8 = static_cast<uint8_t*>(ptr);
      for (size_t i = 0; i < layout.size(); ++i) {
        u8[i] = static_cast<uint8_t>(i & 0xFF);
      }
    }
  }

  /// Checks that a valid allocation has been properly `Paint`ed. Returns the
  /// first index where the expected pattern doesn't match, e.g. where memory
  /// has been corrupted, or `layout.size()` if the memory is all correct.
  size_t Inspect() const {
    if (ptr != nullptr) {
      auto* u8 = static_cast<uint8_t*>(ptr);
      for (size_t i = 0; i < layout.size(); ++i) {
        if (u8[i] != (i & 0xFF)) {
          return i;
        }
      }
    }
    return layout.size();
  }
};

/// Test fixture that manages a background allocation thread.
class Background final {
 public:
  Background(pw::Allocator& allocator)
      : Background(allocator, 1, std::numeric_limits<size_t>::max()) {}

  Background(pw::Allocator& allocator, uint64_t seed, size_t iterations)
      : background_(allocator, seed, iterations) {
    background_thread_ = pw::thread::Thread(context_.options(), background_);
  }

  ~Background() {
    background_.Stop();
    Await();
  }

  void Await() {
    background_.Await();
    if (background_thread_.joinable()) {
      background_thread_.join();
    }
  }

 private:
  struct TestHarness
      : public pw::allocator::test::TestHarness<kBackgroundRequests> {
    pw::Allocator* allocator = nullptr;
    pw::Allocator* Init() override { return allocator; }
  };

  /// Thread body that uses a test harness to perform random sequences of
  /// allocations on a synchronous allocator.
  class BackgroundThreadCore : public pw::thread::ThreadCore {
   public:
    BackgroundThreadCore(pw::Allocator& allocator,
                         uint64_t seed,
                         size_t iterations)
        : prng_(seed), iterations_(iterations) {
      test_harness_.allocator = &allocator;
    }

    void Stop() { semaphore_.release(); }

    void Await() {
      semaphore_.acquire();
      semaphore_.release();
    }

   private:
    void Run() override {
      for (size_t i = 0; i < iterations_ && !semaphore_.try_acquire(); ++i) {
        test_harness_.GenerateRequests(prng_, kMaxSize, kBackgroundRequests);
        pw::this_thread::yield();
      }
      semaphore_.release();
    }

    TestHarness test_harness_;
    pw::random::XorShiftStarRng64 prng_;
    pw::sync::BinarySemaphore semaphore_;
    size_t iterations_;
  } background_;

  pw::thread::test::TestThreadContext context_;
  pw::thread::Thread background_thread_;
};

// Unit tests.

// The tests below manipulate dynamically allocated memory while a background
// thread simultaneously exercises the allocator. Allocations, queries and
// resizes may fail, but memory must not be corrupted and the test must not
// deadlock.

template <typename LockType>
void TestGetCapacity() {
  AllocatorForTest allocator;
  SynchronizedAllocator<LockType> synchronized(allocator);
  Background background(synchronized);

  pw::StatusWithSize capacity = synchronized.GetCapacity();
  EXPECT_EQ(capacity.status(), pw::OkStatus());
  EXPECT_EQ(capacity.size(), kCapacity);
}

TEST(SynchronizedAllocatorTest, GetCapacitySpinLock) {
  TestGetCapacity<pw::sync::InterruptSpinLock>();
}

TEST(SynchronizedAllocatorTest, GetCapacityMutex) {
  TestGetCapacity<pw::sync::Mutex>();
}

template <typename LockType>
void TestAllocate() {
  AllocatorForTest allocator;
  SynchronizedAllocator<LockType> synchronized(allocator);
  Background background(synchronized);

  pw::Vector<Allocation, kNumAllocations> allocations;
  while (!allocations.full()) {
    Layout layout(kSize, kAlignment);
    void* ptr = synchronized.Allocate(layout);
    Allocation allocation{ptr, layout};
    allocation.Paint();
    allocations.push_back(allocation);
    pw::this_thread::yield();
  }

  for (const auto& allocation : allocations) {
    EXPECT_EQ(allocation.Inspect(), allocation.layout.size());
    synchronized.Deallocate(allocation.ptr);
  }
}

TEST(SynchronizedAllocatorTest, AllocateDeallocateInterruptSpinLock) {
  TestAllocate<pw::sync::InterruptSpinLock>();
}

TEST(SynchronizedAllocatorTest, AllocateDeallocateMutex) {
  TestAllocate<pw::sync::Mutex>();
}

template <typename LockType>
void TestResize() {
  AllocatorForTest allocator;
  SynchronizedAllocator<LockType> synchronized(allocator);
  Background background(synchronized);

  pw::Vector<Allocation, kNumAllocations> allocations;
  while (!allocations.full()) {
    Layout layout(kSize, kAlignment);
    void* ptr = synchronized.Allocate(layout);
    Allocation allocation{ptr, layout};
    allocation.Paint();
    allocations.push_back(allocation);
    pw::this_thread::yield();
  }

  // First, resize them smaller.
  for (auto& allocation : allocations) {
    EXPECT_EQ(allocation.Inspect(), allocation.layout.size());
    size_t new_size = allocation.layout.size() / 2;
    if (!synchronized.Resize(allocation.ptr, new_size)) {
      continue;
    }
    allocation.layout = Layout(new_size, allocation.layout.alignment());
    allocation.Paint();
    pw::this_thread::yield();
  }

  // Then, resize them back to their original size.
  for (auto& allocation : allocations) {
    EXPECT_EQ(allocation.Inspect(), allocation.layout.size());
    size_t old_size = allocation.layout.size() * 2;
    if (!synchronized.Resize(allocation.ptr, old_size)) {
      continue;
    }
    allocation.layout = Layout(old_size, allocation.layout.alignment());
    allocation.Paint();
    pw::this_thread::yield();
  }
}

TEST(SynchronizedAllocatorTest, ResizeInterruptSpinLock) {
  TestResize<pw::sync::InterruptSpinLock>();
}

TEST(SynchronizedAllocatorTest, ResizeMutex) { TestResize<pw::sync::Mutex>(); }

template <typename LockType>
void TestReallocate() {
  AllocatorForTest allocator;
  SynchronizedAllocator<LockType> synchronized(allocator);
  Background background(synchronized);

  pw::Vector<Allocation, kNumAllocations> allocations;
  while (!allocations.full()) {
    Layout layout(kSize, kAlignment);
    void* ptr = synchronized.Allocate(layout);
    Allocation allocation{ptr, layout};
    allocation.Paint();
    allocations.push_back(allocation);
    pw::this_thread::yield();
  }

  for (auto& allocation : allocations) {
    EXPECT_EQ(allocation.Inspect(), allocation.layout.size());
    Layout new_layout = allocation.layout.Extend(1);
    if (!synchronized.Reallocate(allocation.ptr, new_layout)) {
      continue;
    }
    allocation.layout = new_layout;
    allocation.Paint();
    pw::this_thread::yield();
  }
}

TEST(SynchronizedAllocatorTest, ReallocateInterruptSpinLock) {
  TestReallocate<pw::sync::InterruptSpinLock>();
}

TEST(SynchronizedAllocatorTest, ReallocateMutex) {
  TestReallocate<pw::sync::Mutex>();
}

template <typename LockType>
void TestGenerateRequests() {
  constexpr size_t kNumIterations = 10000;
  AllocatorForTest allocator;
  SynchronizedAllocator<LockType> synchronized(allocator);
  Background background1(synchronized, 1, kNumIterations);
  Background background2(synchronized, 2, kNumIterations);
  background1.Await();
  background2.Await();
}

TEST(SynchronizedAllocatorTest, GenerateRequestsSpinLock) {
  TestGenerateRequests<pw::sync::InterruptSpinLock>();
}

TEST(SynchronizedAllocatorTest, GenerateRequestsMutex) {
  TestGenerateRequests<pw::sync::Mutex>();
}

}  // namespace
