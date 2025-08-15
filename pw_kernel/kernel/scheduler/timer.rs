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
use time::Instant;

use crate::scheduler::Kernel;

list::define_adapter!(pub TimerCallbackListAdapter<C: time::Clock> => Timer<C>::link);

pub trait TimerCallback<C: time::Clock> {
    fn callback(
        &mut self,
        timer: ForeignBox<Timer<C>>,
        now: Instant<C>,
    ) -> Option<ForeignBox<Timer<C>>>;
}

impl<C, F> TimerCallback<C> for F
where
    C: time::Clock,
    F: FnMut(ForeignBox<Timer<C>>, Instant<C>) -> Option<ForeignBox<Timer<C>>>,
{
    fn callback(
        &mut self,
        timer: ForeignBox<Timer<C>>,
        now: Instant<C>,
    ) -> Option<ForeignBox<Timer<C>>> {
        self(timer, now)
    }
}

#[allow(dead_code)]
pub struct Timer<C: time::Clock> {
    deadline: Instant<C>,
    // XXX: make private
    pub callback: Option<ForeignBox<dyn TimerCallback<C>>>,
    link: Link,
}

impl<C: time::Clock> Timer<C> {
    #[allow(dead_code)]
    #[must_use]
    pub fn new(deadline: Instant<C>, callback: ForeignBox<dyn TimerCallback<C>>) -> Self {
        Self {
            deadline,
            callback: Some(callback),
            link: Link::new(),
        }
    }

    pub fn set(&mut self, deadline: Instant<C>) {
        self.deadline = deadline
    }
}

impl<C: time::Clock> PartialEq for Timer<C> {
    fn eq(&self, other: &Self) -> bool {
        self.deadline == other.deadline
    }
}

impl<C: time::Clock> Eq for Timer<C> {}

impl<C: time::Clock> PartialOrd for Timer<C> {
    fn partial_cmp(&self, other: &Self) -> Option<core::cmp::Ordering> {
        Some(self.cmp(other))
    }
}

impl<C: time::Clock> Ord for Timer<C> {
    fn cmp(&self, other: &Self) -> core::cmp::Ordering {
        self.deadline.cmp(&other.deadline)
    }
}

#[allow(dead_code)]
pub struct TimerQueue<C: time::Clock> {
    queue: ForeignList<Timer<C>, TimerCallbackListAdapter<C>>,
}

unsafe impl<C: time::Clock> Sync for TimerQueue<C> {}
unsafe impl<C: time::Clock> Send for TimerQueue<C> {}

impl<C: time::Clock> TimerQueue<C> {
    #[must_use]
    pub const fn new() -> Self {
        Self {
            queue: ForeignList::new(),
        }
    }

    pub fn get_next_exipred_callback(&mut self, now: Instant<C>) -> Option<ForeignBox<Timer<C>>> {
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
}

#[allow(dead_code)]
pub fn schedule_timer<K: Kernel>(kernel: K, callback: ForeignBox<Timer<K::Clock>>) {
    let mut timer_queue = kernel.get_timer_queue().lock(kernel);
    timer_queue.queue.sorted_insert(callback);
}

/// # Safety
///
/// `timer` must point to a valid [`TimerCallback`] object which is in the
/// timer queue.
pub unsafe fn cancel_timer<K: Kernel>(
    kernel: K,
    timer: NonNull<Timer<K::Clock>>,
) -> Option<ForeignBox<Timer<K::Clock>>> {
    let mut timer_queue = kernel.get_timer_queue().lock(kernel);
    unsafe { timer_queue.queue.remove_element(timer) }
}

/// # Safety
///
/// `timer` must point to a valid [`TimerCallback`] object which is in the
/// timer queue.
pub unsafe fn cancel_and_consume_timer<K: Kernel>(kernel: K, timer: NonNull<Timer<K::Clock>>) {
    if let Some(mut callback) = unsafe { cancel_timer(kernel, timer) } {
        if let Some(callback) = callback.callback.take() {
            callback.consume();
        }
        callback.consume();
    }
}

#[allow(dead_code)]
pub fn process_queue<K: Kernel>(kernel: K, now: Instant<K::Clock>) {
    let mut timer_queue = kernel.get_timer_queue().lock(kernel);
    while let Some(mut callback) = timer_queue.get_next_exipred_callback(now) {
        // Drop the timer queue lock before processing callbacks.  This is
        // Safe to do because the callback is already removed from the queue.
        drop(timer_queue);

        let Some(mut callback_fn) = callback.callback.take() else {
            pw_assert::panic!("Non callback function found on timer");
        };
        match callback_fn.callback(callback, now) {
            Some(mut next_callback) => {
                next_callback.callback = Some(callback_fn);
                schedule_timer(kernel, next_callback);
            }
            None => {
                let _ = callback_fn.consume();
            }
        }

        // Require timer queue lock.
        timer_queue = kernel.get_timer_queue().lock(kernel);
    }
}
