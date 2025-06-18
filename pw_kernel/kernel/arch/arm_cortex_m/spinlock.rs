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

use core::arch::asm;
use core::cell::UnsafeCell;
use core::mem::ManuallyDrop;
use core::sync::atomic::{compiler_fence, Ordering};

pub struct InterruptGuard {
    saved_primask: u32,
}

impl InterruptGuard {
    #[inline]
    pub fn new() -> Self {
        let saved_primask: u32;
        unsafe {
            asm!(
                "mrs {}, PRIMASK
                 cpsid i",
                out(reg) saved_primask,
                options(preserves_flags)
            );
        }
        compiler_fence(Ordering::SeqCst);

        Self { saved_primask }
    }
}

impl Drop for InterruptGuard {
    #[inline]
    fn drop(&mut self) {
        compiler_fence(Ordering::SeqCst);
        if (self.saved_primask & 0x1) == 0x0 {
            unsafe {
                asm!("cpsie i", options(preserves_flags));
            }
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

impl crate::sync::spinlock::BareSpinLock for BareSpinLock {
    type Guard<'a> = CortexMSpinLockGuard<'a>;

    const NEW: BareSpinLock = Self::new();

    #[inline(always)]
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

    #[inline(always)]
    fn lock(&self) -> Self::Guard<'_> {
        let guard = InterruptGuard::new();
        // Safety: exclusive access to `is_locked` guaranteed because interrupts
        // are off.

        // For the uniprocessor version of the spinlock, there is no need to spin.

        // TODO - konkers: add debug panic on recursively locked UP spinlock
        // if unsafe { *self.is_locked.get() } {
        //     panic!("recursively locked spinlock");
        // }

        unsafe {
            *self.is_locked.get() = true;
        }

        return CortexMSpinLockGuard {
            guard: ManuallyDrop::new(guard),
            lock: self,
        };
    }
}
