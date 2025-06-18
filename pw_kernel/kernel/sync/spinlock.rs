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
use core::ops::{Deref, DerefMut};

use crate::arch::Arch;

pub type ConcreteBareSpinLock = <Arch as crate::scheduler::SchedulerContext>::BareSpinLock;

pub trait BareSpinLock {
    type Guard<'a>
    where
        Self: 'a;

    const NEW: Self;

    fn try_lock(&self) -> Option<Self::Guard<'_>>;

    #[inline(always)]
    fn lock(&self) -> Self::Guard<'_> {
        loop {
            if let Some(sentinel) = self.try_lock() {
                return sentinel;
            }
        }
    }

    // TODO - konkers: Add optimized path for functions that know they are in
    // atomic context (i.e. interrupt handlers).
}

pub struct SpinLockGuard<'lock, L: BareSpinLock, T> {
    lock: &'lock SpinLock<L, T>,
    _inner_guard: L::Guard<'lock>,
}

impl<L: BareSpinLock, T> Deref for SpinLockGuard<'_, L, T> {
    type Target = T;

    fn deref(&self) -> &T {
        unsafe { &*self.lock.data.get() }
    }
}

impl<L: BareSpinLock, T> DerefMut for SpinLockGuard<'_, L, T> {
    fn deref_mut(&mut self) -> &mut T {
        unsafe { &mut *self.lock.data.get() }
    }
}

pub struct SpinLock<L, T> {
    data: UnsafeCell<T>,
    inner: L,
}

// As long as the inner type is `Send` and the bare spinlock is `Sync`, the lock
// can be shared between threads.
unsafe impl<L: Sync, T: Send> Sync for SpinLock<L, T> {}

impl<L: BareSpinLock, T> SpinLock<L, T> {
    pub const fn new(initial_value: T) -> Self {
        Self {
            data: UnsafeCell::new(initial_value),
            inner: L::NEW,
        }
    }

    pub fn try_lock(&self) -> Option<SpinLockGuard<'_, L, T>> {
        self.inner.try_lock().map(|guard| SpinLockGuard {
            lock: self,
            _inner_guard: guard,
        })
    }

    pub fn lock(&self) -> SpinLockGuard<'_, L, T> {
        let inner_guard = self.inner.lock();
        SpinLockGuard {
            lock: self,
            _inner_guard: inner_guard,
        }
    }
}

#[cfg(test)]
mod tests {
    use unittest::test;

    use super::*;

    #[test]
    fn bare_try_lock_returns_correct_value() -> unittest::Result<()> {
        let lock = ConcreteBareSpinLock::new();

        {
            let _sentinel = lock.lock();
            unittest::assert_true!(lock.try_lock().is_none());
        }

        unittest::assert_true!(lock.try_lock().is_some());

        Ok(())
    }

    #[test]
    fn try_lock_returns_correct_value() -> unittest::Result<()> {
        let lock = SpinLock::<ConcreteBareSpinLock, _>::new(false);

        {
            let mut guard = lock.lock();
            *guard = true;
            unittest::assert_true!(lock.try_lock().is_none());
        }

        let guard = lock.lock();
        unittest::assert_true!(*guard);

        Ok(())
    }
}
