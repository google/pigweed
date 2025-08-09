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
use core::ptr::NonNull;
use core::sync::atomic::Ordering;

use foreign_box::ForeignBox;
use list::*;
use pw_atomic::{AtomicAdd, AtomicLoad, AtomicSub, AtomicZero};
use pw_log::info;
use pw_status::{Error, Result};
use time::Instant;

use crate::memory::MemoryConfig as _;
use crate::scheduler::timer::TimerCallback;
use crate::sync::spinlock::SpinLockGuard;
use crate::{Arch, Kernel};

mod locks;
pub mod thread;
pub mod timer;

pub use locks::{SchedLockGuard, WaitQueueLock};
use thread::*;

const WAIT_QUEUE_DEBUG: bool = false;
macro_rules! wait_queue_debug {
  ($($args:expr),*) => {{
    log_if::debug_if!(WAIT_QUEUE_DEBUG, $($args),*)
  }}
}

// generic on `arch` instead of `kernel` as it appears in the `arch` interface.
pub struct ThreadLocalState<A: Arch> {
    pub(crate) preempt_disable_count: A::AtomicUsize,
}

impl<A: Arch> ThreadLocalState<A> {
    #[must_use]
    pub const fn new() -> Self {
        Self {
            preempt_disable_count: A::AtomicUsize::ZERO,
        }
    }
}

pub fn start_thread<K: Kernel>(kernel: K, mut thread: ForeignBox<Thread<K>>) {
    info!(
        "starting thread {} {:#x}",
        thread.name as &str,
        thread.id() as usize
    );

    pw_assert::assert!(thread.state == State::Initial);

    thread.state = State::Ready;

    let mut sched_state = kernel.get_scheduler().lock(kernel);

    // If there is a current thread, put it back on the top of the run queue.
    let id = if let Some(mut current_thread) = sched_state.current_thread.take() {
        let id = current_thread.id();
        current_thread.state = State::Ready;
        sched_state.insert_in_run_queue_head(current_thread);
        id
    } else {
        Thread::<K>::null_id()
    };

    sched_state.insert_in_run_queue_tail(thread);

    // Add this thread to the scheduler and trigger a reschedule event
    reschedule(kernel, sched_state, id);
}

pub fn initialize<K: Kernel>(kernel: K) {
    let mut sched_state = kernel.get_scheduler().lock(kernel);

    // The kernel process needs be to initialized before any kernel threads so
    // that they can properly be parented underneath it.
    unsafe {
        let kernel_process = sched_state.kernel_process.get();
        sched_state.add_process_to_list(kernel_process);
    }
}

pub fn bootstrap_scheduler<K: Kernel>(
    kernel: K,
    preempt_guard: PreemptDisableGuard<K>,
    mut thread: ForeignBox<Thread<K>>,
) -> ! {
    let mut sched_state = kernel.get_scheduler().lock(kernel);

    // TODO: assert that this is called exactly once at bootup to switch
    // to this particular thread.
    pw_assert::assert!(thread.state == State::Initial);
    thread.state = State::Ready;

    sched_state.run_queue.push_back(thread);

    info!("context switching to first thread");

    // Special case where we're switching from a non-thread to something real
    let mut temp_arch_thread_state = K::ThreadState::NEW;
    sched_state.current_arch_thread_state = &raw mut temp_arch_thread_state;

    drop(preempt_guard);

    reschedule(kernel, sched_state, Thread::<K>::null_id());
    pw_assert::panic!("should not reach here");
}

pub struct PreemptDisableGuard<K: Kernel>(K);

impl<K: Kernel> PreemptDisableGuard<K> {
    pub fn new(kernel: K) -> Self {
        let prev_count = kernel
            .thread_local_state()
            .preempt_disable_count
            .fetch_add(1, core::sync::atomic::Ordering::SeqCst);

        // atomics have wrapping semantics so overflow is explicitly checked.
        if prev_count == usize::MAX {
            pw_assert::debug_panic!("PreemptDisableGuard preempt_disable_count overflow")
        }

        Self(kernel)
    }
}

impl<K: Kernel> Drop for PreemptDisableGuard<K> {
    fn drop(&mut self) {
        let prev_count = self
            .0
            .thread_local_state()
            .preempt_disable_count
            .fetch_sub(1, core::sync::atomic::Ordering::SeqCst);

        if prev_count == 0 {
            pw_assert::debug_panic!("PreemptDisableGuard preempt_disable_count underflow")
        }
    }
}

// Global scheduler state (single processor for now)
#[allow(dead_code)]
pub struct SchedulerState<K: Kernel> {
    // The scheduler owns the kernel process from which all kernel threads
    // are parented.
    kernel_process: UnsafeCell<Process<K>>,

    current_thread: Option<ForeignBox<Thread<K>>>,
    current_arch_thread_state: *mut K::ThreadState,
    process_list: UnsafeList<Process<K>, ProcessListAdapter<K>>,
    // For now just have a single round robin list, expand to multiple queues.
    run_queue: ForeignList<Thread<K>, ThreadListAdapter<K>>,
}

unsafe impl<K: Kernel> Sync for SchedulerState<K> {}
unsafe impl<K: Kernel> Send for SchedulerState<K> {}

impl<K: Kernel> SchedulerState<K> {
    #[allow(dead_code)]
    #[allow(clippy::new_without_default)]
    pub const fn new() -> Self {
        Self {
            kernel_process: UnsafeCell::new(Process::new(
                "kernel",
                <K::ThreadState as ThreadState>::MemoryConfig::KERNEL_THREAD_MEMORY_CONFIG,
            )),
            current_thread: None,
            current_arch_thread_state: core::ptr::null_mut(),
            process_list: UnsafeList::new(),
            run_queue: ForeignList::new(),
        }
    }

    /// Returns a pointer to the current threads architecture thread state
    /// struct.
    ///
    /// Only meant to be called from within an architecture implementation.
    ///
    /// # Safety
    ///
    /// Must be called with the scheduler lock held.  Pointer is only valid
    /// while the current threads remains the current thread.
    #[allow(dead_code)]
    #[doc(hidden)]
    pub unsafe fn get_current_arch_thread_state(&mut self) -> *mut K::ThreadState {
        self.current_arch_thread_state
    }

    fn move_current_thread_to_back(&mut self) -> usize {
        let mut current_thread = self.take_current_thread();
        let current_thread_id = current_thread.id();
        current_thread.state = State::Ready;
        self.insert_in_run_queue_tail(current_thread);
        current_thread_id
    }

    #[allow(dead_code)]
    fn move_current_thread_to_front(&mut self) -> usize {
        let mut current_thread = self.take_current_thread();
        let current_thread_id = current_thread.id();
        current_thread.state = State::Ready;
        self.insert_in_run_queue_head(current_thread);
        current_thread_id
    }

    fn set_current_thread(&mut self, thread: ForeignBox<Thread<K>>) {
        self.current_arch_thread_state = thread.arch_thread_state.get();
        self.current_thread = Some(thread);
    }

    pub fn current_thread_id(&self) -> usize {
        match &self.current_thread {
            Some(thread) => thread.id(),
            None => Thread::<K>::null_id(),
        }
    }

    #[allow(dead_code)]
    pub fn current_thread_name(&self) -> &'static str {
        match &self.current_thread {
            Some(thread) => thread.name,
            None => "none",
        }
    }

    pub fn take_current_thread(&mut self) -> ForeignBox<Thread<K>> {
        let Some(thread) = self.current_thread.take() else {
            pw_assert::panic!("No current thread");
        };
        thread
    }

    #[allow(dead_code)]
    pub fn current_thread(&self) -> &Thread<K> {
        let Some(thread) = &self.current_thread else {
            pw_assert::panic!("No current thread");
        };
        thread
    }

    #[allow(dead_code)]
    pub fn current_thread_mut(&mut self) -> &mut Thread<K> {
        let Some(thread) = &mut self.current_thread else {
            pw_assert::panic!("No current thread");
        };
        thread
    }

    /// # Safety
    ///
    /// This method has the same safety preconditions as
    /// [`UnsafeList::push_front_unchecked`].
    #[allow(dead_code)]
    #[inline(never)]
    pub unsafe fn add_process_to_list(&mut self, process: *mut Process<K>) {
        unsafe {
            self.process_list.push_front_unchecked(process);
        }
    }

    #[allow(dead_code)]
    pub fn dump_all_threads(&self) {
        info!("list of all threads:");
        unsafe {
            let _ = self
                .process_list
                .for_each(|process| -> core::result::Result<(), ()> {
                    process.dump();
                    Ok(())
                });
        }
    }

    #[allow(dead_code)]
    fn insert_in_run_queue_head(&mut self, thread: ForeignBox<Thread<K>>) {
        pw_assert::assert!(thread.state == State::Ready);
        // info!("pushing thread {:#x} on run queue head", thread.id());

        self.run_queue.push_front(thread);
    }

    #[allow(dead_code)]
    fn insert_in_run_queue_tail(&mut self, thread: ForeignBox<Thread<K>>) {
        pw_assert::assert!(thread.state == State::Ready);
        // info!("pushing thread {:#x} on run queue tail", thread.id());

        self.run_queue.push_back(thread);
    }
}

impl<K: Kernel> SpinLockGuard<'_, K, SchedulerState<K>> {
    /// Reschedule if preemption is enabled
    fn try_reschedule(mut self, kernel: K) -> Self {
        if kernel
            .thread_local_state()
            .preempt_disable_count
            .load(Ordering::SeqCst)
            == 1
        {
            let current_thread_id = self.move_current_thread_to_back();
            reschedule(kernel, self, current_thread_id)
        } else {
            self
        }
    }
}

#[allow(dead_code)]
fn reschedule<K: Kernel>(
    kernel: K,
    mut sched_state: SpinLockGuard<K, SchedulerState<K>>,
    current_thread_id: usize,
) -> SpinLockGuard<K, SchedulerState<K>> {
    // Caller to reschedule is responsible for removing current thread and
    // put it in the correct run/wait queue.
    pw_assert::assert!(sched_state.current_thread.is_none());

    // Validate that the only mechanism disabling preemption is the scheduler
    // lock which is passed in to this function.
    pw_assert::assert!(
        kernel
            .thread_local_state()
            .preempt_disable_count
            .load(Ordering::SeqCst)
            <= 1,
        "Preemption count greater than 1"
    );

    // Pop a new thread off the head of the run queue.
    // At the moment cannot handle an empty queue, so will panic in that case.
    // TODO: Implement either an idle thread or a special idle routine for that case.
    let Some(mut new_thread) = sched_state.run_queue.pop_head() else {
        pw_assert::panic!("run_queue empty");
    };

    pw_assert::assert!(
        new_thread.state == State::Ready,
        "<{}>({:#x}) not ready",
        new_thread.name as &str,
        new_thread.id() as usize,
    );
    new_thread.state = State::Running;

    if current_thread_id == new_thread.id() {
        sched_state.current_thread = Some(new_thread);
        return sched_state;
    }

    let old_thread_state = sched_state.current_arch_thread_state;
    let new_thread_state = new_thread.arch_thread_state.get();
    sched_state.set_current_thread(new_thread);
    unsafe { kernel.context_switch(sched_state, old_thread_state, new_thread_state) }
}

#[allow(dead_code)]
pub fn yield_timeslice<K: Kernel>(kernel: K) {
    // info!("yielding thread {:#x}", current_thread.id());
    let sched_state = kernel.get_scheduler().lock(kernel);
    sched_state.try_reschedule(kernel);
}

#[allow(dead_code)]
fn preempt<K: Kernel>(kernel: K) {
    // info!("preempt thread {:#x}", current_thread.id());
    let mut sched_state = kernel.get_scheduler().lock(kernel);

    // For now, always move the current thread to the back of the run queue.
    // When the scheduler gets more complex, it should evaluate if it has used
    // up it's time allocation.
    let current_thread_id = sched_state.move_current_thread_to_back();

    reschedule(kernel, sched_state, current_thread_id);
}

// Tick that is called from a timer handler. The scheduler will evaluate if the current thread
// should be preempted or not
#[allow(dead_code)]
pub fn tick<K: Kernel>(kernel: K, now: Instant<K::Clock>) {
    //info!("tick {} ms", time_ms);
    pw_assert::assert!(
        kernel
            .thread_local_state()
            .preempt_disable_count
            .load(Ordering::SeqCst)
            == 0,
        "scheduler::tick() called with preemption disabled"
    );

    let guard = PreemptDisableGuard::new(kernel);

    // In lieu of a proper timer interface, the scheduler needs to be robust
    // to timer ticks arriving before it is initialized.
    if kernel.get_scheduler().lock(kernel).current_thread.is_none() {
        return;
    }

    timer::process_queue(kernel, now);
    drop(guard);

    kernel.get_scheduler().lock(kernel).try_reschedule(kernel);
}

// Exit the current thread.
// For now, simply remove ourselves from the run queue. No cleanup of thread resources
// is performed.
#[allow(dead_code)]
pub fn exit_thread<K: Kernel>(kernel: K) -> ! {
    let mut sched_state = kernel.get_scheduler().lock(kernel);

    let mut current_thread = sched_state.take_current_thread();
    let current_thread_id = current_thread.id();

    info!("thread {:#x} exiting", current_thread.id() as usize);
    current_thread.state = State::Stopped;

    reschedule(kernel, sched_state, current_thread_id);

    // Should not get here
    #[allow(clippy::empty_loop)]
    loop {}
}

pub fn sleep_until<K: Kernel>(kernel: K, deadline: Instant<K::Clock>) {
    let wait_queue = WaitQueueLock::new(kernel, ());
    let _ = wait_queue.lock().wait_until(deadline);
}

pub struct WaitQueue<K: Kernel> {
    queue: ForeignList<Thread<K>, ThreadListAdapter<K>>,
}

unsafe impl<K: Kernel> Sync for WaitQueue<K> {}
unsafe impl<K: Kernel> Send for WaitQueue<K> {}

impl<K: Kernel> WaitQueue<K> {
    #[allow(dead_code, clippy::new_without_default)]
    #[must_use]
    pub const fn new() -> Self {
        Self {
            queue: ForeignList::new(),
        }
    }
}

#[derive(Eq, PartialEq)]
pub enum WakeResult {
    Woken,
    QueueEmpty,
}

impl<K: Kernel> SchedLockGuard<'_, K, WaitQueue<K>> {
    fn add_to_queue_and_reschedule(mut self, mut thread: ForeignBox<Thread<K>>) -> Self {
        let current_thread_id = thread.id();
        let current_thread_name = thread.name;
        thread.state = State::Waiting;
        self.queue.push_back(thread);
        wait_queue_debug!("<{}> rescheduling", current_thread_name as &str);
        self.reschedule(current_thread_id)
    }

    // Safety:
    // Caller guarantees that thread is non-null, valid, and process_timeout
    // has exclusive access to `waiting_thread`.
    unsafe fn process_timeout(&mut self, waiting_thread: *mut Thread<K>) -> Option<Error> {
        if unsafe { (*waiting_thread).state } != State::Waiting {
            // Thread has already been woken.
            return None;
        }

        let Some(mut thread) = (unsafe {
            self.queue
                .remove_element(NonNull::new_unchecked(waiting_thread))
        }) else {
            pw_assert::panic!("thread no longer in wait queue");
        };

        wait_queue_debug!("<{}> timeout", thread.name as &str);
        thread.state = State::Ready;
        self.sched_mut().run_queue.push_back(thread);
        Some(Error::DeadlineExceeded)
    }

    #[allow(clippy::must_use_candidate)]
    pub fn wake_one(mut self) -> (Self, WakeResult) {
        let Some(mut thread) = self.queue.pop_head() else {
            return (self, WakeResult::QueueEmpty);
        };
        wait_queue_debug!("waking <{}>", thread.name as &str);
        thread.state = State::Ready;
        self.sched_mut().run_queue.push_back(thread);
        (self.try_reschedule(), WakeResult::Woken)
    }

    #[allow(clippy::return_self_not_must_use, clippy::must_use_candidate)]
    pub fn wake_all(mut self) -> Self {
        loop {
            let result;
            (self, result) = self.wake_one();
            if result == WakeResult::QueueEmpty {
                return self;
            }
        }
    }

    #[allow(clippy::return_self_not_must_use, clippy::must_use_candidate)]
    pub fn wait(mut self) -> Self {
        let thread = self.sched_mut().take_current_thread();
        wait_queue_debug!("<{}> waiting", thread.name as &str);
        self = self.add_to_queue_and_reschedule(thread);
        wait_queue_debug!("<{}> back", self.sched().current_thread_name() as &str);
        self
    }

    pub fn wait_until(mut self, deadline: Instant<K::Clock>) -> (Self, Result<()>) {
        let mut thread = self.sched_mut().take_current_thread();
        wait_queue_debug!("<{}> wait_until", thread.name as &str);

        // Smuggle references to the thread and wait queue into the callback.
        // Safety:
        // * The thread will always exists (TODO: support thread termination)
        // * The wait queue will outlive the callback because it will either
        //   fire while the thread is in the wait queue or will be the timer
        //   will be canceled before this function returns.
        // * All access to thread_ptr and wait_queue_ptr in the callback are
        //   done while the wait queue lock is held.
        let thread_ptr = unsafe { thread.as_mut_ptr() };
        let smuggled_wait_queue = unsafe { self.smuggle() };

        // Safety:
        // * Only accessed while the wait_queue_lock is held;
        let result: UnsafeCell<Result<()>> = UnsafeCell::new(Ok(()));
        let result_ptr = result.get();

        // Timeout callback will remove the thread from the wait queue and put
        // it back on the run queue.
        let mut callback_closure = move |callback: ForeignBox<TimerCallback<K::Clock>>, _now| {
            // Safety: wait queue lock is valid for the lifetime of the callback.
            let mut wait_queue = unsafe { smuggled_wait_queue.lock() };

            // Safety: the wait queue lock protects access to the thread.
            wait_queue_debug!(
                "timeout callback for {} ({})",
                unsafe { (*thread_ptr).name } as &str,
                unsafe { thread::to_string((*thread_ptr).state) } as &str
            );

            // Safety: We know that thread_ptr is valid for the life of `wait_until`
            // and this callback will either be called or canceled before `wait_until`
            // returns.
            if let Some(error) = unsafe { wait_queue.process_timeout(thread_ptr) } {
                // Safety: Acquisition of the wait queue lock at the beginning of
                // the callback ensures mutual exclusion with accesses from the
                // body of `wait_until`.
                unsafe { result_ptr.write_volatile(Err(error)) };
            }

            let _ = callback.consume();
            None // Don't re-arm
        };

        let mut callback = TimerCallback::new(deadline, unsafe {
            ForeignBox::new_from_ptr(&raw mut callback_closure)
        });
        let callback_ptr = &raw mut callback;
        timer::schedule_timer(self.kernel, unsafe {
            ForeignBox::new_from_ptr(callback_ptr)
        });

        // Safety: It is important hold on to the WaitQueue lock that is returned
        // from reschedule as the pointers needed by the timer canceling code
        // below rely on it for correctness.
        self = self.add_to_queue_and_reschedule(thread);

        wait_queue_debug!("<{}> back", self.sched().current_thread_name() as &str);

        // Cancel timeout callback if has not already fired.
        //
        // Safety: callback_ptr is valid until callback goes out of scope.
        unsafe {
            timer::cancel_and_consume_timer(self.kernel, NonNull::new_unchecked(callback_ptr))
        };

        wait_queue_debug!(
            "<{}> exiting wait_until",
            self.sched().current_thread_name() as &str
        );

        // Safety:
        //
        // At this point the thread will be in the run queue by virtue of
        // `reschedule()` return and the timer callback will have fired or be
        // canceled.  This leaves not dangling references to our "smuggled"
        // pointers.
        //
        // It is also now safe to read the result UnsafeCell
        (self, unsafe { result.get().read_volatile() })
    }
}
