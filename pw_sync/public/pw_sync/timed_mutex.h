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
#include "pw_sync/lock_annotations.h"
#include "pw_sync/mutex.h"

#ifdef __cplusplus

namespace pw::sync {

// The TimedMutex is a synchronization primitive that can be used to protect
// shared data from being simultaneously accessed by multiple threads with
// timeouts and deadlines, extending the Mutex.
// It offers exclusive, non-recursive ownership semantics where priority
// inheritance is used to solve the classic priority-inversion problem.
// This is thread safe, but NOT IRQ safe.
//
// WARNING: In order to support global statically constructed TimedMutexes, the
// user and/or backend MUST ensure that any initialization required in your
// environment is done prior to the creation and/or initialization of the native
// synchronization primitives (e.g. kernel initialization).
class TimedMutex : public Mutex {
 public:
  TimedMutex() = default;
  ~TimedMutex() = default;
  TimedMutex(const TimedMutex&) = delete;
  TimedMutex(TimedMutex&&) = delete;
  TimedMutex& operator=(const TimedMutex&) = delete;
  TimedMutex& operator=(TimedMutex&&) = delete;

  // Attempts to lock the mutex where, if needed, blocking for at least the
  // specified duration.
  // Returns true if the mutex was successfully acquired.
  //
  // PRECONDITION:
  //   The lock isn't already held by this thread. Recursive locking is
  //   undefined behavior.
  bool try_lock_for(chrono::SystemClock::duration for_at_least)
      PW_EXCLUSIVE_TRYLOCK_FUNCTION(true);

  // Attempts to lock the mutex where, if needed, blocking until at least the
  // specified time_point.
  // Returns true if the mutex was successfully acquired.
  //
  // PRECONDITION:
  //   The lock isn't already held by this thread. Recursive locking is
  //   undefined behavior.
  bool try_lock_until(chrono::SystemClock::time_point until_at_least)
      PW_EXCLUSIVE_TRYLOCK_FUNCTION(true);
};

}  // namespace pw::sync

#include "pw_sync_backend/timed_mutex_inline.h"

using pw_sync_TimedMutex = pw::sync::TimedMutex;

#else  // !defined(__cplusplus)

typedef struct pw_sync_TimedMutex pw_sync_TimedMutex;

#endif  // __cplusplus

PW_EXTERN_C_START

void pw_sync_TimedMutex_Lock(pw_sync_TimedMutex* mutex)
    PW_NO_LOCK_SAFETY_ANALYSIS;
bool pw_sync_TimedMutex_TryLock(pw_sync_TimedMutex* mutex)
    PW_NO_LOCK_SAFETY_ANALYSIS;
bool pw_sync_TimedMutex_TryLockFor(pw_sync_TimedMutex* mutex,
                                   pw_chrono_SystemClock_Duration for_at_least)
    PW_NO_LOCK_SAFETY_ANALYSIS;
bool pw_sync_TimedMutex_TryLockUntil(
    pw_sync_TimedMutex* mutex,
    pw_chrono_SystemClock_TimePoint until_at_least) PW_NO_LOCK_SAFETY_ANALYSIS;
void pw_sync_TimedMutex_Unlock(pw_sync_TimedMutex* mutex)
    PW_NO_LOCK_SAFETY_ANALYSIS;

PW_EXTERN_C_END
