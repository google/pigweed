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

use core::cell::UnsafeCell;
use core::mem::ManuallyDrop;

use cortex_m::interrupt;
use cortex_m::register::primask::{self, Primask};

struct InterruptGuard {
    saved_primask: Primask,
}

impl InterruptGuard {
    fn new() -> Self {
        let saved_primask = primask::read();

        // `cortex_m::interrupt::disable()` handles proper fencing.
        interrupt::disable();

        Self { saved_primask }
    }
}

impl Drop for InterruptGuard {
    #[inline]
    fn drop(&mut self) {
        if self.saved_primask.is_active() {
            // `cortex_m::interrupt::enable()` handles proper fencing.
            //
            // Safety: cortex-m critical sections are not used in this code base.
            unsafe { interrupt::enable() };
        }
    }
}

pub struct CortexMSpinLockGuard<'a> {
    // A size optimization that allow the sentinel's drop() to directly
    // call `InterruptGuard::do_drop()`.
    guard: ManuallyDrop<InterruptGuard>,
    lock: &'a BareSpinLock,
}

impl Drop for CortexMSpinLockGuard<'_> {
    #[inline]
    fn drop(&mut self) {
        unsafe {
            self.lock.unlock();
            ManuallyDrop::drop(&mut self.guard);
        };
    }
}

/// Non-SMP bare spinlock
pub struct BareSpinLock {
    // Lock state is needed to support `try_lock()` semantics.  An `UnsafeCell`
    // is used to hold the lock state as exclusive access is guaranteed by
    // enabling and disabling interrupts.
    is_locked: UnsafeCell<bool>,
}

// Safety: Access to `is_locked` is protected by disabling interrupts and
// proper barriers.
unsafe impl Send for BareSpinLock {}
unsafe impl Sync for BareSpinLock {}

impl BareSpinLock {
    pub const fn new() -> Self {
        Self {
            is_locked: UnsafeCell::new(false),
        }
    }

    // Must be called with interrupts disabled.
    #[inline]
    unsafe fn unlock(&self) {
        *self.is_locked.get() = false;
    }
}

impl Default for BareSpinLock {
    fn default() -> Self {
        Self::new()
    }
}

impl crate::arch::BareSpinLock for BareSpinLock {
    type Guard<'a> = CortexMSpinLockGuard<'a>;

    fn try_lock(&self) -> Option<Self::Guard<'_>> {
        let guard = InterruptGuard::new();
        // Safety: exclusive access to `is_locked` guaranteed because interrupts
        // are off.
        if unsafe { *self.is_locked.get() } {
            return None;
        }

        unsafe {
            *self.is_locked.get() = true;
        }

        Some(CortexMSpinLockGuard {
            guard: ManuallyDrop::new(guard),
            lock: self,
        })
    }
}
