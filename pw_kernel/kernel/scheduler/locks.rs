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
use core::ptr::NonNull;

use pw_status::Result;

use super::SCHEDULER_STATE;
use crate::scheduler::{SchedulerState, WaitQueue};
use crate::sync::spinlock::SpinLockGuard;
use crate::timer::Instant;

pub struct SmuggledSchedLock<T> {
    inner: NonNull<T>,
}

impl<T> SmuggledSchedLock<T> {
    /// # Safety
    /// The caller must guarantee that the underlying lock and it's enclosed data
    /// is still valid.
    pub unsafe fn lock(&self) -> SchedLockGuard<'_, T> {
        let guard = SCHEDULER_STATE.lock();
        SchedLockGuard {
            guard,
            inner: unsafe { &mut *self.inner.as_ptr() },
        }
    }
}

pub struct SchedLockGuard<'lock, T> {
    guard: SpinLockGuard<'lock, SchedulerState>,
    inner: &'lock mut T,
}

impl<'lock, T> SchedLockGuard<'lock, T> {
    pub fn sched(&self) -> &SpinLockGuard<'lock, SchedulerState> {
        &self.guard
    }

    pub fn sched_mut(&mut self) -> &mut SpinLockGuard<'lock, SchedulerState> {
        &mut self.guard
    }

    pub fn reschedule(self, current_thread_id: usize) -> Self {
        let inner = self.inner;
        let guard = super::reschedule(self.guard, current_thread_id);
        Self { guard, inner }
    }

    pub fn try_reschedule(self) -> Self {
        let inner = self.inner;
        let guard = self.guard.try_reschedule();
        Self { guard, inner }
    }

    /// # Safety
    /// The caller must guarantee that the underlying lock remains valid and
    /// un-moved for the live the smuggled lock.
    pub unsafe fn smuggle(&self) -> SmuggledSchedLock<T> {
        let inner: *const T = self.inner;
        SmuggledSchedLock {
            inner: unsafe { NonNull::new_unchecked(inner.cast_mut()) },
        }
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
    pub fn sched(&self) -> &SpinLockGuard<'lock, SchedulerState> {
        &self.inner.guard
    }

    #[allow(dead_code)]
    pub fn sched_mut(&mut self) -> &mut SpinLockGuard<'lock, SchedulerState> {
        &mut self.inner.guard
    }

    pub fn operate_on_wait_queue<F, R>(mut self, f: F) -> (Self, R)
    where
        F: FnOnce(SchedLockGuard<'lock, WaitQueue>) -> (SchedLockGuard<'lock, WaitQueue>, R),
    {
        let guard = SchedLockGuard::<'lock, WaitQueue> {
            guard: self.inner.guard,
            // Safety: Mutable reference only lives as long as the call into f()
            #[allow(clippy::deref_addrof)]
            inner: unsafe { &mut *&raw mut self.inner.inner.queue },
        };
        let (guard, result) = f(guard);
        self.inner.guard = guard.guard;
        (self, result)
    }

    pub fn wait_until(self, deadline: Instant) -> (Self, Result<()>) {
        self.operate_on_wait_queue(|guard| guard.wait_until(deadline))
    }

    pub fn wait(self) -> Self {
        self.operate_on_wait_queue(|guard| (guard.wait(), ())).0
    }

    pub fn wake_one(self) -> (Self, super::WakeResult) {
        self.operate_on_wait_queue(|guard| guard.wake_one())
    }

    pub fn wake_all(self) -> Self {
        self.operate_on_wait_queue(|guard| (guard.wake_all(), ())).0
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
