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

use crate::scheduler::PreemptDisableGuard;
use crate::{Arch, Kernel};

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

pub struct SpinLockGuard<'lock, K: Kernel, T> {
    lock: &'lock SpinLock<K, T>,
    _preempt_guard: PreemptDisableGuard<K>,
    _inner_guard: <<K as Arch>::BareSpinLock as BareSpinLock>::Guard<'lock>,
}

impl<K: Kernel, T> Deref for SpinLockGuard<'_, K, T> {
    type Target = T;

    fn deref(&self) -> &T {
        unsafe { &*self.lock.data.get() }
    }
}

impl<K: Kernel, T> DerefMut for SpinLockGuard<'_, K, T> {
    fn deref_mut(&mut self) -> &mut T {
        unsafe { &mut *self.lock.data.get() }
    }
}

pub struct SpinLock<K: Kernel, T> {
    data: UnsafeCell<T>,
    inner: K::BareSpinLock,
}
// As long as the inner type is `Send` and the bare spinlock is `Sync`, the lock
// can be shared between threads.
unsafe impl<K: Sync + Kernel, T: Send> Sync for SpinLock<K, T> {}

impl<K: Kernel, T> SpinLock<K, T> {
    pub const fn new(initial_value: T) -> Self {
        Self {
            data: UnsafeCell::new(initial_value),
            inner: K::BareSpinLock::NEW,
        }
    }

    pub fn try_lock(&self, kernel: K) -> Option<SpinLockGuard<'_, K, T>> {
        self.inner.try_lock().map(|guard| SpinLockGuard {
            lock: self,
            _preempt_guard: PreemptDisableGuard::new(kernel),
            _inner_guard: guard,
        })
    }

    pub fn lock(&self, kernel: K) -> SpinLockGuard<'_, K, T> {
        let inner_guard = self.inner.lock();
        SpinLockGuard {
            lock: self,
            _preempt_guard: PreemptDisableGuard::new(kernel),
            _inner_guard: inner_guard,
        }
    }
}
