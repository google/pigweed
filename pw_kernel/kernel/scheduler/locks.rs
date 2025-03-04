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

use core::{
    cell::UnsafeCell,
    ops::{Deref, DerefMut},
};

use crate::scheduler::{SchedulerState, WaitQueue};
use crate::sync::spinlock::SpinLockGuard;

pub struct SchedLockGuard<'lock, T> {
    guard: SpinLockGuard<'lock, SchedulerState>,
    inner: &'lock mut T,
}

impl<'lock, T> SchedLockGuard<'lock, T> {
    #[allow(dead_code)]
    pub fn sched(&self) -> &SpinLockGuard<'lock, SchedulerState> {
        &self.guard
    }

    #[allow(dead_code)]
    pub fn sched_mut(&mut self) -> &mut SpinLockGuard<'lock, SchedulerState> {
        &mut self.guard
    }

    #[allow(dead_code)]
    pub fn into_sched(self) -> SpinLockGuard<'lock, SchedulerState> {
        self.guard
    }
}

impl<T> Deref for SchedLockGuard<'_, T> {
    type Target = T;

    fn deref(&self) -> &T {
        self.inner
    }
}

impl<T> DerefMut for SchedLockGuard<'_, T> {
    fn deref_mut(&mut self) -> &mut T {
        self.inner
    }
}

/// An owning lock that shares the global scheduler lock.
///
/// A [`SchedLockGuard`] can be turned into a `SpinLockGuard<'lock, SchedulerState>`
/// so that it can be passed to `reschedule()`
///
/// # Safety
/// Taking two different `SchedLock`s at the same time will deadlock as they
/// share the same underlying lock.
pub struct SchedLock<T> {
    inner: UnsafeCell<T>,
}
unsafe impl<T> Sync for SchedLock<T> {}
unsafe impl<T> Send for SchedLock<T> {}

impl<T> SchedLock<T> {
    pub const fn new(initial_value: T) -> Self {
        Self {
            inner: UnsafeCell::new(initial_value),
        }
    }

    #[allow(unused)]
    pub fn try_lock(&self) -> Option<SchedLockGuard<'_, T>> {
        // Safety: The lock guarantees
        super::SCHEDULER_STATE
            .try_lock()
            .map(|guard| SchedLockGuard {
                inner: unsafe { &mut *self.inner.get() },
                guard,
            })
    }

    pub fn lock(&self) -> SchedLockGuard<'_, T> {
        let guard = super::SCHEDULER_STATE.lock();
        SchedLockGuard {
            inner: unsafe { &mut *self.inner.get() },
            guard,
        }
    }
}

pub struct WaitQueueLockState<T> {
    queue: WaitQueue,
    inner: T,
}

pub struct WaitQueueLock<T> {
    state: SchedLock<WaitQueueLockState<T>>,
}

impl<T> WaitQueueLock<T> {
    pub const fn new(initial_value: T) -> Self {
        Self {
            state: SchedLock::new(WaitQueueLockState {
                queue: WaitQueue::new(),
                inner: initial_value,
            }),
        }
    }

    pub fn lock(&self) -> WaitQueueLockGuard<'_, T> {
        WaitQueueLockGuard {
            inner: self.state.lock(),
        }
    }
}

pub struct WaitQueueLockGuard<'lock, T> {
    inner: SchedLockGuard<'lock, WaitQueueLockState<T>>,
}

impl<'lock, T> WaitQueueLockGuard<'lock, T> {
    #[allow(dead_code)]
    pub fn sched(&self) -> &SpinLockGuard<'lock, SchedulerState> {
        &self.inner.guard
    }

    #[allow(dead_code)]
    pub fn sched_mut(&mut self) -> &mut SpinLockGuard<'lock, SchedulerState> {
        &mut self.inner.guard
    }

    #[allow(dead_code)]
    pub fn into_sched(self) -> SpinLockGuard<'lock, SchedulerState> {
        self.inner.guard
    }

    #[allow(dead_code)]
    pub fn into_wait_queue(self) -> SchedLockGuard<'lock, WaitQueue> {
        SchedLockGuard::<'lock, WaitQueue> {
            guard: self.inner.guard,
            inner: &mut self.inner.inner.queue,
        }
    }
}

impl<T> Deref for WaitQueueLockGuard<'_, T> {
    type Target = T;

    fn deref(&self) -> &T {
        &self.inner.inner.inner
    }
}

impl<T> DerefMut for WaitQueueLockGuard<'_, T> {
    fn deref_mut(&mut self) -> &mut T {
        &mut self.inner.inner.inner
    }
}
