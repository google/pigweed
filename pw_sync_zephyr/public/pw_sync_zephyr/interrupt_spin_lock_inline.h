// Copyright 2023 The Pigweed Authors
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

#include "pw_sync/interrupt_spin_lock.h"

namespace pw::sync {

constexpr InterruptSpinLock::InterruptSpinLock()
    : native_type_{.lock = {0}, .locked = false, .key = {0}} {}

inline bool InterruptSpinLock::try_lock() {
  // This backend will spin the current processor until the deadlock is
  // available and does not implement a 'try' mechanism. Recursive locking is
  // already detected by lock().
  lock();
  return true;
}

inline InterruptSpinLock::native_handle_type
InterruptSpinLock::native_handle() {
  return native_type_;
}

}  // namespace pw::sync
