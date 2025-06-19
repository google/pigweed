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

use pw_status::Result;

use crate::scheduler::timer::Instant;
use crate::scheduler::{SchedulerContext, SchedulerStateContext, WaitQueueLock};

/// Configuration for the behavior of an [`Event`].
#[derive(Eq, PartialEq)]
pub enum EventConfig {
    /// When an [`Event`] is signaled, the first waiter to observe that signal
    /// automatically clears the signaled state, returning the `Event` to the
    /// un-signaled state.
    AutoReset,
    /// When an [`Event`] is signaled, it remains signaled indefinitely until
    /// [`unsignal`] is called, which returns the `Event` to the un-signaled
    /// state.
    ///
    /// [`unsignal`]: Event::unsignal
    ManualReset,
}

struct EventState {
    signaled: bool,
}

/// A basic synchronization primitive allowing threads to block on an event
/// happening.
///
/// An `Event` starts in an un-signaled state. Threads can use [`wait`] or
/// [`wait_until`] to block until the state has changed to signaled. The signal
/// state is controlled via the [`signal`] and [`unsignal`] methods.
///
/// Depending on what [`EventConfig`] is used, [`signal`] may either set the
/// signal permanently, or it may be automatically cleared once a single waiter
/// has been un-blocked by it.
///
/// [`wait`]: Event::wait
/// [`wait_until`]: Event::wait_until
/// [`signal`]: Event::signal
/// [`unsignal`]: Event::unsignal
pub struct Event<C: SchedulerContext> {
    config: EventConfig,
    state: WaitQueueLock<C, EventState>,
}

unsafe impl<C: SchedulerContext> Sync for Event<C> {}
unsafe impl<C: SchedulerContext> Send for Event<C> {}

impl<C: SchedulerStateContext> Event<C> {
    /// Constructs a new `Event` with the given configuration.
    #[must_use]
    pub const fn new(ctx: C, config: EventConfig) -> Self {
        Self {
            config,
            state: WaitQueueLock::new(ctx, EventState { signaled: false }),
        }
    }

    /// Waits until the `Event` is in the signaled state.
    ///
    /// If the event's configuration is [`AutoReset`], the thread which is
    /// un-blocked by a signal also clears that signal, resetting its value to
    /// un-signaled.
    ///
    /// [`AutoReset`]: EventConfig::AutoReset
    ///
    /// # Interrupt context
    ///
    /// This method is *not* safe to call in an interrupt context.
    pub fn wait(&self) {
        let mut state = self.state.lock();
        if !state.signaled {
            state.wait();
        } else if self.config == EventConfig::AutoReset {
            state.signaled = false;
        }
    }

    /// Waits until the `Event` is in the signaled state or the `deadline` is
    /// reached, whichever happens first.
    ///
    /// If the event's configuration is [`EventConfig::AutoReset`], the thread
    /// which is un-blocked by a signal also clears that signal, resetting its
    /// value to un-signaled.
    ///
    /// # Interrupt context
    ///
    /// This method is *not* safe to call in an interrupt context.
    pub fn wait_until(&self, deadline: Instant) -> Result<()> {
        let mut state = self.state.lock();
        if !state.signaled {
            let (_state, result) = state.wait_until(deadline);
            return result;
        } else if self.config == EventConfig::AutoReset {
            state.signaled = false;
        }

        Ok(())
    }

    /// Sets the `Event`'s state to signaled.
    ///
    /// # Interrupt context
    ///
    /// This method *is* safe to call in an interrupt context.
    pub fn signal(&self) {
        let mut state = self.state.lock();
        if !state.signaled {
            match self.config {
                EventConfig::AutoReset => {
                    let (mut state, result) = state.wake_one();
                    if result == crate::scheduler::WakeResult::QueueEmpty {
                        state.signaled = true;
                    }
                }
                EventConfig::ManualReset => {
                    state.signaled = true;
                    state.wake_all();
                }
            }
        }
    }

    /// Sets the `Event`'s state to un-signaled.
    ///
    /// # Interrupt context
    ///
    /// This method *is* safe to call in an interrupt context.
    pub fn unsignal(&self) {
        self.state.lock().signaled = false;
    }
}
