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

use core::cell::UnsafeCell;

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

pub use arch::{Arch, ArchInterface, MemoryRegion, MemoryRegionType};
use kernel_config::{KernelConfig, KernelConfigInterface};
pub use scheduler::thread::{Process, Stack, Thread};
// Used by the `init_thread!` macro.
#[doc(hidden)]
pub use scheduler::thread::{StackStorage, StackStorageExt};
use scheduler::SCHEDULER_STATE;
pub use scheduler::{sleep_until, start_thread, yield_timeslice};
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
    #[must_use]
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
            0 as usize,
        );
        let thread_ptr = self.buffer.as_mut_ptr().cast::<Thread>();
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

pub struct StaticProcess {
    process_cell: UnsafeCell<Process>,
}

#[allow(dead_code)]
impl StaticProcess {
    #[must_use]
    pub const fn new(
        name: &'static str,
        memory_config: <Arch as ArchInterface>::MemoryConfig,
    ) -> Self {
        Self {
            process_cell: UnsafeCell::new(Process::new(name, memory_config)),
        }
    }

    #[must_use]
    pub fn get(&self) -> *mut Process {
        self.process_cell.get()
    }
}

unsafe impl Sync for StaticProcess {}
unsafe impl Send for StaticProcess {}

// Module re-exporting modules into a scope that can be referenced by macros
// in this crate.
#[doc(hidden)]
pub mod macro_exports {
    pub use pw_assert;
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
