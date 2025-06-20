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

use crate::scheduler::SchedulerStateContext;

pub type Instant<C> = time::Instant<C>;
pub type Duration<C> = time::Duration<C>;

list::define_adapter!(pub TimerCallbackListAdapter<C: time::Clock> => TimerCallback<C>::link);

pub type TimerCallbackFn<C> = dyn FnMut(ForeignBox<TimerCallback<C>>, Instant<C>);
#[allow(dead_code)]
pub struct TimerCallback<C: time::Clock> {
    deadline: Instant<C>,
    // XXX: make private
    pub callback: Option<ForeignBox<TimerCallbackFn<C>>>,
    link: Link,
}

impl<C: time::Clock> TimerCallback<C> {
    #[allow(dead_code)]
    #[must_use]
    pub fn new(deadline: Instant<C>, callback: ForeignBox<TimerCallbackFn<C>>) -> Self {
        Self {
            deadline,
            callback: Some(callback),
            link: Link::new(),
        }
    }
}

impl<C: time::Clock> PartialEq for TimerCallback<C> {
    fn eq(&self, other: &Self) -> bool {
        self.deadline == other.deadline
    }
}

impl<C: time::Clock> Eq for TimerCallback<C> {}

impl<C: time::Clock> PartialOrd for TimerCallback<C> {
    fn partial_cmp(&self, other: &Self) -> Option<core::cmp::Ordering> {
        Some(self.deadline.cmp(&other.deadline))
    }
}

impl<C: time::Clock> Ord for TimerCallback<C> {
    fn cmp(&self, other: &Self) -> core::cmp::Ordering {
        self.deadline.cmp(&other.deadline)
    }
}

#[allow(dead_code)]
pub struct TimerQueue<C: time::Clock> {
    queue: ForeignList<TimerCallback<C>, TimerCallbackListAdapter<C>>,
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

    pub fn get_next_exipred_callback(
        &mut self,
        now: Instant<C>,
    ) -> Option<ForeignBox<TimerCallback<C>>> {
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
pub fn schedule_timer<C: SchedulerStateContext>(
    ctx: C,
    callback: ForeignBox<TimerCallback<C::Clock>>,
) {
    let mut timer_queue = ctx.get_timer_queue().lock();
    timer_queue.queue.sorted_insert(callback);
}

/// # Safety
///
/// `timer` must point to a valid [`TimerCallback`] object which is in the
/// timer queue.
pub unsafe fn cancel_timer<C: SchedulerStateContext>(
    ctx: C,
    timer: NonNull<TimerCallback<C::Clock>>,
) -> Option<ForeignBox<TimerCallback<C::Clock>>> {
    let mut timer_queue = ctx.get_timer_queue().lock();
    unsafe { timer_queue.queue.remove_element(timer) }
}

/// # Safety
///
/// `timer` must point to a valid [`TimerCallback`] object which is in the
/// timer queue.
pub unsafe fn cancel_and_consume_timer<C: SchedulerStateContext>(
    ctx: C,
    timer: NonNull<TimerCallback<C::Clock>>,
) {
    if let Some(mut callback) = unsafe { cancel_timer(ctx, timer) } {
        if let Some(callback) = callback.callback.take() {
            callback.consume();
        }
        callback.consume();
    }
}

#[allow(dead_code)]
pub fn process_queue<C: SchedulerStateContext>(ctx: C, now: Instant<C::Clock>) {
    let mut timer_queue = ctx.get_timer_queue().lock();
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
        timer_queue = ctx.get_timer_queue().lock();
    }
}
