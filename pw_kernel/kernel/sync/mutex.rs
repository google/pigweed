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

use pw_status::Result;

use crate::scheduler::thread::Thread;
use crate::scheduler::{SchedulerContext, SchedulerStateContext, WaitQueueLock};
use crate::timer::Instant;

const MUTEX_DEBUG: bool = false;
macro_rules! mutex_debug {
  ($($args:expr),*) => {{
    log_if::debug_if!(MUTEX_DEBUG, $($args),*)
  }}
}

struct MutexState {
    count: usize,
    holder_thread_id: usize,
}

pub struct Mutex<C: SchedulerContext, T> {
    // An future optimization can be made by keeping an atomic count outside of
    // the spinlock.  However, not all architectures support atomics so a pure
    // SchedLock based approach will always be needed.
    state: WaitQueueLock<C, MutexState>,

    inner: UnsafeCell<T>,
}
unsafe impl<C: SchedulerContext, T> Sync for Mutex<C, T> {}
unsafe impl<C: SchedulerContext, T> Send for Mutex<C, T> {}

pub struct MutexGuard<'lock, C: SchedulerStateContext, T> {
    lock: &'lock Mutex<C, T>,
}

impl<C: SchedulerStateContext, T> Deref for MutexGuard<'_, C, T> {
    type Target = T;

    fn deref(&self) -> &T {
        unsafe { &*self.lock.inner.get() }
    }
}

impl<C: SchedulerStateContext, T> DerefMut for MutexGuard<'_, C, T> {
    fn deref_mut(&mut self) -> &mut T {
        unsafe { &mut *self.lock.inner.get() }
    }
}

impl<C: SchedulerStateContext, T> Drop for MutexGuard<'_, C, T> {
    fn drop(&mut self) {
        self.lock.unlock();
    }
}

impl<C: SchedulerStateContext, T> Mutex<C, T> {
    pub const fn new(ctx: C, initial_value: T) -> Self {
        Self {
            state: WaitQueueLock::new(
                ctx,
                MutexState {
                    count: 0,
                    holder_thread_id: Thread::<C::ThreadState>::null_id(),
                },
            ),
            inner: UnsafeCell::new(initial_value),
        }
    }

    pub fn lock(&self) -> MutexGuard<'_, C, T> {
        let mut state = self.state.lock();
        pw_assert::ne!(
            state.holder_thread_id as usize,
            state.sched().current_thread_id() as usize
        );

        #[allow(clippy::needless_else)]
        if let Some(val) = state.count.checked_add(1) {
            state.count = val;
        } else {
            pw_assert::debug_assert!(false)
        }

        // TODO - konkers: investigate using core::intrinsics::unlikely() or
        //                 core::hint::unlikely()
        if state.count > 1 {
            mutex_debug!("mutex {:08x} lock wait", &raw const *self as usize);
            state = state.wait();
        }
        mutex_debug!("mutex {:08x} lock acquired", &raw const *self as usize);

        state.holder_thread_id = state.sched().current_thread_id();

        // At this point we have exclusive access to `self.inner`.

        MutexGuard { lock: self }
    }

    // TODO - konkers: Investigate combining with lock().
    pub fn lock_until(&self, deadline: Instant) -> Result<MutexGuard<'_, C, T>> {
        let mut state = self.state.lock();
        pw_assert::ne!(
            state.holder_thread_id as usize,
            state.sched().current_thread_id() as usize
        );

        #[allow(clippy::needless_else)]
        if let Some(val) = state.count.checked_add(1) {
            state.count = val;
        } else {
            pw_assert::debug_assert!(false)
        }

        // TODO - konkers: investigate using core::intrinsics::unlikely() or
        //                 core::hint::unlikely()
        if state.count > 1 {
            let result;
            mutex_debug!(
                "mutex {:08x} lock_util({}) wait",
                &raw const *self as usize,
                deadline.ticks() as u64
            );
            (state, result) = state.wait_until(deadline);

            if let Err(e) = result {
                mutex_debug!(
                    "mutex {:08x} lock_until error: {}",
                    &raw const *self as usize,
                    e as u32
                );

                if let Some(val) = state.count.checked_sub(1) {
                    state.count = val;
                } else {
                    // use assert not debug_assert, as it's possible a
                    // real bug could trigger this assert,
                    pw_assert::assert!(false)
                }
                return Err(e);
            }
        }
        mutex_debug!("mutex {:08x} lock_until", &raw const *self as usize);

        state.holder_thread_id = state.sched().current_thread_id();

        // At this point we have exclusive access to `self.inner`.

        Ok(MutexGuard { lock: self })
    }

    fn unlock(&self) {
        let mut state = self.state.lock();

        pw_assert::assert!(state.count > 0);
        pw_assert::eq!(
            state.holder_thread_id as usize,
            state.sched().current_thread_id() as usize
        );
        state.holder_thread_id = Thread::<C::ThreadState>::null_id();

        state.count -= 1;

        // TODO - konkers: investigate using core::intrinsics::unlikely() or
        //                 core::hint::unlikely()
        if state.count > 0 {
            state.wake_one();
        }
    }
}
