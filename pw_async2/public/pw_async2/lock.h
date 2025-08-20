// Copyright 2025 The Pigweed Authors
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
#include "pw_toolchain/no_destructor.h"

namespace pw::async2::impl {

/// @submodule{pw_async2,backends}

/// A lock guarding the `Task` queue and `Waker` lists. This is a `Dispatcher`
/// implementation detail and should only be used by `Dispatcher` backends.
///
/// This is an `InterruptSpinLock` in order to allow posting work from ISR
/// contexts.
///
/// This lock is global rather than per-dispatcher in order to allow `Task` and
/// `Waker` to take out the lock without dereferencing their `Dispatcher*`
/// fields, which are themselves guarded by the lock in order to allow the
/// `Dispatcher` to `Deregister` itself upon destruction.
inline pw::sync::InterruptSpinLock& dispatcher_lock() {
  static NoDestructor<pw::sync::InterruptSpinLock> lock;
  return *lock;
}

/// @}

}  // namespace pw::async2::impl
