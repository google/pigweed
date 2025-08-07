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

#include "pw_allocator/guarded_allocator.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>

#include "pw_allocator/first_fit_block_allocator.h"
#include "pw_allocator/sync_allocator_testing.h"
#include "pw_sync/interrupt_spin_lock.h"
#include "pw_sync/mutex.h"
#include "pw_unit_test/framework.h"

// TODO: https://pwbug.dev/365161669 - Express joinability as a build-system
// constraint.
#if PW_THREAD_JOINING_ENABLED

namespace {

// Test fixtures.

static constexpr size_t kCapacity = 8192;

using ::pw::allocator::GuardedAllocator;
using ::pw::allocator::test::Background;
using ::pw::allocator::test::BackgroundThreadCore;
using ::pw::allocator::test::SyncAllocatorTest;
using BlockAllocator = ::pw::allocator::FirstFitBlockAllocator<uint16_t>;

enum class Mode {
  kValidateOne,
  kValidateAll,
};

/// Thread body that validates a guarded allocator's blocks in the background.
template <typename GuardedAllocatorType>
class GuardedAllocatorTestThreadCore : public BackgroundThreadCore {
 public:
  GuardedAllocatorTestThreadCore(GuardedAllocatorType& allocator)
      : allocator_(allocator) {}

  void SetMode(Mode mode) {
    std::lock_guard lock(mutex_);
    mode_ = mode;
  }

  void* GetInvalid() const PW_LOCKS_EXCLUDED(mutex_) {
    std::lock_guard lock(mutex_);
    return invalid_;
  }

  /// Clobbers the byte at the given location and returns the original byte.
  ///
  /// This is done in a thread-safe manner to allow the allocator to detect the
  /// corruption without TSAN flagging it first.
  void Corrupt(uint8_t* ptr) PW_LOCKS_EXCLUDED(mutex_) {
    std::lock_guard lock(mutex_);
    original_ = *ptr;
    corrupted_ = ptr;
    *corrupted_ ^= 0xFF;
  }

  /// Restores a corrupted byte to its original value.
  void Restore() PW_LOCKS_EXCLUDED(mutex_) {
    std::lock_guard lock(mutex_);
    *corrupted_ = original_;
    invalid_ = nullptr;
  }

  bool RunOnce() PW_LOCKS_EXCLUDED(mutex_) override {
    std::lock_guard lock(mutex_);
    switch (mode_) {
      case Mode::kValidateOne:
        invalid_ = allocator_.ValidateOne();
        break;
      case Mode::kValidateAll:
        invalid_ = allocator_.ValidateAll();
        break;
    }
    return invalid_ == nullptr;
  }

 private:
  mutable ::pw::sync::Mutex mutex_;
  Mode mode_ = Mode::kValidateOne;
  void* invalid_ PW_GUARDED_BY(mutex_) = nullptr;
  uint8_t* corrupted_ PW_GUARDED_BY(mutex_) = nullptr;
  uint8_t original_ PW_GUARDED_BY(mutex_) = 0;
  GuardedAllocatorType& allocator_;
};

/// Test fixture responsible for managing a guarded allocator and a
/// background thread that accesses it concurrently with unit tests.
///
/// @tparam LockType  Synchronization type used by the allocator.
template <typename LockType>
class GuardedAllocatorTestBase : public SyncAllocatorTest {
 protected:
  using GuardedAllocatorType = GuardedAllocator<BlockAllocator, LockType>;
  using BlockType = BlockAllocator::BlockType;

  // This necessarily violates the encapsulation of GuardedAllocator in order
  // to precisely simulate overflows of a single byte. Keep it in sync with the
  // constant of the same name in guarded_allocator.cc
  static constexpr size_t kMinPrefixSize = sizeof(size_t) * 2;

  GuardedAllocatorTestBase() : guarded_(allocator_), core_(guarded_) {
    allocator_.Init(buffer_);
  }

  void SetMode(Mode mode) { core_.SetMode(mode); }

  GuardedAllocatorType& GetAllocator() override { return guarded_; }

  BackgroundThreadCore& GetCore() override { return core_; }

  void CheckValid() { EXPECT_EQ(core_.GetInvalid(), nullptr); }

  // Unit tests

  void TestValidateAllAfterInit() {
    core_.SetMode(Mode::kValidateAll);
    core_.RunOnce();
    CheckValid();
  }

  void TestValidateAllAfterAllocation() {
    pw::UniquePtr<uint8_t[]> bytes =
        guarded_.template MakeUniqueArray<uint8_t>(64);
    ASSERT_NE(bytes, nullptr);

    core_.SetMode(Mode::kValidateAll);
    core_.RunOnce();
    CheckValid();
  }

  void TestDetectHeapUnderrun() {
    Background background(core_);

    pw::UniquePtr<uint8_t[]> bytes =
        guarded_.template MakeUniqueArray<uint8_t>(64);
    ASSERT_NE(bytes, nullptr);

    // Modify the last byte of the prefix.
    core_.Corrupt(bytes.get() - 1);

    core_.Await();
    EXPECT_GE(core_.GetInvalid(), bytes.get() - kMinPrefixSize);
    EXPECT_LE(core_.GetInvalid(), bytes.get());
    core_.Restore();
  }

  void TestDetectHeapOverrunFromPrev() {
    Background background(core_);

    pw::UniquePtr<uint8_t[]> bytes =
        guarded_.template MakeUniqueArray<uint8_t>(64);
    ASSERT_NE(bytes, nullptr);

    // Modify the first byte of the prefix.
    core_.Corrupt(bytes.get() - kMinPrefixSize);

    core_.Await();
    EXPECT_GE(core_.GetInvalid(), bytes.get() - kMinPrefixSize);
    EXPECT_LE(core_.GetInvalid(), bytes.get());
    core_.Restore();
  }

  void TestDetectHeapOverrun() {
    Background background(core_);

    pw::UniquePtr<uint8_t[]> bytes =
        guarded_.template MakeUniqueArray<uint8_t>(64);
    ASSERT_NE(bytes, nullptr);

    // Modify the first byte of the suffix.
    core_.Corrupt(bytes.get() + 64);

    core_.Await();
    EXPECT_GE(core_.GetInvalid(), bytes.get() - kMinPrefixSize);
    EXPECT_LE(core_.GetInvalid(), bytes.get());
    core_.Restore();
  }

 private:
  alignas(BlockType::kAlignment) std::array<std::byte, kCapacity> buffer_;
  BlockAllocator allocator_;
  GuardedAllocatorType guarded_;
  GuardedAllocatorTestThreadCore<GuardedAllocatorType> core_;
};

using GuardedAllocatorInterruptSpinLockTest =
    GuardedAllocatorTestBase<::pw::sync::InterruptSpinLock>;
using GuardedAllocatorMutexTest = GuardedAllocatorTestBase<::pw::sync::Mutex>;

// Unit tests.

TEST_F(GuardedAllocatorInterruptSpinLockTest, GetCapacity) {
  TestGetCapacity(kCapacity);
  CheckValid();
}

TEST_F(GuardedAllocatorMutexTest, GetCapacity) {
  TestGetCapacity(kCapacity);
  CheckValid();
}

TEST_F(GuardedAllocatorInterruptSpinLockTest, AllocateDeallocate) {
  TestAllocate();
  CheckValid();
}

TEST_F(GuardedAllocatorMutexTest, AllocateDeallocate) {
  TestAllocate();
  CheckValid();
}

TEST_F(GuardedAllocatorInterruptSpinLockTest, Resize) {
  TestResize();
  CheckValid();
}

TEST_F(GuardedAllocatorMutexTest, Resize) {
  TestResize();
  CheckValid();
}

TEST_F(GuardedAllocatorInterruptSpinLockTest, Reallocate) {
  TestReallocate();
  CheckValid();
}

TEST_F(GuardedAllocatorMutexTest, Reallocate) {
  TestReallocate();
  CheckValid();
}

TEST_F(GuardedAllocatorInterruptSpinLockTest, ValidateAllAfterInit) {
  TestValidateAllAfterInit();
}

TEST_F(GuardedAllocatorMutexTest, ValidateAllAfterInit) {
  TestValidateAllAfterInit();
}

TEST_F(GuardedAllocatorInterruptSpinLockTest, ValidateAllAfterAllocation) {
  TestValidateAllAfterAllocation();
}

TEST_F(GuardedAllocatorMutexTest, ValidateAllAfterAllocation) {
  TestValidateAllAfterAllocation();
}

TEST_F(GuardedAllocatorInterruptSpinLockTest, DetectHeapUnderrun_ValidateOne) {
  SetMode(Mode::kValidateOne);
  TestDetectHeapUnderrun();
}

TEST_F(GuardedAllocatorInterruptSpinLockTest, DetectHeapUnderrun_ValidateAll) {
  SetMode(Mode::kValidateAll);
  TestDetectHeapUnderrun();
}

TEST_F(GuardedAllocatorMutexTest, DetectHeapUnderrun_ValidateOne) {
  SetMode(Mode::kValidateOne);
  TestDetectHeapUnderrun();
}

TEST_F(GuardedAllocatorMutexTest, DetectHeapUnderrun_ValidateAll) {
  SetMode(Mode::kValidateAll);
  TestDetectHeapUnderrun();
}

TEST_F(GuardedAllocatorInterruptSpinLockTest,
       DetectHeapOverrunFromPrev_ValidateOne) {
  SetMode(Mode::kValidateOne);
  TestDetectHeapOverrunFromPrev();
}

TEST_F(GuardedAllocatorInterruptSpinLockTest,
       DetectHeapOverrunFromPrev_ValidateAll) {
  SetMode(Mode::kValidateAll);
  TestDetectHeapOverrunFromPrev();
}

TEST_F(GuardedAllocatorMutexTest, DetectHeapOverrunFromPrev_ValidateOne) {
  SetMode(Mode::kValidateOne);
  TestDetectHeapOverrunFromPrev();
}

TEST_F(GuardedAllocatorMutexTest, DetectHeapOverrunFromPrev_ValidateAll) {
  SetMode(Mode::kValidateAll);
  TestDetectHeapOverrunFromPrev();
}

TEST_F(GuardedAllocatorInterruptSpinLockTest, DetectHeapOverrun_ValidateOne) {
  SetMode(Mode::kValidateOne);
  TestDetectHeapOverrun();
}

TEST_F(GuardedAllocatorInterruptSpinLockTest, DetectHeapOverrun_ValidateAll) {
  SetMode(Mode::kValidateAll);
  TestDetectHeapOverrun();
}

TEST_F(GuardedAllocatorMutexTest, DetectHeapOverrun_ValidateOne) {
  SetMode(Mode::kValidateOne);
  TestDetectHeapOverrun();
}

TEST_F(GuardedAllocatorMutexTest, DetectHeapOverrun_ValidateAll) {
  SetMode(Mode::kValidateAll);
  TestDetectHeapOverrun();
}

}  // namespace

#endif  // PW_THREAD_JOINING_ENABLED
