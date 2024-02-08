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

#include "pw_allocator/allocator_test_harness.h"
#include "pw_allocator/allocator_testing.h"
#include "pw_containers/vector.h"
#include "pw_random/xor_shift.h"
#include "pw_status/status_with_size.h"
#include "pw_sync/binary_semaphore.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/mutex.h"
#include "pw_sync/virtual_basic_lockable.h"
#include "pw_thread/test_thread_context.h"
#include "pw_thread/thread.h"
#include "pw_thread/thread_core.h"
#include "pw_thread/yield.h"
#include "pw_unit_test/framework.h"

namespace pw::allocator {
namespace {

static constexpr size_t kCapacity = 8192;
static constexpr size_t kMaxSize = 512;
static constexpr size_t kNumAllocations = 128;
static constexpr size_t kSize = 64;
static constexpr size_t kAlignment = 4;
static constexpr size_t kBackgroundRequests = 8;

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
  Background(Allocator& allocator) : background_(allocator) {
    background_thread_ = thread::Thread(context_.options(), background_);
  }

  ~Background() {
    background_.Stop();
    if (background_thread_.joinable()) {
      background_thread_.join();
    }
  }

 private:
  struct TestHarness : public test::AllocatorTestHarness<kBackgroundRequests> {
    Allocator* allocator = nullptr;
    Allocator* Init() override { return allocator; }
  };

  /// Thread body that uses a test harness to perform random sequences of
  /// allocations on a synchronous allocator.
  class BackgroundThreadCore : public thread::ThreadCore {
   public:
    BackgroundThreadCore(Allocator& allocator) : prng_(1) {
      test_harness_.allocator = &allocator;
    }

    void Stop() { semaphore_.release(); }

   private:
    void Run() override {
      while (!semaphore_.try_acquire()) {
        test_harness_.GenerateRequests(prng_, kMaxSize, kBackgroundRequests);
      }
    }

    TestHarness test_harness_;
    random::XorShiftStarRng64 prng_;
    sync::BinarySemaphore semaphore_;
  } background_;

  thread::test::TestThreadContext context_;
  thread::Thread background_thread_;
};

// Unit tests.

// The tests below manipulate dynamically allocated memory while a background
// thread simultaneously exercises the allocator. Allocations, queries and
// resizes may fail, but memory must not be corrupted and the test must not
// deadlock.

template <typename LockType>
void TestAllocate() {
  test::AllocatorForTest<kCapacity> allocator;
  SynchronizedAllocator<LockType> sync_proxy(*(allocator.get()));
  Background background(sync_proxy);

  Vector<Allocation, kNumAllocations> allocations;
  while (!allocations.full()) {
    Layout layout(kSize, kAlignment);
    void* ptr = sync_proxy.Allocate(layout);
    Allocation allocation{ptr, layout};
    allocation.Paint();
    allocations.push_back(allocation);
    this_thread::yield();
  }

  for (const auto& allocation : allocations) {
    EXPECT_EQ(allocation.Inspect(), allocation.layout.size());
    sync_proxy.Deallocate(allocation.ptr, allocation.layout);
  }
}

TEST(SynchronizedAllocatorTest, AllocateDeallocateInterruptSpinLock) {
  TestAllocate<sync::InterruptSpinLock>();
}

TEST(SynchronizedAllocatorTest, AllocateDeallocateMutex) {
  TestAllocate<sync::Mutex>();
}

template <typename LockType>
void TestQuery() {
  test::AllocatorForTest<kCapacity> allocator;
  SynchronizedAllocator<LockType> sync_proxy(*(allocator.get()));
  Background background(sync_proxy);

  Vector<Allocation, kNumAllocations> allocations;
  while (!allocations.full()) {
    Layout layout(kSize, kAlignment);
    void* ptr = sync_proxy.Allocate(layout);
    Allocation allocation{ptr, layout};
    allocation.Paint();
    allocations.push_back(allocation);
    this_thread::yield();
  }

  for (const auto& allocation : allocations) {
    if (allocation.ptr != nullptr) {
      EXPECT_EQ(sync_proxy.Query(allocation.ptr, allocation.layout),
                OkStatus());
    }
    this_thread::yield();
  }
}

TEST(SynchronizedAllocatorTest, QueryInterruptSpinLock) {
  TestQuery<sync::InterruptSpinLock>();
}

TEST(SynchronizedAllocatorTest, QueryMutex) { TestQuery<sync::Mutex>(); }

template <typename LockType>
void TestResize() {
  test::AllocatorForTest<kCapacity> allocator;
  SynchronizedAllocator<LockType> sync_proxy(*(allocator.get()));
  Background background(sync_proxy);

  Vector<Allocation, kNumAllocations> allocations;
  while (!allocations.full()) {
    Layout layout(kSize, kAlignment);
    void* ptr = sync_proxy.Allocate(layout);
    Allocation allocation{ptr, layout};
    allocation.Paint();
    allocations.push_back(allocation);
    this_thread::yield();
  }

  // First, resize them smaller.
  for (auto& allocation : allocations) {
    EXPECT_EQ(allocation.Inspect(), allocation.layout.size());
    size_t new_size = allocation.layout.size() / 2;
    if (!sync_proxy.Resize(allocation.ptr, allocation.layout, new_size)) {
      continue;
    }
    allocation.layout = Layout(new_size, allocation.layout.alignment());
    allocation.Paint();
    this_thread::yield();
  }

  // Then, resize them back to their original size.
  for (auto& allocation : allocations) {
    EXPECT_EQ(allocation.Inspect(), allocation.layout.size());
    size_t old_size = allocation.layout.size() * 2;
    if (!sync_proxy.Resize(allocation.ptr, allocation.layout, old_size)) {
      continue;
    }
    allocation.layout = Layout(old_size, allocation.layout.alignment());
    allocation.Paint();
    this_thread::yield();
  }
}

TEST(SynchronizedAllocatorTest, ResizeInterruptSpinLock) {
  TestResize<sync::InterruptSpinLock>();
}

TEST(SynchronizedAllocatorTest, ResizeMutex) { TestResize<sync::Mutex>(); }

}  // namespace
}  // namespace pw::allocator
