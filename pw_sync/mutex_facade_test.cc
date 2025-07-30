// Copyright 2020 The Pigweed Authors
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

#include <chrono>

#include "pw_sync/mutex.h"
#include "pw_sync/test/borrow_testing.h"
#include "pw_unit_test/framework.h"

using pw::sync::Mutex;
using pw::sync::VirtualMutex;
using pw::sync::test::BorrowTest;

namespace {

extern "C" {

// Functions defined in mutex_facade_test_c.c which call the API from C.
void pw_sync_Mutex_CallLock(pw_sync_Mutex* mutex);
bool pw_sync_Mutex_CallTryLock(pw_sync_Mutex* mutex);
void pw_sync_Mutex_CallUnlock(pw_sync_Mutex* mutex);

}  // extern "C"

// TODO: b/235284163 - Add real concurrency tests once we have pw::thread.

TEST(MutexTest, LockUnlock) {
  Mutex mutex;
  mutex.lock();
  // TODO: b/235284163 - Ensure it fails to lock when already held.
  // EXPECT_FALSE(mutex.try_lock());
  mutex.unlock();
}

Mutex static_mutex;
TEST(MutexTest, LockUnlockStatic) {
  static_mutex.lock();
  // TODO: b/235284163 - Ensure it fails to lock when already held.
  // EXPECT_FALSE(static_mutex.try_lock());
  static_mutex.unlock();
}

TEST(MutexTest, TryLockUnlock) {
  Mutex mutex;
  const bool locked = mutex.try_lock();
  EXPECT_TRUE(locked);
  if (locked) {
    // TODO: b/235284163 - Ensure it fails to lock when already held.
    // EXPECT_FALSE(mutex.try_lock());
    mutex.unlock();
  }
}

// Unit tests for a `Borrowable`that uses a `Mutex` as its lock.
using MutexBorrowTest = BorrowTest<Mutex>;

TEST_F(MutexBorrowTest, Acquire) { TestAcquire(); }

TEST_F(MutexBorrowTest, ConstAcquire) { TestConstAcquire(); }

TEST_F(MutexBorrowTest, RepeatedAcquire) { TestRepeatedAcquire(); }

TEST_F(MutexBorrowTest, Moveable) { TestMoveable(); }

TEST_F(MutexBorrowTest, Copyable) { TestCopyable(); }

TEST_F(MutexBorrowTest, CopyableCovariant) { TestCopyableCovariant(); }

TEST_F(MutexBorrowTest, TryAcquireSuccess) { TestTryAcquireSuccess(); }

TEST_F(MutexBorrowTest, TryAcquireFailure) { TestTryAcquireFailure(); }

TEST(VirtualMutexTest, LockUnlock) {
  VirtualMutex mutex;
  mutex.lock();
  // TODO: b/235284163 - Ensure it fails to lock when already held.
  // EXPECT_FALSE(mutex.try_lock());
  mutex.unlock();
}

VirtualMutex static_virtual_mutex;
TEST(VirtualMutexTest, LockUnlockStatic) {
  static_virtual_mutex.lock();
  // TODO: b/235284163 - Ensure it fails to lock when already held.
  // EXPECT_FALSE(static_virtual_mutex.try_lock());
  static_virtual_mutex.unlock();
}

TEST(VirtualMutexTest, LockUnlockExternal) {
  VirtualMutex virtual_mutex;
  auto& mutex = virtual_mutex.mutex();
  mutex.lock();
  // TODO: b/235284163 - Ensure it fails to lock when already held.
  // EXPECT_FALSE(mutex.try_lock());
  mutex.unlock();
}

// Unit tests for a `Borrowable`that uses a `VirtualMutex` as its lock.
using VirtualMutexBorrowTest = BorrowTest<VirtualMutex>;

TEST_F(VirtualMutexBorrowTest, Acquire) { TestAcquire(); }

TEST_F(VirtualMutexBorrowTest, ConstAcquire) { TestConstAcquire(); }

TEST_F(VirtualMutexBorrowTest, RepeatedAcquire) { TestRepeatedAcquire(); }

TEST_F(VirtualMutexBorrowTest, Moveable) { TestMoveable(); }

TEST_F(VirtualMutexBorrowTest, Copyable) { TestCopyable(); }

TEST_F(VirtualMutexBorrowTest, CopyableCovariant) { TestCopyableCovariant(); }

TEST_F(VirtualMutexBorrowTest, TryAcquireSuccess) { TestTryAcquireSuccess(); }

TEST_F(VirtualMutexBorrowTest, TryAcquireFailure) { TestTryAcquireFailure(); }

TEST(MutexTest, LockUnlockInC) {
  Mutex mutex;
  pw_sync_Mutex_CallLock(&mutex);
  pw_sync_Mutex_CallUnlock(&mutex);
}

TEST(MutexTest, TryLockUnlockInC) {
  Mutex mutex;
  ASSERT_TRUE(pw_sync_Mutex_CallTryLock(&mutex));
  // TODO: b/235284163 - Ensure it fails to lock when already held.
  // EXPECT_FALSE(pw_sync_Mutex_CallTryLock(&mutex));
  pw_sync_Mutex_CallUnlock(&mutex);
}

}  // namespace
