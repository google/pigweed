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

#include "pw_chrono/system_clock.h"
#include "pw_preprocessor/util.h"

#ifdef __cplusplus

namespace pw::this_thread {

// This can only be called from a thread, meaning the scheduler is running.
void sleep_for(chrono::SystemClock::duration for_at_least);

// This can only be called from a thread, meaning the scheduler is running.
void sleep_until(chrono::SystemClock::time_point until_at_least);

}  // namespace pw::this_thread

// The backend can opt to include inlined implementations.
#if __has_include("pw_thread_backend/sleep_inline.h")
#include "pw_thread_backend/sleep_inline.h"
#endif  // __has_include("pw_thread_backend/sleep_inline.h")

#endif  // __cplusplus

PW_EXTERN_C_START

void pw_this_thread_SleepFor(pw_chrono_SystemClock_TickCount for_at_least);
void pw_this_thread_SleepUntil(pw_chrono_SystemClock_TimePoint until_at_least);

PW_EXTERN_C_END
