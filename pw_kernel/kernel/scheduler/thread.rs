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

use list::*;
use pw_log::info;

use crate::arch::ThreadState;

use super::SCHEDULER_STATE;

#[derive(Clone, Copy)]
pub struct Stack {
    // Starting (lowest) address of the stack.  Inclusive.
    start: *const u8,

    // Ending (highest) address of the stack.  Exclusive.
    end: *const u8,
}

#[allow(dead_code)]
impl Stack {
    pub const fn from_slice(slice: &[u8]) -> Self {
        let start: *const u8 = slice.as_ptr();
        // Safety: offset based on known size of slice.
        let end = unsafe { start.add(slice.len()) };
        Self { start, end }
    }

    const fn new() -> Self {
        Self {
            start: core::ptr::null(),
            end: core::ptr::null(),
        }
    }

    pub fn start(&self) -> *const u8 {
        self.start
    }
    pub fn end(&self) -> *const u8 {
        self.end
    }

    pub fn initial_sp(&self, alignment: usize) -> *const u8 {
        // Use a zero sized allocation to align the initial stack pointer.
        Self::aligned_stack_allocation(self.end, 0, alignment)
    }

    pub fn contains(&self, ptr: *const u8) -> bool {
        ptr >= self.start && ptr < self.end
    }

    pub fn aligned_stack_allocation(sp: *const u8, size: usize, alignment: usize) -> *const u8 {
        let sp = sp.wrapping_byte_sub(size);
        let offset = sp.align_offset(alignment);
        if offset > 0 {
            sp.wrapping_byte_sub(alignment - offset)
        } else {
            sp
        }
    }
}

// TODO: want to name this ThreadState, but collides with ArchThreadstate
#[derive(Copy, Clone, PartialEq)]
pub(super) enum State {
    New,
    Initial,
    Ready,
    Running,
    Stopped,
    Waiting,
}

// TODO: use From or Into trait (unclear how to do it with 'static str)
pub(super) fn to_string(s: State) -> &'static str {
    match s {
        State::New => "New",
        State::Initial => "Initial",
        State::Ready => "Ready",
        State::Running => "Running",
        State::Stopped => "Stopped",
        State::Waiting => "Waiting",
    }
}

pub struct Process {
    // List of the processes in the system
    pub link: Link,

    // TODO - konkers: allow this to be tokenized.
    pub name: &'static str,

    thread_list: UnsafeList<Thread, ProcessThreadListAdapter>,
}
list::define_adapter!(pub ProcessListAdapter => Process.link);

impl Process {
    /// Creates a new, empty, unregistered process.
    pub const fn new(name: &'static str) -> Self {
        Self {
            link: Link::new(),
            name,
            thread_list: UnsafeList::new(),
        }
    }

    /// Registers process with scheduler.
    pub fn register(&mut self) {
        unsafe {
            SCHEDULER_STATE.lock().add_process_to_list(self);
        }
    }

    pub fn add_to_thread_list(&mut self, thread: &mut Thread) {
        unsafe {
            self.thread_list.push_front_unchecked(thread);
        }
    }

    // A simple id for debugging purposes, currently the pointer to the thread structure itself
    pub fn id(&self) -> usize {
        core::ptr::from_ref(self) as usize
    }

    pub fn dump(&self) {
        info!("process {} ({:#x})", self.name as &str, self.id() as usize);
        unsafe {
            let _ = self
                .thread_list
                .for_each(|thread| -> core::result::Result<(), ()> {
                    thread.dump();
                    Ok(())
                });
        }
    }
}

pub struct Thread {
    // List of threads in a given process.
    pub process_link: Link,

    // Active state link (run queue, wait queue, etc)
    pub active_link: Link,

    // Safety: All accesses to the parent process must be done with the
    // scheduler lock held.
    process: *mut Process,

    pub(super) state: State,
    pub(super) preempt_disable_count: u32,
    stack: Stack,

    // Architecturally specific thread state, saved on context switch
    pub arch_thread_state: UnsafeCell<crate::arch::ArchThreadState>,

    // TODO - konkers: allow this to be tokenized.
    pub name: &'static str,
}

list::define_adapter!(pub ThreadListAdapter => Thread.active_link);
list::define_adapter!(pub ProcessThreadListAdapter => Thread.process_link);

impl Thread {
    // Create an empty, uninitialzed thread
    pub fn new(name: &'static str) -> Self {
        Thread {
            process_link: Link::new(),
            active_link: Link::new(),
            process: core::ptr::null_mut(),
            state: State::New,
            preempt_disable_count: 0,
            arch_thread_state: UnsafeCell::new(ThreadState::new()),
            stack: Stack::new(),
            name,
        }
    }

    extern "C" fn trampoline(entry_point: usize, arg: usize) {
        let entry_point = core::ptr::with_exposed_provenance::<()>(entry_point);
        // SAFETY: This function is only ever passed to the
        // architecture-specific call to `initialize_frame` below. It is
        // never called directly. In `initialize_frame`, the first argument
        // is `entry_point as usize`. `entry_point` is a `fn(usize)`. Thus,
        // this transmute preserves validity, and the preceding
        // `with_exposed_provenance` ensures that the resulting `fn(usize)`
        // has valid provenance for its referent.
        let entry_point: fn(usize) = unsafe { core::mem::transmute(entry_point) };
        entry_point(arg);
    }

    pub fn initialize_kernel_thread(
        &mut self,
        kernel_stack: Stack,
        entry_point: fn(usize),
        arg: usize,
    ) -> &mut Thread {
        pw_assert::assert!(self.state == State::New);
        let args = (entry_point as usize, arg);
        unsafe {
            (*self.arch_thread_state.get()).initialize_kernel_frame(
                kernel_stack,
                Self::trampoline,
                args,
            );
        }

        let process = SCHEDULER_STATE.lock().kernel_process.get();
        unsafe { self.initialize(process, kernel_stack) }
    }

    #[cfg(feature = "user_space")]
    pub fn initialize_non_priv_thread(
        &mut self,
        kernel_stack: Stack,
        main_stack: Stack,
        entry_point: fn(usize),
        arg: usize,
    ) -> &mut Thread {
        pw_assert::assert!(self.state == State::New);

        let args = (entry_point as usize, arg);
        unsafe {
            (*self.arch_thread_state.get()).initialize_user_frame(
                kernel_stack,
                // Conservatively align stack to 16 bytes which is needed for
                // the RISC-V calling convention.   Ideally this would be
                // architecture dependant.  However, this value will eventually
                // be passed in from user space.
                main_stack.initial_sp(16) as *mut u8,
                Self::trampoline,
                args,
            );
        }
        // TODO: konkers - pass process in as an argument once we support user processes.
        let process = SCHEDULER_STATE.lock().kernel_process.get();
        unsafe { self.initialize(process, kernel_stack) }
    }

    /// # Preconditions
    /// `self.arch_thread_state` is initialized before calling this function.
    ///
    /// # Safety
    /// It is up to the caller to ensure that *process is valid.
    /// Initialize the mutable parts of the thread, must be called once per
    /// thread prior to starting it
    unsafe fn initialize(&mut self, process: *mut Process, kernel_stack: Stack) -> &mut Thread {
        self.stack = kernel_stack;
        self.process = process;

        self.state = State::Initial;

        let _sched_state = SCHEDULER_STATE.lock();
        unsafe {
            // Safety: *process is only accessed with the scheduler lock held.

            // Assert that the parent process is added to the scheduler.
            pw_assert::assert!(
                (*process).link.is_linked(),
                "Tried to add a Thread to an unregistered Process"
            );
            // Add thread to processes thread list.
            (*process).add_to_thread_list(self);
        }

        self
    }

    // Dump to the console useful information about this thread
    #[allow(dead_code)]
    pub fn dump(&self) {
        info!(
            "- thread {} ({:#x}) state {}",
            self.name as &str,
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
