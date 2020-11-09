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

#include "gtest/gtest.h"
#include "pw_sync/spin_lock.h"

namespace pw::sync {
namespace {

extern "C" {

// Functions defined in spin_lock_facade_test_c.c which call the API from C.
void pw_sync_SpinLock_CallLock(pw_sync_SpinLock* spin_lock);
bool pw_sync_SpinLock_CallTryLock(pw_sync_SpinLock* spin_lock);
void pw_sync_SpinLock_CallUnlock(pw_sync_SpinLock* spin_lock);

}  // extern "C"

TEST(SpinLock, LockUnlock) {
  pw::sync::SpinLock spin_lock;
  spin_lock.lock();
  spin_lock.unlock();
}

// TODO(pwbug/291): Add real concurrency tests once we have pw::thread.

SpinLock static_spin_lock;
TEST(SpinLock, LockUnlockStatic) {
  static_spin_lock.lock();
  // Ensure it fails to lock when already held.
  EXPECT_FALSE(static_spin_lock.try_lock());
  static_spin_lock.unlock();
}

TEST(SpinLock, TryLockUnlock) {
  pw::sync::SpinLock spin_lock;
  ASSERT_TRUE(spin_lock.try_lock());
  // Ensure it fails to lock when already held.
  EXPECT_FALSE(spin_lock.try_lock());
  spin_lock.unlock();
}

TEST(SpinLock, LockUnlockInC) {
  pw::sync::SpinLock spin_lock;
  pw_sync_SpinLock_CallLock(&spin_lock);
  pw_sync_SpinLock_CallUnlock(&spin_lock);
}

TEST(SpinLock, TryLockUnlockInC) {
  pw::sync::SpinLock spin_lock;
  ASSERT_TRUE(pw_sync_SpinLock_CallTryLock(&spin_lock));
  // Ensure it fails to lock when already held.
  EXPECT_FALSE(pw_sync_SpinLock_CallTryLock(&spin_lock));
  pw_sync_SpinLock_CallUnlock(&spin_lock);
}

}  // namespace
}  // namespace pw::sync
