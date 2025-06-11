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

use foreign_box::ForeignBox;
use list::*;
use pw_log::info;
use pw_status::{Error, Result};
use thread::*;

use crate::arch::{Arch, ArchInterface, ArchThreadState, ThreadState};
use crate::sync::spinlock::{SpinLock, SpinLockGuard};
use crate::timer::{Instant, TimerCallback, TimerQueue};

mod locks;
pub(crate) mod thread;

pub use locks::{SchedLockGuard, WaitQueueLock};

const WAIT_QUEUE_DEBUG: bool = false;
macro_rules! wait_queue_debug {
  ($($args:expr),*) => {{
    log_if::debug_if!(WAIT_QUEUE_DEBUG, $($args),*)
  }}
}

pub fn start_thread(mut thread: ForeignBox<Thread>) {
    info!(
        "starting thread {} {:#x}",
        thread.name as &str,
        thread.id() as usize
    );

    pw_assert::assert!(thread.state == State::Initial);

    thread.state = State::Ready;

    let mut sched_state = SCHEDULER_STATE.lock();

    // If there is a current thread, put it back on the top of the run queue.
    let id = if let Some(mut current_thread) = sched_state.current_thread.take() {
        let id = current_thread.id();
        current_thread.state = State::Ready;
        sched_state.insert_in_run_queue_head(current_thread);
        id
    } else {
        Thread::null_id()
    };

    sched_state.insert_in_run_queue_tail(thread);

    // Add this thread to the scheduler and trigger a reschedule event
    reschedule(sched_state, id);
}

pub fn initialize() {
    let mut sched_state = SCHEDULER_STATE.lock();

    // The kernel process needs be to initialized before any kernel threads so
    // that they can properly be parented underneath it.
    unsafe {
        let kernel_process = sched_state.kernel_process.get();
        sched_state.add_process_to_list(kernel_process);
    }
}

pub fn bootstrap_scheduler(mut thread: ForeignBox<Thread>) -> ! {
    let mut sched_state = SCHEDULER_STATE.lock();

    // TODO: assert that this is called exactly once at bootup to switch
    // to this particular thread.
    pw_assert::assert!(thread.state == State::Initial);
    thread.state = State::Ready;

    sched_state.run_queue.push_back(thread);

    info!("context switching to first thread");

    // Special case where we're switching from a non-thread to something real
    let mut temp_arch_thread_state = ArchThreadState::new();
    sched_state.current_arch_thread_state = &raw mut temp_arch_thread_state;

    reschedule(sched_state, Thread::null_id());
    pw_assert::panic!("should not reach here");
}

pub struct PremptDisableGuard;

impl PremptDisableGuard {
    pub fn new() -> Self {
        let mut sched_state = SCHEDULER_STATE.lock();
        let thread = sched_state.current_thread_mut();

        #[allow(clippy::needless_else)]
        if let Some(val) = thread.preempt_disable_count.checked_add(1) {
            thread.preempt_disable_count = val;
        } else {
            pw_assert::debug_panic!("PremptDisableGuard preempt_disable_count overflow")
        }

        Self
    }
}

impl Drop for PremptDisableGuard {
    fn drop(&mut self) {
        let mut sched_state = SCHEDULER_STATE.lock();
        let thread = sched_state.current_thread_mut();

        if let Some(val) = thread.preempt_disable_count.checked_sub(1) {
            thread.preempt_disable_count = val;
        } else {
            // use panic, not debug_panic, as it's possible a
            // real bug could trigger this assert.
            pw_assert::panic!("PremptDisableGuard drop past zero")
        }

        if thread.preempt_disable_count == 0 {
            preempt();
        }
    }
}

// Global scheduler state (single processor for now)
#[allow(dead_code)]
pub struct SchedulerState {
    // The scheduler owns the kernel process from which all kernel threads
    // are parented.
    kernel_process: UnsafeCell<Process>,

    current_thread: Option<ForeignBox<Thread>>,
    current_arch_thread_state: *mut ArchThreadState,
    process_list: UnsafeList<Process, ProcessListAdapter>,
    // For now just have a single round robin list, expand to multiple queues.
    run_queue: ForeignList<Thread, ThreadListAdapter>,
}

pub static SCHEDULER_STATE: SpinLock<SchedulerState> = SpinLock::new(SchedulerState::new());

unsafe impl Sync for SchedulerState {}
unsafe impl Send for SchedulerState {}
impl SchedulerState {
    #[allow(dead_code)]
    const fn new() -> Self {
        Self {
            kernel_process: UnsafeCell::new(Process::new(
                "kernel",
                <Arch as ArchInterface>::MemoryConfig::KERNEL_THREAD_MEMORY_CONFIG,
            )),
            current_thread: None,
            current_arch_thread_state: core::ptr::null_mut(),
            process_list: UnsafeList::new(),
            run_queue: ForeignList::new(),
        }
    }

    #[allow(dead_code)]
    pub(super) unsafe fn get_current_arch_thread_state(&mut self) -> *mut ArchThreadState {
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

    fn set_current_thread(&mut self, thread: ForeignBox<Thread>) {
        self.current_arch_thread_state = thread.arch_thread_state.get();
        self.current_thread = Some(thread);
    }

    pub fn current_thread_id(&self) -> usize {
        match &self.current_thread {
            Some(thread) => thread.id(),
            None => Thread::null_id(),
        }
    }

    #[allow(dead_code)]
    pub fn current_thread_name(&self) -> &'static str {
        match &self.current_thread {
            Some(thread) => thread.name,
            None => "none",
        }
    }

    pub fn take_current_thread(&mut self) -> ForeignBox<Thread> {
        let Some(thread) = self.current_thread.take() else {
            pw_assert::panic!("No current thread");
        };
        thread
    }

    #[allow(dead_code)]
    pub fn current_thread(&self) -> &Thread {
        let Some(thread) = &self.current_thread else {
            pw_assert::panic!("No current thread");
        };
        thread
    }

    #[allow(dead_code)]
    pub fn current_thread_mut(&mut self) -> &mut Thread {
        let Some(thread) = &mut self.current_thread else {
            pw_assert::panic!("No current thread");
        };
        thread
    }

    #[allow(dead_code)]
    #[inline(never)]
    pub unsafe fn add_process_to_list(&mut self, process: *mut Process) {
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
    fn insert_in_run_queue_head(&mut self, thread: ForeignBox<Thread>) {
        pw_assert::assert!(thread.state == State::Ready);
        // info!("pushing thread {:#x} on run queue head", thread.id());

        self.run_queue.push_front(thread);
    }

    #[allow(dead_code)]
    fn insert_in_run_queue_tail(&mut self, thread: ForeignBox<Thread>) {
        pw_assert::assert!(thread.state == State::Ready);
        // info!("pushing thread {:#x} on run queue tail", thread.id());

        self.run_queue.push_back(thread);
    }
}

impl SpinLockGuard<'_, SchedulerState> {
    /// Reschedule if preemption is enabled
    fn try_reschedule(mut self) -> Self {
        if self.current_thread().preempt_disable_count == 0 {
            let current_thread_id = self.move_current_thread_to_back();
            reschedule(self, current_thread_id)
        } else {
            self
        }
    }
}

#[allow(dead_code)]
fn reschedule(
    mut sched_state: SpinLockGuard<SchedulerState>,
    current_thread_id: usize,
) -> SpinLockGuard<SchedulerState> {
    // Caller to reschedule is responsible for removing current thread and
    // put it in the correct run/wait queue.
    pw_assert::assert!(sched_state.current_thread.is_none());

    // info!("reschedule");

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
        // info!("decided to continue running thread {:#x}", new_thread.id());
        return sched_state;
    }

    // info!("switching to thread {:#x}", new_thread.id());
    unsafe {
        let old_thread_state = sched_state.current_arch_thread_state;
        let new_thread_state = new_thread.arch_thread_state.get();
        sched_state.set_current_thread(new_thread);
        <Arch as ArchInterface>::ThreadState::context_switch(
            sched_state,
            old_thread_state,
            new_thread_state,
        )
    }
}

#[allow(dead_code)]
pub fn yield_timeslice() {
    // info!("yielding thread {:#x}", current_thread.id());
    let mut sched_state = SCHEDULER_STATE.lock();

    // Yielding always moves the current task to the back of the run queue
    let current_thread_id = sched_state.move_current_thread_to_back();

    reschedule(sched_state, current_thread_id);
}

#[allow(dead_code)]
pub fn preempt() {
    // info!("preempt thread {:#x}", current_thread.id());
    let mut sched_state = SCHEDULER_STATE.lock();

    // For now, always move the current thread to the back of the run queue.
    // When the scheduler gets more complex, it should evaluate if it has used
    // up it's time allocation.
    let current_thread_id = sched_state.move_current_thread_to_back();

    reschedule(sched_state, current_thread_id);
}

// Tick that is called from a timer handler. The scheduler will evaluate if the current thread
// should be preempted or not
#[allow(dead_code)]
pub fn tick(now: Instant) {
    //info!("tick {} ms", time_ms);

    // In lieu of a proper timer interface, the scheduler needs to be robust
    // to timer ticks arriving before it is initialized.
    if SCHEDULER_STATE.lock().current_thread.is_none() {
        return;
    }

    let _guard = PremptDisableGuard::new();
    TimerQueue::process_queue(now);
}

// Exit the current thread.
// For now, simply remove ourselves from the run queue. No cleanup of thread resources
// is performed.
#[allow(dead_code)]
pub fn exit_thread() -> ! {
    let mut sched_state = SCHEDULER_STATE.lock();

    let mut current_thread = sched_state.take_current_thread();
    let current_thread_id = current_thread.id();

    info!("thread {:#x} exiting", current_thread.id() as usize);
    current_thread.state = State::Stopped;

    reschedule(sched_state, current_thread_id);

    // Should not get here
    #[allow(clippy::empty_loop)]
    loop {}
}

pub fn sleep_until(deadline: Instant) {
    let wait_queue = WaitQueueLock::new(());
    let _ = wait_queue.lock().wait_until(deadline);
}

pub struct WaitQueue {
    queue: ForeignList<Thread, ThreadListAdapter>,
}

unsafe impl Sync for WaitQueue {}
unsafe impl Send for WaitQueue {}

impl WaitQueue {
    #[allow(dead_code)]
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

impl SchedLockGuard<'_, WaitQueue> {
    fn add_to_queue_and_reschedule(mut self, mut thread: ForeignBox<Thread>) -> Self {
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
    unsafe fn process_timeout(&mut self, waiting_thread: *mut Thread) -> Option<Error> {
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

    pub fn wake_one(mut self) -> (Self, WakeResult) {
        let Some(mut thread) = self.queue.pop_head() else {
            return (self, WakeResult::QueueEmpty);
        };
        wait_queue_debug!("waking <{}>", thread.name as &str);
        thread.state = State::Ready;
        self.sched_mut().run_queue.push_back(thread);
        (self.try_reschedule(), WakeResult::Woken)
    }

    pub fn wake_all(mut self) -> Self {
        loop {
            let result;
            (self, result) = self.wake_one();
            if result == WakeResult::QueueEmpty {
                return self;
            }
        }
    }

    pub fn wait(mut self) -> Self {
        let thread = self.sched_mut().take_current_thread();
        wait_queue_debug!("<{}> waiting", thread.name as &str);
        self = self.add_to_queue_and_reschedule(thread);
        wait_queue_debug!("<{}> back", self.sched().current_thread_name() as &str);
        self
    }

    pub fn wait_until(mut self, deadline: Instant) -> (Self, Result<()>) {
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
        let mut callback_closure = move |callback: ForeignBox<TimerCallback>, _now| {
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
        };

        let mut callback = TimerCallback::new(deadline, unsafe {
            ForeignBox::new_from_ptr(&raw mut callback_closure)
        });
        let callback_ptr = &raw mut callback;
        TimerQueue::schedule_timer(unsafe { ForeignBox::new_from_ptr(callback_ptr) });

        // Safety: It is important hold on to the WaitQueue lock that is returned
        // from reschedule as the pointers needed by the timer canceling code
        // below rely on it for correctness.
        self = self.add_to_queue_and_reschedule(thread);

        wait_queue_debug!("<{}> back", self.sched().current_thread_name() as &str);

        // Cancel timeout callback if has not already fired.
        //
        // Safety: callback_ptr is valid until callback goes out of scope.
        unsafe { TimerQueue::cancel_and_consume_timer(NonNull::new_unchecked(callback_ptr)) };

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
