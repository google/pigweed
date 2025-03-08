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
use core::mem::offset_of;

use foreign_box::ForeignBox;
use list::*;
use pw_log::info;

use crate::arch::{Arch, ArchInterface, ArchThreadState, ThreadState};
use crate::sync::spinlock::{SpinLock, SpinLockGuard};

mod locks;

pub use locks::{SchedLock, SchedLockGuard, WaitQueueLock};

#[derive(Clone, Copy)]
pub struct Stack {
    start: *const u8,
    end: *const u8,
}

#[allow(dead_code)]
impl Stack {
    pub const fn from_slice(slice: &[u8]) -> Self {
        let start: *const u8 = slice.as_ptr();
        // Safety: offset based on known size of slice.
        let end = unsafe { start.add(slice.len() - 1) };
        Self { start, end }
    }

    const fn new() -> Self {
        Self {
            start: core::ptr::null(),
            end: core::ptr::null(),
        }
    }

    pub fn start(self) -> *const u8 {
        self.start
    }
    pub fn end(self) -> *const u8 {
        self.end
    }
}

// TODO: want to name this ThreadState, but collides with ArchThreadstate
#[derive(Copy, Clone, PartialEq)]
enum State {
    New,
    Initial,
    Ready,
    Running,
    Stopped,
    Waiting,
}

// TODO: use From or Into trait (unclear how to do it with 'static str)
fn to_string(s: State) -> &'static str {
    match s {
        State::New => "New",
        State::Initial => "Initial",
        State::Ready => "Ready",
        State::Running => "Running",
        State::Stopped => "Stopped",
        State::Waiting => "Waiting",
    }
}

pub struct Thread {
    // List of the threads in the system
    pub global_link: Link,

    // Active state link (run queue, wait queue, etc)
    pub active_link: Link,

    state: State,
    stack: Stack,

    // Architecturally specific thread state, saved on context switch
    pub arch_thread_state: UnsafeCell<ArchThreadState>,
}

pub struct ThreadListAdapter {}

impl list::Adapter for ThreadListAdapter {
    const LINK_OFFSET: usize = offset_of!(Thread, active_link);
}

pub struct GlobalThreadListAdapter {}

impl list::Adapter for GlobalThreadListAdapter {
    const LINK_OFFSET: usize = offset_of!(Thread, global_link);
}

impl Thread {
    // Create an empty, uninitialzed thread
    pub fn new() -> Self {
        Thread {
            global_link: Link::new(),
            active_link: Link::new(),
            state: State::New,
            arch_thread_state: UnsafeCell::new(ThreadState::new()),
            stack: Stack::new(),
        }
    }

    // Initialize the mutable parts of the thread, must be called once per
    // thread prior to starting it
    #[allow(dead_code)]
    pub fn initialize(&mut self, stack: Stack, entry_point: fn(usize), arg: usize) -> &mut Thread {
        assert!(self.state == State::New);
        self.stack = stack;

        // Call the arch to arrange for the thread to start directly
        unsafe {
            (*self.arch_thread_state.get()).initialize_frame(stack, entry_point, arg);
        }
        self.state = State::Initial;

        // Add our list to the global thread list
        SCHEDULER_STATE.lock().add_thread_to_list(self);

        self
    }

    #[allow(dead_code)]
    pub fn start(mut thread: ForeignBox<Self>) {
        info!("starting thread {:#x}", thread.id() as usize);

        assert!(thread.state == State::Initial);
        thread.state = State::Ready;

        let mut sched_state = SCHEDULER_STATE.lock();

        // If there is a current thread, put it back on the top of the run queue.
        let id = if let Some(mut current_thread) = sched_state.current_thread.take() {
            let id = current_thread.id();
            current_thread.state = State::Ready;
            sched_state.insert_in_run_queue_head(current_thread);
            id
        } else {
            Self::null_id()
        };

        sched_state.insert_in_run_queue_tail(thread);

        // Add this thread to the scheduler and trigger a reschedule event
        reschedule(sched_state, id);
    }

    // Dump to the console useful information about this thread
    #[allow(dead_code)]
    pub fn dump(&self) {
        info!(
            "thread {:#x} state {}",
            self.id() as usize,
            to_string(self.state) as &str
        );
    }

    // A simple id for debugging purposes, currently the pointer to the thread structure itself
    pub fn id(&self) -> usize {
        core::ptr::from_ref(self) as usize
    }

    // An id that can not be assigned to any thread in the system.
    pub const fn null_id() -> usize {
        // `core::ptr::null::<Self>() as usize` can not be evaluated at const time
        // and a null pointer is defined to be at address 0 (see
        // https://doc.rust-lang.org/beta/core/ptr/fn.null.html).
        0usize
    }
}

pub fn bootstrap_scheduler(mut thread: ForeignBox<Thread>) -> ! {
    let mut sched_state = SCHEDULER_STATE.lock();

    // TODO: assert that this is called exactly once at bootup to switch
    // to this particular thread.
    assert!(thread.state == State::Initial);
    thread.state = State::Ready;

    sched_state.run_queue.push_back(thread);

    info!("context switching to first thread");

    // Special case where we're switching from a non-thread to something real
    let mut temp_arch_thread_state = ArchThreadState::new();
    sched_state.current_arch_thread_state = &raw mut temp_arch_thread_state;

    reschedule(sched_state, Thread::null_id());
    panic!("should not reach here");
}

// Global scheduler state (single processor for now)
#[allow(dead_code)]
pub struct SchedulerState {
    current_thread: Option<ForeignBox<Thread>>,
    current_arch_thread_state: *mut ArchThreadState,
    thread_list: UnsafeList<Thread, GlobalThreadListAdapter>,
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
            current_thread: None,
            current_arch_thread_state: core::ptr::null_mut(),
            thread_list: UnsafeList::new(),
            run_queue: ForeignList::new(),
        }
    }

    #[allow(dead_code)]
    pub(super) unsafe fn get_current_arch_thread_state(&mut self) -> *mut ArchThreadState {
        self.current_arch_thread_state
    }

    fn move_current_thread_to_back(&mut self) -> usize {
        let Some(mut current_thread) = self.current_thread.take() else {
            panic!("no current thread");
        };
        let current_thread_id = current_thread.id();
        current_thread.state = State::Ready;
        self.insert_in_run_queue_tail(current_thread);
        current_thread_id
    }

    fn move_current_thread_to_front(&mut self) -> usize {
        let Some(mut current_thread) = self.current_thread.take() else {
            panic!("no current thread");
        };
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
    #[inline(never)]
    pub fn add_thread_to_list(&mut self, thread: &mut Thread) {
        unsafe {
            self.thread_list.push_front_unchecked(thread);
        }
    }

    #[allow(dead_code)]
    pub fn dump_all_threads(&self) {
        info!("list of all threads:");
        unsafe {
            let _ = self.thread_list.for_each(|thread| -> Result<(), ()> {
                //                info!("ptr {:#x}", thread.id());
                thread.dump();
                Ok(())
            });
        }
    }

    #[allow(dead_code)]
    fn insert_in_run_queue_head(&mut self, thread: ForeignBox<Thread>) {
        assert!(thread.state == State::Ready);
        // info!("pushing thread {:#x} on run queue head", thread.id());

        self.run_queue.push_front(thread);
    }

    #[allow(dead_code)]
    fn insert_in_run_queue_tail(&mut self, thread: ForeignBox<Thread>) {
        assert!(thread.state == State::Ready);
        // info!("pushing thread {:#x} on run queue tail", thread.id());

        self.run_queue.push_back(thread);
    }
}

#[allow(dead_code)]
fn reschedule(
    mut sched_state: SpinLockGuard<SchedulerState>,
    current_thread_id: usize,
) -> SpinLockGuard<SchedulerState> {
    // Caller to reschedule is responsible for removing current thread and
    // put it in the correct run/wait queue.

    assert!(sched_state.current_thread.is_none());

    // info!("reschedule");

    // Pop a new thread off the head of the run queue.
    // At the moment cannot handle an empty queue, so will panic in that case.
    // TODO: Implement either an idle thread or a special idle routine for that case.
    let Some(mut new_thread) = sched_state.run_queue.pop_head() else {
        panic!("run_queue empty");
    };

    assert!(new_thread.state == State::Ready);
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
pub fn tick(_time_ms: u32) {
    // info!("tick {} ms", _time_ms);

    // TODO: dynamically deal with time slice for this thread and put it
    // at the head or tail depending.
    TICK_WAIT_QUEUE.lock().wake_one();

    preempt();
}

// Exit the current thread.
// For now, simply remove ourselves from the run queue. No cleanup of thread resources
// is performed.
#[allow(dead_code)]
pub fn exit_thread() -> ! {
    let mut sched_state = SCHEDULER_STATE.lock();

    let Some(mut current_thread) = sched_state.current_thread.take() else {
        panic!("no current thread");
    };
    let current_thread_id = current_thread.id();

    info!("thread {:#x} exiting", current_thread.id() as usize);
    current_thread.state = State::Stopped;

    reschedule(sched_state, current_thread_id);

    // Should not get here
    #[allow(clippy::empty_loop)]
    loop {}
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

impl SchedLockGuard<'_, WaitQueue> {
    pub fn wake_one(mut self) {
        if let Some(mut thread) = self.queue.pop_head() {
            // Move the current thread to the head of its work queue as to not
            // steal it's time allocation.
            let current_thread_id = self.sched_mut().move_current_thread_to_front();

            thread.state = State::Ready;
            self.sched_mut().run_queue.push_back(thread);
            reschedule(self.into_sched(), current_thread_id);
        }
    }

    pub fn wait(mut self) {
        let Some(mut thread) = self.sched_mut().current_thread.take() else {
            panic!("no active thread");
        };
        let current_thread_id = thread.id();
        thread.state = State::Waiting;
        self.queue.push_back(thread);
        reschedule(self.into_sched(), current_thread_id);
    }
}

pub static TICK_WAIT_QUEUE: SchedLock<WaitQueue> = SchedLock::new(WaitQueue::new());
