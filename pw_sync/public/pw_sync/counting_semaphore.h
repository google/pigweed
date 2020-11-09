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

#include <stddef.h>

#include "pw_chrono/system_clock.h"
#include "pw_preprocessor/util.h"

#ifdef __cplusplus

#include "pw_sync_backend/counting_semaphore_native.h"

namespace pw::sync {

// The CountingSemaphore is a synchronization primitive that can be used for
// counting events and/or resource management where receiver(s) can block on
// acquire until notifier(s) signal by invoking release.
// Note that unlike Mutexes, priority inheritance is not used by semaphores
// meaning semaphores are subject to unbounded priority inversions.
// Pigweed does not recommend semaphores for mutual exclusion. The entire API is
// thread safe but only a subset is IRQ safe.
//
// WARNING: In order to support global statically constructed
// CountingSemaphores, the backend MUST ensure that any initialization required
// in your environment prior to the creation and/or initialization of the native
// semaphore (e.g. kernel initialization), is done before or during the
// invocation of the global static C++ constructors.
class CountingSemaphore {
 public:
  using native_handle_type = backend::NativeCountingSemaphoreHandle;

  CountingSemaphore();
  ~CountingSemaphore();
  CountingSemaphore(const CountingSemaphore&) = delete;
  CountingSemaphore(CountingSemaphore&&) = delete;
  CountingSemaphore& operator=(const CountingSemaphore&) = delete;
  CountingSemaphore& operator=(CountingSemaphore&&) = delete;

  // Atomically increments the internal counter by the value of update.
  // Any thread(s) waiting for the counter to be greater than 0, i.e.
  // blocked in acquire, will subsequently be unblocked.
  // This is IRQ safe.
  //
  // PRECONDITIONS:
  //   update >= 0
  //   update <= max() - counter
  void release(ptrdiff_t update = 1);

  // Decrements the internal counter by 1 or blocks indefinitely until it can.
  // This is thread safe.
  void acquire();

  // Attempts to decrement by the internal counter by 1 without blocking.
  // Returns true if the internal counter was decremented successfully.
  // This is IRQ safe.
  bool try_acquire();

  // Attempts to decrement the internal counter by 1 where, if needed, blocking
  // for at least the specified duration.
  // Returns true if the internal counter was decremented successfully.
  // This is thread safe.
  bool try_acquire_for(chrono::SystemClock::duration for_at_least);

  // Attempts to decrement the internal counter by 1 where, if needed, blocking
  // until at least the specified time point.
  // Returns true if the internal counter was decremented successfully.
  // This is thread safe.
  bool try_acquire_until(chrono::SystemClock::time_point until_at_least);

  static constexpr ptrdiff_t max() {
    return backend::kCountingSemaphoreMaxValue;
  }

  native_handle_type native_handle();

 private:
  // This may be a wrapper around a native type with additional members.
  backend::NativeCountingSemaphore native_type_;
};

}  // namespace pw::sync

#include "pw_sync_backend/counting_semaphore_inline.h"

using pw_sync_CountingSemaphore = pw::sync::CountingSemaphore;

#else  // !defined(__cplusplus)

typedef struct pw_sync_CountingSemaphore pw_sync_CountingSemaphore;

#endif  // __cplusplus

PW_EXTERN_C_START

void pw_sync_CountingSemaphore_Release(pw_sync_CountingSemaphore* semaphore);
void pw_sync_CountingSemaphore_ReleaseNum(pw_sync_CountingSemaphore* semaphore,
                                          ptrdiff_t update);
void pw_sync_CountingSemaphore_Acquire(pw_sync_CountingSemaphore* semaphore);
bool pw_sync_CountingSemaphore_TryAcquire(pw_sync_CountingSemaphore* semaphore);
bool pw_sync_CountingSemaphore_TryAcquireFor(
    pw_sync_CountingSemaphore* semaphore,
    pw_chrono_SystemClock_TickCount for_at_least);
bool pw_sync_CountingSemaphore_TryAcquireUntil(
    pw_sync_CountingSemaphore* semaphore,
    pw_chrono_SystemClock_TimePoint until_at_least);
ptrdiff_t pw_sync_CountingSemaphore_Max(void);

PW_EXTERN_C_END
