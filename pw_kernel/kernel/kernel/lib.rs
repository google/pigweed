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

use pw_log::info;
use target::{Target, TargetInterface};

mod arch;
mod scheduler;

use scheduler::Stack;
use scheduler::Thread;
use scheduler::SCHEDULER_STATE;

use arch::{Arch, ArchInterface};

// A structure intended to be statically allocated to hold a Thread structure that will
// be constructed at run time.
#[repr(C, align(4))]
struct ThreadBuffer {
    buffer: [u8; size_of::<Thread>()],
}

impl ThreadBuffer {
    const fn new() -> Self {
        ThreadBuffer {
            buffer: [0; size_of::<Thread>()],
        }
    }

    // Create and new a thread out of the internal u8 buffer.
    // TODO: figure out how to properly statically construct a thread or
    // make sure this function can only be called once.
    #[inline(never)]
    fn alloc_thread(&mut self) -> &mut Thread {
        assert!(self.buffer.as_ptr().align_offset(align_of::<Thread>()) == 0);
        unsafe {
            let thread_ptr = self.buffer.as_mut_ptr() as *mut Thread;
            thread_ptr.write(Thread::new());
            &mut *thread_ptr
        }
    }
}

pub struct Kernel {}

impl Kernel {
    pub fn main() -> ! {
        Target::console_init();
        info!("Welcome to Maize on {}!", Target::NAME);
        Arch::early_init();

        let bootstrap_thread;
        #[allow(static_mut_refs)]
        unsafe {
            info!("allocating bootstrap thread");
            static mut THREAD_BUFFER_BOOTSTRAP: ThreadBuffer = ThreadBuffer::new();
            bootstrap_thread = THREAD_BUFFER_BOOTSTRAP.alloc_thread();

            info!("initializing bootstrap thread");
            static mut STACK_BOOTSTRAP: [u8; 2048] = [0; 2048];
            bootstrap_thread.initialize(
                Stack::from_slice(&STACK_BOOTSTRAP),
                bootstrap_thread_entry,
                0,
            );
        }

        info!("created thread, bootstrapping");

        // special case where we bootstrap the system by half context switching to this thread
        scheduler::bootstrap_scheduler(bootstrap_thread);

        // never get to here
    }
}

// completion of main in thread context
fn bootstrap_thread_entry(_arg: usize) {
    info!("Welcome to the first thread, continuing bootstrap");
    assert!(Arch::interrupts_enabled());

    Arch::init();

    SCHEDULER_STATE.lock().dump_all_threads();

    // TODO: Create a few test threads
    let thread_a;
    #[allow(static_mut_refs)]
    unsafe {
        info!("allocating thread A");
        static mut THREAD_BUFFER_A: ThreadBuffer = ThreadBuffer::new();
        thread_a = THREAD_BUFFER_A.alloc_thread();

        info!("initializing thread A");
        static mut STACK_A: [u8; 2048] = [0; 2048];
        thread_a.initialize(Stack::from_slice(&STACK_A), test_thread_entry, 'a' as usize);
    }
    SCHEDULER_STATE.lock().dump_all_threads();

    let thread_b;
    #[allow(static_mut_refs)]
    unsafe {
        info!("allocating thread B");
        static mut THREAD_BUFFER_B: ThreadBuffer = ThreadBuffer::new();
        thread_b = THREAD_BUFFER_B.alloc_thread();

        info!("initializing thread B");
        static mut STACK_B: [u8; 2048] = [0; 2048];
        thread_b.initialize(Stack::from_slice(&STACK_B), test_thread_entry, 'b' as usize);
    }

    thread_a.start();
    thread_b.start();

    SCHEDULER_STATE.lock().dump_all_threads();

    info!("End of kernel test");
    assert!(Arch::interrupts_enabled());

    info!("Exiting bootstrap thread");
}

#[allow(dead_code)]
fn test_thread_entry(arg: usize) {
    info!("i'm a thread! arg {}", arg);
    assert!(Arch::interrupts_enabled());
    #[allow(clippy::empty_loop)]
    loop {
        info!("thread {}", arg as u8 as char);
        Arch::idle();
    }
}
