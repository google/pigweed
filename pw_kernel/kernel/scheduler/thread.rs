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
use core::mem::MaybeUninit;

use list::*;
use pw_log::info;
use pw_status::Result;

use super::SCHEDULER_STATE;
use crate::arch::{Arch, ArchInterface, ThreadState};

/// The memory backing a thread's stack before it has been started.
///
/// After a thread has been started, ownership of its stack's memory is (from
/// the Rust Abstract Machine (AM) perspective) relinquished, and so the type we
/// use to represent that memory is irrelevant.
///
/// However, while we are initializing a thread in preparation for starting it,
/// we are operating on that memory as a normal Rust variable, and so its type
/// is important.
///
/// Using `MaybeUninit<u8>` instead of `u8` is important for two reasons:
/// - It ensures that it is sound to write values which are not entirely
///   initialized (e.g., which contain padding bytes).
/// - It ensures that pointers written to the stack [retain
///   provenance][provenance].
///
/// [provenance]: https://github.com/rust-lang/unsafe-code-guidelines/issues/286#issuecomment-2837585644
pub type StackStorage<const N: usize> = [MaybeUninit<u8>; N];

pub trait StackStorageExt {
    const ZEROED: Self;
}

impl<const N: usize> StackStorageExt for StackStorage<N> {
    const ZEROED: StackStorage<N> = [MaybeUninit::new(0); N];
}

#[derive(Clone, Copy)]
pub struct Stack {
    // Starting (lowest) address of the stack.  Inclusive.
    start: *const MaybeUninit<u8>,

    // Ending (highest) address of the stack.  Exclusive.
    end: *const MaybeUninit<u8>,
}

#[allow(dead_code)]
impl Stack {
    #[must_use]
    pub const fn from_slice(slice: &[MaybeUninit<u8>]) -> Self {
        let start: *const MaybeUninit<u8> = slice.as_ptr();
        // Safety: offset based on known size of slice.
        let end = unsafe { start.add(slice.len()) };
        Self { start, end }
    }

    #[must_use]
    const fn new() -> Self {
        Self {
            start: core::ptr::null(),
            end: core::ptr::null(),
        }
    }

    #[must_use]
    pub fn start(&self) -> *const MaybeUninit<u8> {
        self.start
    }

    #[must_use]
    pub fn end(&self) -> *const MaybeUninit<u8> {
        self.end
    }

    /// # Safety
    /// Caller must ensure exclusive mutable access to underlying data
    #[must_use]
    pub unsafe fn end_mut(&self) -> *mut MaybeUninit<u8> {
        self.end as *mut MaybeUninit<u8>
    }

    #[must_use]
    pub fn contains(&self, ptr: *const MaybeUninit<u8>) -> bool {
        ptr >= self.start && ptr < self.end
    }

    #[must_use]
    pub fn aligned_stack_allocation_mut<T: Sized>(
        sp: *mut MaybeUninit<u8>,
        alignment: usize,
    ) -> *mut T {
        let sp = sp.wrapping_byte_sub(size_of::<T>());
        let offset = sp.align_offset(alignment);
        if offset > 0 {
            sp.wrapping_byte_sub(alignment - offset).cast()
        } else {
            sp.cast()
        }
    }

    pub fn aligned_stack_allocation<T: Sized>(
        sp: *mut MaybeUninit<u8>,
        alignment: usize,
    ) -> *const T {
        Self::aligned_stack_allocation_mut::<*mut T>(sp, alignment).cast()
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

    memory_config: <Arch as ArchInterface>::MemoryConfig,

    thread_list: UnsafeList<Thread, ProcessThreadListAdapter>,
}
list::define_adapter!(pub ProcessListAdapter => Process.link);

impl Process {
    /// Creates a new, empty, unregistered process.
    #[must_use]
    pub const fn new(
        name: &'static str,
        memory_config: <Arch as ArchInterface>::MemoryConfig,
    ) -> Self {
        Self {
            link: Link::new(),
            name,
            memory_config,
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

    /// A simple ID for debugging purposes, currently the pointer to the thread
    /// structure itself.
    ///
    /// # Safety
    ///
    /// The returned value should not be relied upon as being a valid pointer.
    /// Even in the current implementation, `id` does not expose the pointer's
    /// provenance.
    #[must_use]
    pub fn id(&self) -> usize {
        core::ptr::from_ref(self).addr()
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
    #[must_use]
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
        let process = SCHEDULER_STATE.lock().kernel_process.get();
        let args = (entry_point as usize, arg);
        unsafe {
            (*self.arch_thread_state.get()).initialize_kernel_frame(
                kernel_stack,
                &raw const (*process).memory_config,
                Self::trampoline,
                args,
            );
        }

        unsafe { self.initialize(process, kernel_stack) }
    }

    #[cfg(feature = "user_space")]
    /// # Safety
    /// It is up to the caller to ensure that *process is valid.
    /// Initialize the mutable parts of the non privileged thread, must be
    /// called once per thread prior to starting it
    pub unsafe fn initialize_non_priv_thread(
        &mut self,
        kernel_stack: Stack,
        initial_sp: usize,
        process: *mut Process,
        entry_point: usize,
        arg: usize,
    ) -> Result<&mut Thread> {
        pw_assert::assert!(self.state == State::New);

        unsafe {
            (*self.arch_thread_state.get()).initialize_user_frame(
                kernel_stack,
                &raw const (*process).memory_config,
                // be passed in from user space.
                initial_sp,
                entry_point,
                arg,
            )?;
        }
        unsafe { Ok(self.initialize(process, kernel_stack)) }
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

    /// A simple ID for debugging purposes, currently the pointer to the thread
    /// structure itself.
    ///
    /// # Safety
    ///
    /// The returned value should not be relied upon as being a valid pointer.
    /// Even in the current implementation, `id` does not expose the pointer's
    /// provenance.
    #[must_use]
    pub fn id(&self) -> usize {
        core::ptr::from_ref(self).addr()
    }

    // An ID that can not be assigned to any thread in the system.
    #[must_use]
    pub const fn null_id() -> usize {
        // `core::ptr::null::<Self>() as usize` can not be evaluated at const time
        // and a null pointer is defined to be at address 0 (see
        // https://doc.rust-lang.org/beta/core/ptr/fn.null.html).
        0usize
    }
}

// TODO: davidroth - Add const assertions to ensure stack sizes aren't too
// small, once the sizing analysis has been done to understand what a
// reasonable minimum is.
#[macro_export]
macro_rules! init_thread {
    ($name:literal, $entry:expr, $stack_size:expr) => {{
        info!("allocating thread: {}", $name as &'static str);
        use $crate::{Stack, ThreadBuffer};
        let mut thread = {
            static mut THREAD_BUFFER: ThreadBuffer = ThreadBuffer::new();
            #[allow(static_mut_refs)]
            unsafe {
                THREAD_BUFFER.alloc_thread($name)
            }
        };

        info!("initializing thread: {}", $name as &'static str);
        thread.initialize_kernel_thread(
            {
                static mut STACK_STORAGE: $crate::StackStorage<{ $stack_size }> =
                    $crate::StackStorageExt::ZEROED;
                #[allow(static_mut_refs)]
                unsafe {
                    Stack::from_slice(&STACK_STORAGE)
                }
            },
            $entry,
            0,
        );

        thread
    }};
}

#[cfg(feature = "user_space")]
#[macro_export]
macro_rules! init_non_priv_process {
    ($name:literal, $memory_config:expr) => {{
        use kernel::StaticProcess;
        use pw_log::info;
        info!(
            "allocating non-privileged process: {}",
            $name as &'static str
        );

        static PROCESS: StaticProcess = StaticProcess::new($name, $memory_config);
        unsafe { (*PROCESS.get()).register() };
        &PROCESS
    }};
}

#[cfg(feature = "user_space")]
#[macro_export]
macro_rules! init_non_priv_thread {
    ($name:literal, $process:expr, $entry:expr, $initial_sp:expr, $kernel_stack_size:expr) => {{
        use pw_log::info;
        info!(
            "allocating non-privileged thread: {}, entry {:#x}",
            $name as &'static str, $entry as usize
        );
        use $crate::{Stack, ThreadBuffer};
        let mut thread = {
            static mut THREAD_BUFFER: ThreadBuffer = ThreadBuffer::new();
            #[allow(static_mut_refs)]
            unsafe {
                THREAD_BUFFER.alloc_thread($name)
            }
        };

        info!(
            "initializing non-privileged thread: {}",
            $name as &'static str
        );
        unsafe {
            if let Err(e) = thread.initialize_non_priv_thread(
                {
                    static mut STACK_STORAGE: $crate::StackStorage<{ $kernel_stack_size }> =
                        $crate::StackStorageExt::ZEROED;
                    #[allow(static_mut_refs)]
                    unsafe {
                        Stack::from_slice(&STACK_STORAGE)
                    }
                },
                $initial_sp,
                $process.get(),
                $entry,
                0,
            ) {
                $crate::macro_exports::pw_assert::panic!(
                    "Error initializing thread: {}: {}",
                    $name as &'static str,
                    e as u32
                );
            }
        }

        thread
    }};
}
