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
use core::sync::atomic::{AtomicBool, Ordering};

pub struct AtomicSpinLockGuard<'a> {
    lock: &'a BareSpinLock,
}

impl Drop for AtomicSpinLockGuard<'_> {
    fn drop(&mut self) {
        self.lock.unlock();
    }
}

pub struct BareSpinLock {
    locked: AtomicBool,
}

impl BareSpinLock {
    pub const fn new() -> Self {
        Self {
            locked: AtomicBool::new(false),
        }
    }

    // Only to be called by AtomicSpinlockSentinel::drop().
    fn unlock(&self) {
        self.locked.store(false, Ordering::Release);
    }
}

impl Default for BareSpinLock {
    fn default() -> Self {
        Self::new()
    }
}

impl crate::sync::spinlock::BareSpinLock for BareSpinLock {
    type Guard<'a> = AtomicSpinLockGuard<'a>;

    #[allow(clippy::declare_interior_mutable_const)]
    const NEW: BareSpinLock = Self::new();

    fn try_lock(&self) -> Option<Self::Guard<'_>> {
        self.locked
            .compare_exchange(false, true, Ordering::Acquire, Ordering::Relaxed)
            .map(|_| AtomicSpinLockGuard { lock: self })
            .ok()
    }
}
