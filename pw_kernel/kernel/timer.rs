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

use core::ptr::NonNull;

use foreign_box::ForeignBox;
use list::{ForeignList, Link};

use crate::arch::{Arch, ArchInterface};
use crate::sync::spinlock::SpinLock;

pub type Clock = <Arch as ArchInterface>::Clock;
pub type Instant = time::Instant<Clock>;
pub type Duration = time::Duration<Clock>;

list::define_adapter!(pub TimerCallbackListAdapter => TimerCallback::link);

pub type TimerCallbackFn = dyn FnMut(ForeignBox<TimerCallback>, Instant);
#[allow(dead_code)]
pub struct TimerCallback {
    deadline: Instant,
    // XXX: make private
    pub callback: Option<ForeignBox<TimerCallbackFn>>,
    link: Link,
}

impl TimerCallback {
    #[allow(dead_code)]
    pub fn new(deadline: Instant, callback: ForeignBox<TimerCallbackFn>) -> Self {
        Self {
            deadline,
            callback: Some(callback),
            link: Link::new(),
        }
    }
}

impl PartialEq for TimerCallback {
    fn eq(&self, other: &Self) -> bool {
        self.deadline == other.deadline
    }
}

impl Eq for TimerCallback {}

impl PartialOrd for TimerCallback {
    fn partial_cmp(&self, other: &Self) -> Option<core::cmp::Ordering> {
        Some(self.deadline.cmp(&other.deadline))
    }
}

impl Ord for TimerCallback {
    fn cmp(&self, other: &Self) -> core::cmp::Ordering {
        self.deadline.cmp(&other.deadline)
    }
}

#[allow(dead_code)]
pub struct TimerQueue {
    queue: ForeignList<TimerCallback, TimerCallbackListAdapter>,
}

unsafe impl Sync for TimerQueue {}
unsafe impl Send for TimerQueue {}

impl TimerQueue {
    pub const fn new() -> Self {
        Self {
            queue: ForeignList::new(),
        }
    }

    #[allow(dead_code)]
    pub fn schedule_timer(callback: ForeignBox<TimerCallback>) {
        let mut timer_queue = TIMER_QUEUE.lock();
        timer_queue.queue.sorted_insert(callback);
    }

    pub unsafe fn cancel_timer(timer: NonNull<TimerCallback>) -> Option<ForeignBox<TimerCallback>> {
        let mut timer_queue = TIMER_QUEUE.lock();
        unsafe { timer_queue.queue.remove_element(timer) }
    }

    pub unsafe fn cancel_and_consume_timer(timer: NonNull<TimerCallback>) {
        if let Some(mut callback) = unsafe { Self::cancel_timer(timer) } {
            if let Some(callback) = callback.callback.take() {
                callback.consume();
            }
            callback.consume();
        }
    }

    pub fn get_next_exipred_callback(&mut self, now: Instant) -> Option<ForeignBox<TimerCallback>> {
        // TODO - konkers: An optimization opportunity exists here to implement
        // a something like `peek_front(|front| front.deadline > now)` which
        // would eliminate the popping and re-pushing of the non-expired
        // callback.
        let callback = self.queue.pop_head()?;
        if callback.deadline > now {
            self.queue.push_front(callback);
            return None;
        }

        Some(callback)
    }

    #[allow(dead_code)]
    pub fn process_queue(now: Instant) {
        let mut timer_queue = TIMER_QUEUE.lock();
        while let Some(mut callback) = timer_queue.get_next_exipred_callback(now) {
            // Drop the timer queue lock before processing callbacks.  This is
            // Safe to do because the callback is already removed from the queue.
            drop(timer_queue);

            let Some(mut callback_fn) = callback.callback.take() else {
                pw_assert::panic!("Non callback function found on timer");
            };
            (callback_fn)(callback, now);
            let _ = callback_fn.consume();

            // Require timer queue lock.
            timer_queue = TIMER_QUEUE.lock();
        }
    }
}

pub static TIMER_QUEUE: SpinLock<TimerQueue> = SpinLock::new(TimerQueue::new());
