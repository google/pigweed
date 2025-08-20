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

use core::any::Any;
use core::cell::UnsafeCell;
use core::ptr::NonNull;

use bitflags::bitflags;
use foreign_box::{ForeignBox, ForeignRc};
use list::{self, Link, RandomAccessForeignList};
use pw_status::{Error, Result};
use time::Instant;

use crate::Kernel;
use crate::sync::event::{Event, EventConfig, EventSignaler};
use crate::sync::spinlock::SpinLock;

/// Trait that all kernel objects implement.
///
/// The methods on this trait map directly to the kernel's system calls.
pub trait KernelObject<K: Kernel>: Any + Send + Sync {
    /// Wait on a set of signals to be active.
    ///
    /// Blocks until any of the signals in `signal_mask` are active on the object
    /// or `deadline` has expired.
    fn object_wait(&self, ctx: K, signal_mask: Signals, deadline: Instant<K::Clock>) -> Result<()>;
}

/// Demo kernel object that signals based off of a timer.
pub struct TickerObject<K: Kernel> {
    base: ObjectBase<K>,
}

impl<K: Kernel> TickerObject<K> {
    #[must_use]
    pub const fn new() -> Self {
        Self {
            base: ObjectBase::new(),
        }
    }
    pub fn tick(&self, kernel: K) {
        self.base.signal(kernel, Signals(0x1));
        self.base.signal(kernel, Signals(0));
    }
}

impl<K: Kernel> KernelObject<K> for TickerObject<K> {
    fn object_wait(
        &self,
        kernel: K,
        signal_mask: Signals,
        deadline: Instant<K::Clock>,
    ) -> Result<()> {
        self.base.wait_until(kernel, signal_mask, deadline).map(|_|
                 //  wait result TBD
            ())
    }
}

list::define_adapter!(pub ObjectWaiterListAdapter<K: Kernel> => ObjectWaiter<K>::link);

#[derive(Copy, Clone)]
pub struct Signals(u32);

bitflags! {
    impl Signals: u32 {
        const Readable = 1 << 0;
        const Writeable = 1 << 1;
        const Error = 1 << 2;
        const User = 1 << 16;
    }
}

impl Signals {
    #[must_use]
    pub const fn new() -> Self {
        Self(0)
    }
}

struct WaitResult {
    result: UnsafeCell<Result<Signals>>,
}

impl WaitResult {
    pub const fn new() -> Self {
        Self {
            result: UnsafeCell::new(Err(Error::Unknown)),
        }
    }

    /// Safety: Users of [`WaitResult::get()`] and [`WaitResult::set()`] must
    /// ensure that their calls into them are not done concurrently.
    unsafe fn get(&self) -> Result<Signals> {
        unsafe { *self.result.get() }
    }

    /// Safety: Users of [`WaitResult::get()`] and [`WaitResult::set()`] must
    /// ensure that their calls into them are not done concurrently.
    unsafe fn set(&self, result: Result<Signals>) {
        unsafe { (*self.result.get()) = result }
    }
}

struct ObjectWaiter<K: Kernel> {
    link: Link,
    signaler: EventSignaler<K>,
    signal_mask: Signals,
    wait_result: WaitResult,
}

pub struct ObjectBaseState<K: Kernel> {
    active_signals: Signals,
    waiters: RandomAccessForeignList<ObjectWaiter<K>, ObjectWaiterListAdapter<K>>,
}

impl<K: Kernel> ObjectBaseState<K> {
    #[must_use]
    pub const fn new() -> Self {
        Self {
            active_signals: Signals::new(),
            waiters: RandomAccessForeignList::new(),
        }
    }
}

/// Translates handles to dynamic references to kernel objects.
///
/// `ObjectTable` is a trait to allow for both static and dynamic tables to be
/// used in the same time.
pub trait ObjectTable<K: Kernel> {
    fn get_object(
        &self,
        kernel: K,
        handle: u32,
    ) -> Option<ForeignRc<K::AtomicUsize, dyn KernelObject<K>>>;
}

/// An object table with no entries.
///
/// This may be replaced with a static object tables with no entries.
pub struct NullObjectTable {}

impl NullObjectTable {
    #[must_use]
    pub const fn new() -> Self {
        Self {}
    }
}

impl<K: Kernel> ObjectTable<K> for NullObjectTable {
    fn get_object(
        &self,
        kernel: K,
        handle: u32,
    ) -> Option<ForeignRc<K::AtomicUsize, dyn KernelObject<K>>> {
        match handle {
            // For development purposes, we cheat and hard code a ticker object
            // into every table.  This goes away when "real" objects exist and
            // we have per process object tables that are configured through the
            // system generator.
            0 => kernel
                .get_state()
                .ticker
                .lock(kernel)
                .as_ref()
                .map(Clone::clone),
            _ => None,
        }
    }
}

/// Common functionality used by many kernel objects
pub struct ObjectBase<K: Kernel> {
    state: SpinLock<K, ObjectBaseState<K>>,
}

impl<K: Kernel> ObjectBase<K> {
    #[must_use]
    pub const fn new() -> Self {
        Self {
            state: SpinLock::new(ObjectBaseState::new()),
        }
    }
}

impl<K: Kernel> ObjectBase<K> {
    pub fn wait_until(
        &self,
        kernel: K,
        signal_mask: Signals,
        deadline: Instant<K::Clock>,
    ) -> Result<Signals> {
        let mut state = self.state.lock(kernel);

        // Skip waiting if signals are already pending.
        if state.active_signals.contains(signal_mask) {
            return Ok(state.active_signals);
        }

        let event = Event::new(kernel, EventConfig::ManualReset);
        let signaler = event.get_signaler();

        let waiter = ObjectWaiter {
            link: Link::new(),
            signaler,
            signal_mask,
            wait_result: WaitResult::new(),
        };

        // Safety: `waiter_box`'s membership in `self.waiters` is managed by
        // and never outlives this function's lifetime.
        let waiter_box = unsafe { ForeignBox::new(NonNull::from_ref(&waiter)) };

        let key = state.waiters.push_back(waiter_box);

        // Drop state lock while waiting.
        drop(state);
        let wait_result = event.wait_until(deadline);
        let mut state = self.state.lock(kernel);

        // Before processing the wait result, we remove the waiter from the queue.
        let waiter_box = state.waiters.remove_element(key);

        let result = match wait_result {
            Err(e) => Err(e),
            // Safety: Since `waiter_box` has been removed from the object's
            // `waiters` list, there is no way for another thread to have a
            // reference to the waiter.
            _ => unsafe { waiter_box.wait_result.get() },
        };

        // `waiter_box` is no longer referenced and is safe to consume.
        waiter_box.consume();

        result
    }

    pub fn signal(&self, kernel: K, active_signals: Signals) {
        let mut state = self.state.lock(kernel);
        state.active_signals = active_signals;

        let _ = state.waiters.for_each(|waiter| -> Result<()> {
            if waiter.signal_mask.contains(active_signals) {
                // Safety: While a waiter is in an object's `waiters` list, that
                // object has exclusive access to the waiter.  The below
                // operation is done with the object's spinlock held.
                unsafe { waiter.wait_result.set(Ok(active_signals)) };
                waiter.signaler.signal();
            }
            Ok(())
        });
    }
}
