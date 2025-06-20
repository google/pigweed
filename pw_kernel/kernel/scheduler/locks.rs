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

use crate::scheduler::thread::ThreadState;
use crate::scheduler::timer::Instant;
use crate::scheduler::{SchedulerContext, SchedulerState, SchedulerStateContext, WaitQueue};
use crate::sync::spinlock::SpinLockGuard;

pub struct SmuggledSchedLock<C, T> {
    inner: NonNull<T>,
    ctx: C,
}

impl<C: SchedulerStateContext, T> SmuggledSchedLock<C, T> {
    /// # Safety
    /// The caller must guarantee that the underlying lock and it's enclosed data
    /// is still valid.
    pub unsafe fn lock(&self) -> SchedLockGuard<'_, C, T> {
        let guard = self.ctx.get_scheduler().lock();
        SchedLockGuard {
            guard,
            inner: unsafe { &mut *self.inner.as_ptr() },
            ctx: self.ctx,
        }
    }
}

pub struct SchedLockGuard<'lock, C: SchedulerContext, T> {
    guard: SpinLockGuard<'lock, C::BareSpinLock, SchedulerState<C::ThreadState>>,
    inner: &'lock mut T,
    pub(super) ctx: C,
}

impl<'lock, C: SchedulerContext, T> SchedLockGuard<'lock, C, T> {
    #[must_use]
    pub fn sched(&self) -> &SpinLockGuard<'lock, C::BareSpinLock, SchedulerState<C::ThreadState>> {
        &self.guard
    }

    #[must_use]
    pub fn sched_mut(
        &mut self,
    ) -> &mut SpinLockGuard<'lock, C::BareSpinLock, SchedulerState<C::ThreadState>> {
        &mut self.guard
    }

    #[allow(clippy::return_self_not_must_use)]
    pub fn reschedule(self, current_thread_id: usize) -> Self {
        let inner = self.inner;
        let ctx = self.ctx;
        let guard = super::reschedule(ctx, self.guard, current_thread_id);
        Self { guard, inner, ctx }
    }

    #[allow(clippy::return_self_not_must_use, clippy::must_use_candidate)]
    pub fn try_reschedule(self) -> Self {
        let inner = self.inner;
        let ctx = self.ctx;
        let guard = self.guard.try_reschedule(ctx);
        Self { guard, inner, ctx }
    }

    /// # Safety
    /// The caller must guarantee that the underlying lock remains valid and
    /// un-moved for the live the smuggled lock.
    #[must_use]
    pub unsafe fn smuggle(&self) -> SmuggledSchedLock<C, T> {
        let inner: *const T = self.inner;
        SmuggledSchedLock {
            inner: unsafe { NonNull::new_unchecked(inner.cast_mut()) },
            ctx: self.ctx,
        }
    }
}

impl<C: SchedulerContext, T> Deref for SchedLockGuard<'_, C, T> {
    type Target = T;

    fn deref(&self) -> &T {
        self.inner
    }
}

impl<C: SchedulerContext, T> DerefMut for SchedLockGuard<'_, C, T> {
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
pub struct SchedLock<C, T> {
    inner: UnsafeCell<T>,
    ctx: C,
}
unsafe impl<C: Sync, T> Sync for SchedLock<C, T> {}
unsafe impl<C: Send, T> Send for SchedLock<C, T> {}

impl<C, T> SchedLock<C, T> {
    pub const fn new(ctx: C, initial_value: T) -> Self {
        Self {
            inner: UnsafeCell::new(initial_value),
            ctx,
        }
    }
}

impl<C: SchedulerStateContext, T> SchedLock<C, T> {
    #[allow(unused)]
    pub fn try_lock(&self) -> Option<SchedLockGuard<'_, C, T>> {
        // Safety: The lock guarantees
        self.ctx
            .get_scheduler()
            .try_lock()
            .map(|guard| SchedLockGuard {
                inner: unsafe { &mut *self.inner.get() },
                guard,
                ctx: self.ctx,
            })
    }

    pub fn lock(&self) -> SchedLockGuard<'_, C, T> {
        let guard = self.ctx.get_scheduler().lock();
        SchedLockGuard {
            inner: unsafe { &mut *self.inner.get() },
            guard,
            ctx: self.ctx,
        }
    }
}

pub struct WaitQueueLockState<S: ThreadState, T> {
    queue: WaitQueue<S>,
    inner: T,
}

pub struct WaitQueueLock<C: SchedulerContext, T> {
    state: SchedLock<C, WaitQueueLockState<C::ThreadState, T>>,
}

impl<C: SchedulerStateContext, T> WaitQueueLock<C, T> {
    pub const fn new(ctx: C, initial_value: T) -> Self {
        Self {
            state: SchedLock::new(
                ctx,
                WaitQueueLockState {
                    queue: WaitQueue::new(),
                    inner: initial_value,
                },
            ),
        }
    }

    pub fn lock(&self) -> WaitQueueLockGuard<'_, C, T> {
        WaitQueueLockGuard {
            inner: self.state.lock(),
        }
    }
}

pub struct WaitQueueLockGuard<'lock, C: SchedulerContext, T> {
    inner: SchedLockGuard<'lock, C, WaitQueueLockState<C::ThreadState, T>>,
}

impl<'lock, C: SchedulerStateContext, T> WaitQueueLockGuard<'lock, C, T> {
    pub fn sched(&self) -> &SpinLockGuard<'lock, C::BareSpinLock, SchedulerState<C::ThreadState>> {
        &self.inner.guard
    }

    #[allow(dead_code)]
    pub fn sched_mut(
        &mut self,
    ) -> &mut SpinLockGuard<'lock, C::BareSpinLock, SchedulerState<C::ThreadState>> {
        &mut self.inner.guard
    }

    pub fn operate_on_wait_queue<F, R>(mut self, f: F) -> (Self, R)
    where
        F: FnOnce(
            SchedLockGuard<'lock, C, WaitQueue<C::ThreadState>>,
        ) -> (SchedLockGuard<'lock, C, WaitQueue<C::ThreadState>>, R),
    {
        let guard = SchedLockGuard::<'lock, _, WaitQueue<C::ThreadState>> {
            guard: self.inner.guard,
            // Safety: Mutable reference only lives as long as the call into f()
            #[allow(clippy::deref_addrof)]
            inner: unsafe { &mut *&raw mut self.inner.inner.queue },
            ctx: self.inner.ctx,
        };
        let (guard, result) = f(guard);
        self.inner.guard = guard.guard;
        (self, result)
    }

    pub fn wait_until(self, deadline: Instant<C::Clock>) -> (Self, Result<()>) {
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

impl<C: SchedulerContext, T> Deref for WaitQueueLockGuard<'_, C, T> {
    type Target = T;

    fn deref(&self) -> &T {
        &self.inner.inner.inner
    }
}

impl<C: SchedulerContext, T> DerefMut for WaitQueueLockGuard<'_, C, T> {
    fn deref_mut(&mut self) -> &mut T {
        &mut self.inner.inner.inner
    }
}
