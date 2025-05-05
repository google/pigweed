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
#![no_std]
#![feature(const_trait_impl)]
#![feature(naked_functions)]

use foreign_box::ForeignBox;
use pw_log::info;

mod arch;
#[cfg(not(feature = "std_panic_handler"))]
mod panic;
mod scheduler;
pub mod sync;
mod syscall;
mod target;
mod timer;

use arch::{Arch, ArchInterface};
use kernel_config::{KernelConfig, KernelConfigInterface};
use scheduler::SCHEDULER_STATE;
pub use scheduler::{
    sleep_until, start_thread,
    thread::{Stack, Thread},
    yield_timeslice,
};
pub use timer::{Clock, Duration};

#[no_mangle]
#[allow(non_snake_case)]
pub extern "C" fn pw_assert_HandleFailure() -> ! {
    Arch::panic();
}

// A structure intended to be statically allocated to hold a Thread structure that will
// be constructed at run time.
#[repr(C, align(4))]
pub struct ThreadBuffer {
    buffer: [u8; size_of::<Thread>()],
}

impl ThreadBuffer {
    pub const fn new() -> Self {
        ThreadBuffer {
            buffer: [0; size_of::<Thread>()],
        }
    }

    // Create and new a thread out of the internal u8 buffer.
    // TODO: figure out how to properly statically construct a thread or
    // make sure this function can only be called once.
    #[inline(never)]
    pub fn alloc_thread(&mut self, name: &'static str) -> ForeignBox<Thread> {
        pw_assert::eq!(
            self.buffer.as_ptr().align_offset(align_of::<Thread>()) as usize,
            0 as usize
        );
        let thread_ptr = self.buffer.as_mut_ptr() as *mut Thread;
        unsafe {
            thread_ptr.write(Thread::new(name));
            ForeignBox::new_from_ptr(&mut *thread_ptr)
        }
    }
}

impl Default for ThreadBuffer {
    fn default() -> Self {
        Self::new()
    }
}

pub struct Kernel {}

#[macro_export]
macro_rules! init_thread {
    ($name:literal, $entry:expr, $stack_size:expr) => {{
        info!("allocating thread: {}", $name as &'static str);
        use $crate::Stack;
        use $crate::ThreadBuffer;
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
                static mut STACK: [u8; $stack_size] = [0; $stack_size];
                #[allow(static_mut_refs)]
                unsafe {
                    Stack::from_slice(&STACK)
                }
            },
            $entry,
            0,
        );

        thread
    }};
}

#[macro_export]
macro_rules! init_non_priv_thread {
    ($name:literal, $entry:expr, $stack_size:expr) => {{
        info!(
            "allocating non-privileged thread: {}",
            $name as &'static str
        );
        use $crate::Stack;
        use $crate::ThreadBuffer;
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
        thread.initialize_non_priv_thread(
            {
                static mut STACK: [u8; $stack_size] = [0; $stack_size];
                #[allow(static_mut_refs)]
                unsafe {
                    Stack::from_slice(&STACK)
                }
            },
            {
                static mut STACK: [u8; $stack_size] = [0; $stack_size];
                #[allow(static_mut_refs)]
                unsafe {
                    Stack::from_slice(&STACK)
                }
            },
            $entry,
            0,
        );

        thread
    }};
}

impl Kernel {
    pub fn main() -> ! {
        target::console_init();
        info!("Welcome to Maize on {}!", target::name() as &str);

        Arch::early_init();

        // Prepare the scheduler for thread initialization.
        scheduler::initialize();

        let bootstrap_thread = init_thread!(
            "bootstrap",
            bootstrap_thread_entry,
            KernelConfig::KERNEL_STACK_SIZE_BYTES
        );
        info!("created thread, bootstrapping");

        // special case where we bootstrap the system by half context switching to this thread
        scheduler::bootstrap_scheduler(bootstrap_thread);

        // never get to here
    }
}

// completion of main in thread context
fn bootstrap_thread_entry(_arg: usize) {
    info!("Welcome to the first thread, continuing bootstrap");
    pw_assert::assert!(Arch::interrupts_enabled());

    Arch::init();

    SCHEDULER_STATE.lock().dump_all_threads();

    let idle_thread = init_thread!(
        "idle",
        idle_thread_entry,
        KernelConfig::KERNEL_STACK_SIZE_BYTES
    );

    SCHEDULER_STATE.lock().dump_all_threads();

    scheduler::start_thread(idle_thread);

    target::main()
}

fn idle_thread_entry(_arg: usize) {
    // Fake idle thread to keep the runqueue from being empty if all threads are blocked.
    pw_assert::assert!(Arch::interrupts_enabled());
    loop {
        Arch::idle();
    }
}
