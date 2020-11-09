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
#pragma once

#include <stdbool.h>

#include "pw_preprocessor/util.h"

#ifdef __cplusplus

#include "pw_sync_backend/spin_lock_native.h"

namespace pw::sync {

// The SpinLock is a synchronization primitive that can be used to protect
// shared data from being simultaneously accessed by multiple threads and/or
// IRQs as a targeted global lock (except for NMIs).
// It offers exclusive, non-recursive ownership semantics where IRQs up to a
// backend defined level of "NMIs" will be masked to solve priority-inversion.
//
// NOTE: This SpinLock relies on built-in local interrupt masking to make it IRQ
// safe without requiring the caller to mask interrupts manually when using this
// primitive.
//
// Unlike global interrupt locks, this also works safely and efficiently on SMP
// systems. This entire API is IRQ safe.
//
// WARNING: Code that holds a specific SpinLock must not try to re-acquire it
// or it will deadlock. However, it is okay to nest distinct spinlocks.
//
// WARNING: In order to support global statically constructed SpinLocks, the
// backend MUST ensure that any initialization required in your environment
// prior to the creation and/or initialization of the native semaphore
// (e.g. kernel initialization), is done before or during the invocation of the
// global static C++ constructors.
class SpinLock {
 public:
  using native_handle_type = backend::NativeSpinLockHandle;

  SpinLock();
  ~SpinLock() = default;
  SpinLock(const SpinLock&) = delete;
  SpinLock(SpinLock&&) = delete;
  SpinLock& operator=(const SpinLock&) = delete;
  SpinLock& operator=(SpinLock&&) = delete;

  // Locks the spinlock, blocking indefinitely. Failures are fatal.
  void lock();

  // Attempts to lock the spinlock in a non-blocking manner.
  // Returns true if the spinlock was successfully acquired.
  bool try_lock();

  // Unlocks the spinlock. Failures are fatal.
  void unlock();

  native_handle_type native_handle();

 private:
  // This may be a wrapper around a native type with additional members.
  backend::NativeSpinLock native_type_;
};

}  // namespace pw::sync

#include "pw_sync_backend/spin_lock_inline.h"

using pw_sync_SpinLock = pw::sync::SpinLock;

#else  // !defined(__cplusplus)

typedef struct pw_sync_SpinLock pw_sync_SpinLock;

#endif  // __cplusplus

PW_EXTERN_C_START

void pw_sync_SpinLock_Lock(pw_sync_SpinLock* spin_lock);
bool pw_sync_SpinLock_TryLock(pw_sync_SpinLock* spin_lock);
void pw_sync_SpinLock_Unlock(pw_sync_SpinLock* spin_lock);

PW_EXTERN_C_END
