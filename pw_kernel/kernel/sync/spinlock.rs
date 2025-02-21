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

use crate::arch::{Arch, ArchInterface};

pub use crate::arch::BareSpinLock as BareSpinLockApi;

pub type BareSpinLock = <Arch as ArchInterface>::BareSpinLock;

pub struct SpinLockGuard<'lock, T> {
    lock: &'lock SpinLock<T>,
    _inner_guard: <BareSpinLock as BareSpinLockApi>::Guard<'lock>,
}

impl<T> Deref for SpinLockGuard<'_, T> {
    type Target = T;

    fn deref(&self) -> &T {
        unsafe { &*self.lock.data.get() }
    }
}

impl<T> DerefMut for SpinLockGuard<'_, T> {
    fn deref_mut(&mut self) -> &mut T {
        unsafe { &mut *self.lock.data.get() }
    }
}

pub struct SpinLock<T> {
    data: UnsafeCell<T>,
    inner: BareSpinLock,
}

// As long as the inner type is `Send` the lock can be shared between threads.
unsafe impl<T: Send> Sync for SpinLock<T> {}

impl<T> SpinLock<T> {
    pub const fn new(initial_value: T) -> Self {
        Self {
            data: UnsafeCell::new(initial_value),
            inner: BareSpinLock::new(),
        }
    }

    pub fn try_lock(&self) -> Option<SpinLockGuard<'_, T>> {
        self.inner.try_lock().map(|guard| SpinLockGuard {
            lock: self,
            _inner_guard: guard,
        })
    }

    pub fn lock(&self) -> SpinLockGuard<'_, T> {
        let inner_guard = self.inner.lock();
        SpinLockGuard {
            lock: self,
            _inner_guard: inner_guard,
        }
    }
}
