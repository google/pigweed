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
use core::sync::atomic::{AtomicUsize, Ordering};

use pw_status::Result;
use time::Instant;

use crate::Kernel;
use crate::scheduler::WaitQueueLock;

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
    /// [`unsignal`]: EventSignaler::unsignal
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
/// state is controlled by an [`EventSignaler`] via the [`signal`] and [`unsignal`]
/// methods.
///
/// Depending on what [`EventConfig`] is used, [`signal`] may either set the
/// signal permanently, or it may be automatically cleared once a single waiter
/// has been un-blocked by it.
///
/// # Panics
///
/// Panics on drop if there are [`EventSignaler`]s referencing this object.
///
/// [`wait`]: Event::wait
/// [`wait_until`]: Event::wait_until
/// [`signal`]: EventSignaler::signal
/// [`unsignal`]: EventSignaler::unsignal
pub struct Event<K: Kernel> {
    config: EventConfig,
    state: WaitQueueLock<K, EventState>,
    signalers: AtomicUsize,
}

/// An signaler for an [`Event`]
///
/// [`EventSignaler`] is returned from [`Event::new()`] and is used to signal
/// the referenced Event.  It can be cloned and the referenced [`Event`] will
/// panic if it is dropped when any [`EventSignaler`]s still reference it.
pub struct EventSignaler<K: Kernel> {
    event: NonNull<Event<K>>,
}

// SAFETY: Event will panic if there are outstanding signalers referencing it
// guaranteeing that the event pointer will always be valid.
unsafe impl<K: Kernel> Send for EventSignaler<K> {}
unsafe impl<K: Kernel> Sync for EventSignaler<K> {}

impl<K: Kernel> Clone for EventSignaler<K> {
    fn clone(&self) -> Self {
        // Safety: Event will panic if there are outstanding signalers referencing it.
        unsafe {
            (*self.event.as_ptr())
                .signalers
                // TODO: rationalize ordering
                .fetch_add(1, Ordering::SeqCst)
        };
        Self { event: self.event }
    }
}

impl<K: Kernel> Drop for EventSignaler<K> {
    fn drop(&mut self) {
        // SAFETY: Event will panic if there are outstanding signalers referencing it.
        unsafe {
            self.event
                .as_ref()
                .signalers
                // TODO: rationalize ordering
                .fetch_sub(1, Ordering::SeqCst)
        };
    }
}

impl<K: Kernel> EventSignaler<K> {
    /// Sets the `Event`'s state to signaled.
    ///
    /// # Interrupt context
    ///
    /// This method *is* safe to call in an interrupt context.
    pub fn signal(&self) {
        // SAFETY: Event will panic if there are outstanding signalers referencing it.
        unsafe { self.event.as_ref().signal() };
    }

    /// Sets the `Event`'s state to un-signaled.
    ///
    /// # Interrupt context
    ///
    /// This method *is* safe to call in an interrupt context.
    pub fn unsignal(&self) {
        // SAFETY: Event will panic if there are outstanding signalers referencing it.
        unsafe { self.event.as_ref().unsignal() };
    }
}

unsafe impl<K: Kernel> Sync for Event<K> {}
unsafe impl<K: Kernel> Send for Event<K> {}

impl<K: Kernel> Event<K> {
    /// Constructs a new `Event` with the given configuration.
    ///
    /// Returns the new `Event` long with an [`EventSignaler`] which can be
    /// used to signal the event.
    #[must_use]
    pub const fn new(kernel: K, config: EventConfig) -> Self {
        Self {
            config,
            state: WaitQueueLock::new(kernel, EventState { signaled: false }),
            signalers: AtomicUsize::new(1),
        }
    }

    pub fn get_signaler(event: &Event<K>) -> EventSignaler<K> {
        EventSignaler {
            event: NonNull::from_ref(event),
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
    pub fn wait_until(&self, deadline: Instant<K::Clock>) -> Result<()> {
        let mut state = self.state.lock();
        if !state.signaled {
            let (_state, result) = state.wait_until(deadline);
            return result;
        } else if self.config == EventConfig::AutoReset {
            state.signaled = false;
        }

        Ok(())
    }

    fn signal(&self) {
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

    fn unsignal(&self) {
        self.state.lock().signaled = false;
    }
}

impl<K: Kernel> Drop for Event<K> {
    fn drop(&mut self) {
        // TODO: rationalize ordering
        let num_signalers = self.signalers.load(Ordering::SeqCst);
        if num_signalers > 0 {
            pw_assert::panic!(
                "Event droped with {} active signalers",
                num_signalers as usize
            )
        }
    }
}
