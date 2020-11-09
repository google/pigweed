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
#include "pw_sync/mutex.h"

namespace pw::sync {

inline Mutex::Mutex() : native_type_() {}

inline Mutex::~Mutex() {}

inline void Mutex::lock() { native_type_.lock(); }

inline bool Mutex::try_lock() { return native_type_.try_lock(); }

inline bool Mutex::try_lock_for(chrono::SystemClock::duration for_at_least) {
  return native_type_.try_lock_for(for_at_least);
}

inline bool Mutex::try_lock_until(
    chrono::SystemClock::time_point until_at_least) {
  return native_type_.try_lock_until(until_at_least);
}

inline void Mutex::unlock() { native_type_.unlock(); }

inline Mutex::native_handle_type Mutex::native_handle() { return native_type_; }

}  // namespace pw::sync
