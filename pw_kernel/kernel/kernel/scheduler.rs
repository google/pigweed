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

use core::mem::offset_of;

use super::{Arch, ArchInterface};
use crate::arch::ThreadState;
use list::*;
use pw_log::info;
use spinlock::{SpinLock, SpinLockGuard};

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
#[allow(dead_code)]
#[derive(Copy, Clone, PartialEq)]
enum State {
    New,
    Initial,
    Ready,
    Running,
}

pub struct Thread {
    // List of the threads in the system
    pub global_link: Link,

    // Active state link (run queue, wait queue, etc)
    pub active_link: Link,

    state: State,
    stack: Stack,

    // Architecturally specific thread state, saved on context switch
    pub arch_thread_state: <Arch as ArchInterface>::ThreadState,
}

unsafe impl Sync for Thread {}
impl Thread {
    // Create an empty, uninitialzed thread
    pub fn new() -> Self {
        Thread {
            global_link: Link::new(),
            active_link: Link::new(),
            state: State::New,
            arch_thread_state: ThreadState::new(),
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
        self.arch_thread_state
            .initialize_frame(stack, entry_point, arg);
        self.state = State::Initial;

        // Add our list to the global thread list
        SCHEDULER_STATE.lock().add_thread_to_list(self);

        self
    }

    #[allow(dead_code)]
    pub fn start(&mut self) {
        info!("starting thread {:#x}", self.id());

        assert!(self.state == State::Initial);
        self.state = State::Ready;

        let mut ss = SCHEDULER_STATE.lock();

        // Insert the current thread in the run queue at the head.
        let current_thread = ss.get_current_thread_ref();
        current_thread.state = State::Ready;
        ss.insert_in_run_queue_head(current_thread);

        // Add this thread to the scheduler and trigger a reschedule event
        ss.insert_in_run_queue_head(self);
        reschedule(ss);
    }

    // Dump to the console useful information about this thread
    #[allow(dead_code)]
    pub fn dump(&self) {
        info!("thread {:#x} state {}", self.id(), self.state as u32);
    }

    // A simple id for debugging purposes, currently the pointer to the thread structure itself
    #[allow(dead_code)]
    pub fn id(&self) -> usize {
        core::ptr::from_ref(self) as usize
    }

    // TODO: consider what to do with methods that implicitly only work on the current thread.
    // Perhaps part of Scheduler::Current namespace or move the methods to a CurrentThread
    // object that holds a ref to the current thread.
    #[allow(dead_code)]
    pub fn exit(&mut self) -> ! {
        info!("thread {:#x} exiting", self.id());
        #[allow(clippy::empty_loop)]
        loop {}
    }
}

pub fn bootstrap_scheduler(thread: &mut Thread) -> ! {
    let mut ss = SCHEDULER_STATE.lock();

    // TODO: assert that this is called exactly once at bootup to switch
    // to this particular thread.
    assert!(thread.state == State::Initial);
    thread.state = State::Running;

    ss.set_current_thread(thread);

    info!("context switching to first thread");

    // Special case where we're switching from a non-thread to something real
    let mut temp_thread = Thread::new();

    // Force a context switch to ourselves.
    <Arch as ArchInterface>::ThreadState::context_switch(ss, &mut temp_thread, thread);

    panic!("should not reach here");
}

#[allow(dead_code)]
pub struct ThreadListAdapter {}

impl list::Adapter for ThreadListAdapter {
    const LINK_OFFSET: usize = offset_of!(Thread, active_link);
}

#[allow(dead_code)]
pub struct GlobalThreadListAdapter {}

impl list::Adapter for GlobalThreadListAdapter {
    const LINK_OFFSET: usize = offset_of!(Thread, global_link);
}

// Global scheduler state (single processor for now)
#[allow(dead_code)]
pub struct SchedulerState {
    current_thread: *mut Thread,
    thread_list: UnsafeList<Thread, GlobalThreadListAdapter>,
    // For now just have a single round robin list, expand to multiple queues.
    run_queue: UnsafeList<Thread, ThreadListAdapter>,
}

pub static SCHEDULER_STATE: SpinLock<SchedulerState> = SpinLock::new(SchedulerState::new());

unsafe impl Sync for SchedulerState {}
unsafe impl Send for SchedulerState {}
impl SchedulerState {
    #[allow(dead_code)]
    const fn new() -> Self {
        Self {
            current_thread: core::ptr::null_mut(),
            thread_list: UnsafeList::new(),
            run_queue: UnsafeList::new(),
        }
    }

    #[allow(dead_code)]
    pub fn get_current_thread(&self) -> *mut Thread {
        self.current_thread
    }

    // Get a mutable reference to the current thread, which is effectively TLS, since it cannot
    // by definition go out of scope as long as we're using it. Uses the static lifetime to keep the
    // system from trying to drop the reference.
    // TODO: cleaner mechanism may be to wrap this with a CurrentThread struct that enforces
    // only using it on the current thread.
    #[allow(dead_code)]
    pub fn get_current_thread_ref(&self) -> &'static mut Thread {
        unsafe { &mut (*self.current_thread) }
    }

    #[allow(dead_code)]
    pub fn set_current_thread(&mut self, thread: *mut Thread) {
        self.current_thread = thread;
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
    fn insert_in_run_queue_head(&mut self, thread: &mut Thread) {
        assert!(thread.active_link.is_unlinked());
        assert!(thread.state == State::Ready);
        info!("pushing thread {:#x} on run queue head", thread.id());

        unsafe {
            self.run_queue.push_front_unchecked(thread);
        }
    }

    #[allow(dead_code)]
    fn insert_in_run_queue_tail(&mut self, thread: &mut Thread) {
        assert!(thread.active_link.is_unlinked());
        assert!(thread.state == State::Ready);
        info!("pushing thread {:#x} on run queue tail", thread.id());

        unsafe {
            self.run_queue.push_back_unchecked(thread);
        }
    }
}

#[allow(dead_code)]
fn reschedule(mut ss: SpinLockGuard<SchedulerState>) -> SpinLockGuard<SchedulerState> {
    let current_thread = ss.get_current_thread_ref();
    assert!(current_thread.state != State::Running);

    info!("reschedule");

    // Pop a new thread off the head of the run queue. Note the current thread
    // might not be in the queue.
    // At the moment cannot handle an empty queue, so will panic in that case.
    // TODO: Implement either an idle thread or a special idle routine for that case.
    let new_thread;
    unsafe {
        let rq = &mut ss.run_queue;
        match rq.pop_head() {
            Some(thread) => {
                new_thread = thread;
            }
            None => {
                info!("empty run queue!");
                panic!("");
            }
        }
    }

    assert!(new_thread.state == State::Ready);

    new_thread.state = State::Running;
    if current_thread.id() == new_thread.id() {
        info!("decided to continue running thread {:#x}", new_thread.id());
        return ss;
    }

    info!("switching to thread {:#x}", new_thread.id());

    ss.set_current_thread(new_thread);
    <Arch as ArchInterface>::ThreadState::context_switch(ss, current_thread, new_thread)
}

#[allow(dead_code)]
pub fn yield_timeslice() {
    let mut ss = SCHEDULER_STATE.lock();

    let current_thread = ss.get_current_thread_ref();
    info!("yielding thread {:#x}", current_thread.id());

    // Insert the current thread in the run queue at the tail.
    current_thread.state = State::Ready;
    ss.insert_in_run_queue_tail(current_thread);

    reschedule(ss);
}
