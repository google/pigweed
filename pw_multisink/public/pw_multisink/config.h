// Copyright 2021 The Pigweed Authors
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

// Enum for different lock configurations.
// Use non-obvious numbers so users use the macro instead of an integer
#define PW_MULTISINK_INTERRUPT_SPIN_LOCK 100
#define PW_MULTISINK_MUTEX 200
#define PW_MULTISINK_VIRTUAL_LOCK 300

// PW_MULTISINK_CONFIG_LOCK_TYPE controls which lock is used when reading and
// writing from the underlying ring-buffer. The interrupt spin lock is
// enabled by default if PW_MULTISINK_CONFIG_LOCK_TYPE is not set. Otherwise,
// one of the following locking implementations can be set using the enums
// above. PW_MULTISINK_VIRTUAL_LOCK can be used if the user wants to provide
// their own locking implementation.
#ifndef PW_MULTISINK_CONFIG_LOCK_TYPE
// Backwards compatibility
#ifdef PW_MULTISINK_LOCK_INTERRUPT_SAFE
#warning "Multisink PW_MULTISINK_INTERRUPT_SPIN_LOCK is deprecated!!!"
#if PW_MULTISINK_LOCK_INTERRUPT_SAFE
#define PW_MULTISINK_CONFIG_LOCK_TYPE PW_MULTISINK_INTERRUPT_SPIN_LOCK
#else
#define PW_MULTISINK_CONFIG_LOCK_TYPE PW_MULTISINK_MUTEX
#endif
#else
// Default to use interrupt spin lock.
#define PW_MULTISINK_CONFIG_LOCK_TYPE PW_MULTISINK_INTERRUPT_SPIN_LOCK
#endif
#endif  // PW_MULTISINK_CONFIG_LOCK_TYPE

static_assert(PW_MULTISINK_CONFIG_LOCK_TYPE ==
                  PW_MULTISINK_INTERRUPT_SPIN_LOCK ||
              PW_MULTISINK_CONFIG_LOCK_TYPE == PW_MULTISINK_MUTEX ||
              PW_MULTISINK_CONFIG_LOCK_TYPE == PW_MULTISINK_VIRTUAL_LOCK);

#if PW_MULTISINK_CONFIG_LOCK_TYPE == PW_MULTISINK_INTERRUPT_SPIN_LOCK
#include "pw_sync/interrupt_spin_lock.h"
#elif PW_MULTISINK_CONFIG_LOCK_TYPE == PW_MULTISINK_VIRTUAL_LOCK
#include "pw_sync/virtual_basic_lockable.h"
#elif PW_MULTISINK_CONFIG_LOCK_TYPE == PW_MULTISINK_MUTEX
#include "pw_sync/mutex.h"
#endif  // PW_MULTISINK_CONFIG_LOCK_INTERRUPT_SAFE

namespace pw {
namespace multisink {

#if PW_MULTISINK_CONFIG_LOCK_TYPE == PW_MULTISINK_INTERRUPT_SPIN_LOCK
using LockType = pw::sync::InterruptSpinLock;
#elif PW_MULTISINK_CONFIG_LOCK_TYPE == PW_MULTISINK_VIRTUAL_LOCK
using LockType = pw::sync::VirtualBasicLockable&;
#elif PW_MULTISINK_CONFIG_LOCK_TYPE == PW_MULTISINK_MUTEX
using LockType = pw::sync::Mutex;
#endif  // PW_MULTISINK_CONFIG_LOCK_INTERRUPT_SAFE

}  // namespace multisink
}  // namespace pw
