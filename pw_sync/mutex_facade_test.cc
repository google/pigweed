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
#include "pw_sync_private/borrow_lockable_tests.h"
#include "pw_unit_test/framework.h"

namespace pw::sync {
namespace {

extern "C" {

// Functions defined in mutex_facade_test_c.c which call the API from C.
void pw_sync_Mutex_CallLock(pw_sync_Mutex* mutex);
bool pw_sync_Mutex_CallTryLock(pw_sync_Mutex* mutex);
void pw_sync_Mutex_CallUnlock(pw_sync_Mutex* mutex);

}  // extern "C"

// TODO: b/235284163 - Add real concurrency tests once we have pw::thread.

TEST(Mutex, LockUnlock) {
  Mutex mutex;
  mutex.lock();
  // TODO: b/235284163 - Ensure it fails to lock when already held.
  // EXPECT_FALSE(mutex.try_lock());
  mutex.unlock();
}

Mutex static_mutex;
TEST(Mutex, LockUnlockStatic) {
  static_mutex.lock();
  // TODO: b/235284163 - Ensure it fails to lock when already held.
  // EXPECT_FALSE(static_mutex.try_lock());
  static_mutex.unlock();
}

TEST(Mutex, TryLockUnlock) {
  Mutex mutex;
  const bool locked = mutex.try_lock();
  EXPECT_TRUE(locked);
  if (locked) {
    // TODO: b/235284163 - Ensure it fails to lock when already held.
    // EXPECT_FALSE(mutex.try_lock());
    mutex.unlock();
  }
}

PW_SYNC_ADD_BORROWABLE_LOCK_NAMED_TESTS(BorrowableMutex, Mutex);

TEST(VirtualMutex, LockUnlock) {
  VirtualMutex mutex;
  mutex.lock();
  // TODO: b/235284163 - Ensure it fails to lock when already held.
  // EXPECT_FALSE(mutex.try_lock());
  mutex.unlock();
}

VirtualMutex static_virtual_mutex;
TEST(VirtualMutex, LockUnlockStatic) {
  static_virtual_mutex.lock();
  // TODO: b/235284163 - Ensure it fails to lock when already held.
  // EXPECT_FALSE(static_virtual_mutex.try_lock());
  static_virtual_mutex.unlock();
}

TEST(VirtualMutex, LockUnlockExternal) {
  VirtualMutex virtual_mutex;
  auto& mutex = virtual_mutex.mutex();
  mutex.lock();
  // TODO: b/235284163 - Ensure it fails to lock when already held.
  // EXPECT_FALSE(mutex.try_lock());
  mutex.unlock();
}

PW_SYNC_ADD_BORROWABLE_LOCK_NAMED_TESTS(BorrowableVirtualMutex, VirtualMutex);

TEST(Mutex, LockUnlockInC) {
  Mutex mutex;
  pw_sync_Mutex_CallLock(&mutex);
  pw_sync_Mutex_CallUnlock(&mutex);
}

TEST(Mutex, TryLockUnlockInC) {
  Mutex mutex;
  ASSERT_TRUE(pw_sync_Mutex_CallTryLock(&mutex));
  // TODO: b/235284163 - Ensure it fails to lock when already held.
  // EXPECT_FALSE(pw_sync_Mutex_CallTryLock(&mutex));
  pw_sync_Mutex_CallUnlock(&mutex);
}

}  // namespace
}  // namespace pw::sync
