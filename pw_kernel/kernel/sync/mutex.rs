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

use pw_status::Result;

use crate::{
    scheduler::{Thread, WaitQueueLock},
    timer::Instant,
};

struct MutexState {
    count: u32,
    holder_thread_id: usize,
}

pub struct Mutex<T> {
    // An future optimization can be made by keeping an atomic count outside of
    // the spinlock.  However, not all architectures support atomics so a pure
    // SchedLock based approach will always be needed.
    state: WaitQueueLock<MutexState>,

    inner: UnsafeCell<T>,
}
unsafe impl<T> Sync for Mutex<T> {}
unsafe impl<T> Send for Mutex<T> {}

pub struct MutexGuard<'lock, T> {
    lock: &'lock Mutex<T>,
}

impl<T> Deref for MutexGuard<'_, T> {
    type Target = T;

    fn deref(&self) -> &T {
        unsafe { &*self.lock.inner.get() }
    }
}

impl<T> DerefMut for MutexGuard<'_, T> {
    fn deref_mut(&mut self) -> &mut T {
        unsafe { &mut *self.lock.inner.get() }
    }
}

impl<T> Drop for MutexGuard<'_, T> {
    fn drop(&mut self) {
        self.lock.unlock();
    }
}

impl<T> Mutex<T> {
    pub const fn new(initial_value: T) -> Self {
        Self {
            state: WaitQueueLock::new(MutexState {
                count: 0,
                holder_thread_id: Thread::null_id(),
            }),
            inner: UnsafeCell::new(initial_value),
        }
    }

    pub fn lock(&self) -> MutexGuard<'_, T> {
        let mut state = self.state.lock();
        assert_ne!(state.holder_thread_id, state.sched().current_thread_id());
        state.count += 1;
        // TODO - konkers: investigate using core::intrinsics::unlikely() or
        //                 core::hint::unlikely()
        if state.count > 1 {
            state = state.wait();
        }

        state.holder_thread_id = state.sched().current_thread_id();

        // At this point we have exclusive access to `self.inner`.

        MutexGuard { lock: self }
    }

    // TODO - konkers: Investigate combining with lock().
    pub fn lock_until(&self, deadline: Instant) -> Result<MutexGuard<'_, T>> {
        let mut state = self.state.lock();
        assert_ne!(state.holder_thread_id, state.sched().current_thread_id());
        state.count += 1;
        // TODO - konkers: investigate using core::intrinsics::unlikely() or
        //                 core::hint::unlikely()
        if state.count > 1 {
            let result;
            (state, result) = state.wait_until(deadline);

            if let Err(e) = result {
                state.count -= 1;
                return Err(e);
            }
        }

        state.holder_thread_id = state.sched().current_thread_id();

        // At this point we have exclusive access to `self.inner`.

        Ok(MutexGuard { lock: self })
    }

    fn unlock(&self) {
        let mut state = self.state.lock();

        assert!(state.count > 0);
        assert_eq!(state.holder_thread_id, state.sched().current_thread_id());
        state.holder_thread_id = Thread::null_id();

        state.count -= 1;

        // TODO - konkers: investigate using core::intrinsics::unlikely() or
        //                 core::hint::unlikely()
        if state.count > 0 {
            state.wake_one();
        }
    }
}
