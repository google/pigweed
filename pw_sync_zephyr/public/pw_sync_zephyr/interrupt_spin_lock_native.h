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

#include <zephyr/spinlock.h>

#include "pw_sync/interrupt_spin_lock.h"

namespace pw::sync::backend {

struct ZephyrSpinLock {
  struct k_spinlock lock;
  bool locked;  // Used to detect recursion.
  k_spinlock_key_t key;
};

using NativeInterruptSpinLock = struct ZephyrSpinLock;
using NativeInterruptSpinLockHandle = NativeInterruptSpinLock&;

}  // namespace pw::sync::backend
