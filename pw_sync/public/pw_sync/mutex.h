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

#include "pw_chrono/system_clock.h"
#include "pw_preprocessor/util.h"

#ifdef __cplusplus

#include "pw_sync_backend/mutex_native.h"

namespace pw::sync {

// The Mutex is a synchronization primitive that can be used to protect
// shared data from being simultaneously accessed by multiple threads.
// It offers exclusive, non-recursive ownership semantics where priority
// inheritance is used to solve the classic priority-inversion problem.
// This is thread safe, but NOT IRQ safe.
//
// WARNING: In order to support global statically constructed Mutex, the backend
// MUST ensure that any initialization required in your environment prior to the
// creation and/or initialization of the native semaphore (e.g. kernel
// initialization), is done before or during the invocation of the global static
// C++ constructors.
class Mutex {
 public:
  using native_handle_type = backend::NativeMutexHandle;

  Mutex();
  ~Mutex();
  Mutex(const Mutex&) = delete;
  Mutex(Mutex&&) = delete;
  Mutex& operator=(const Mutex&) = delete;
  Mutex& operator=(Mutex&&) = delete;

  // Locks the mutex, blocking indefinitely. Failures are fatal.
  void lock();

  // Attempts to lock the mutex in a non-blocking manner.
  // Returns true if the mutex was successfully acquired.
  bool try_lock();

  // Attempts to lock the mutex where, if needed, blocking for at least the
  // specified duration.
  // Returns true if the mutex was successfully acquired.
  bool try_lock_for(chrono::SystemClock::duration for_at_least);

  // Attempts to lock the mutex where, if needed, blocking until at least the
  // specified time_point.
  // Returns true if the mutex was successfully acquired.
  bool try_lock_until(chrono::SystemClock::time_point until_at_least);

  // Unlocks the mutex. Failures are fatal.
  void unlock();

  native_handle_type native_handle();

 private:
  // This may be a wrapper around a native type with additional members.
  backend::NativeMutex native_type_;
};

}  // namespace pw::sync

#include "pw_sync_backend/mutex_inline.h"

using pw_sync_Mutex = pw::sync::Mutex;

#else  // !defined(__cplusplus)

typedef struct pw_sync_Mutex pw_sync_Mutex;

#endif  // __cplusplus

PW_EXTERN_C_START

void pw_sync_Mutex_Lock(pw_sync_Mutex* mutex);
bool pw_sync_Mutex_TryLock(pw_sync_Mutex* mutex);
bool pw_sync_Mutex_TryLockFor(pw_sync_Mutex* mutex,
                              pw_chrono_SystemClock_TickCount for_at_least);
bool pw_sync_Mutex_TryLockUntil(pw_sync_Mutex* mutex,
                                pw_chrono_SystemClock_TimePoint until_at_least);
void pw_sync_Mutex_Unlock(pw_sync_Mutex* mutex);

PW_EXTERN_C_END
