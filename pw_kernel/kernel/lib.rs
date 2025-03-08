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
mod target;

use scheduler::yield_timeslice;
use scheduler::Stack;
use scheduler::Thread;
use scheduler::SCHEDULER_STATE;

use arch::{Arch, ArchInterface};
use sync::mutex::Mutex;

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
    unsafe fn alloc_thread(&mut self) -> ForeignBox<Thread> {
        assert!(self.buffer.as_ptr().align_offset(align_of::<Thread>()) == 0);
        let thread_ptr = self.buffer.as_mut_ptr() as *mut Thread;
        thread_ptr.write(Thread::new());
        ForeignBox::new_from_ptr(&mut *thread_ptr)
    }
}

pub struct Kernel {}

impl Kernel {
    pub fn main() -> ! {
        target::console_init();
        info!("Welcome to Maize on {}!", target::name() as &str);

        Arch::early_init();

        let mut bootstrap_thread;
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

    let mut idle_thread;
    #[allow(static_mut_refs)]
    unsafe {
        info!("allocating idle_thread");
        static mut IDLE_THREAD_BUFFER: ThreadBuffer = ThreadBuffer::new();
        idle_thread = IDLE_THREAD_BUFFER.alloc_thread();

        info!("initializing idle_thread");
        static mut IDLE_STACK: [u8; 2048] = [0; 2048];
        idle_thread.initialize(Stack::from_slice(&IDLE_STACK), idle_thread_entry, 0usize);
    }
    SCHEDULER_STATE.lock().dump_all_threads();

    // TODO: Create a few test threads
    let mut thread_a;
    #[allow(static_mut_refs)]
    unsafe {
        info!("allocating thread A");
        static mut THREAD_BUFFER_A: ThreadBuffer = ThreadBuffer::new();
        thread_a = THREAD_BUFFER_A.alloc_thread();

        info!("initializing thread A");
        static mut STACK_A: [u8; 2048] = [0; 2048];
        thread_a.initialize(
            Stack::from_slice(&STACK_A),
            test_thread_entry_a,
            'a' as usize,
        );
    }
    SCHEDULER_STATE.lock().dump_all_threads();

    let mut thread_b;
    #[allow(static_mut_refs)]
    unsafe {
        info!("allocating thread B");
        static mut THREAD_BUFFER_B: ThreadBuffer = ThreadBuffer::new();
        thread_b = THREAD_BUFFER_B.alloc_thread();

        info!("initializing thread B");
        static mut STACK_B: [u8; 2048] = [0; 2048];
        thread_b.initialize(
            Stack::from_slice(&STACK_B),
            test_thread_entry_b,
            'b' as usize,
        );
    }

    Thread::start(idle_thread);
    Thread::start(thread_a);
    Thread::start(thread_b);

    SCHEDULER_STATE.lock().dump_all_threads();

    info!("End of kernel test");
    assert!(Arch::interrupts_enabled());

    info!("Exiting bootstrap thread");
}

#[allow(dead_code)]
fn idle_thread_entry(_arg: usize) {
    // Fake idle thread to keep the runqueue from being empty if all threads are blocked.
    assert!(Arch::interrupts_enabled());
    loop {
        Arch::idle();
    }
}

static TEST_COUNTER: Mutex<u64> = Mutex::new(0);

#[allow(dead_code)]
fn test_thread_entry_a(_arg: usize) {
    info!("I'm thread A");
    assert!(Arch::interrupts_enabled());
    loop {
        let mut counter = TEST_COUNTER.lock();
        info!("Thread A incrementing counter");
        *counter += 1;
        scheduler::TICK_WAIT_QUEUE.lock().wait();
    }
}

#[allow(dead_code)]
fn test_thread_entry_b(_arg: usize) {
    info!("I'm thread B");
    assert!(Arch::interrupts_enabled());
    loop {
        let counter = TEST_COUNTER.lock();
        info!("Thread B: counter value {}", *counter as u64);
        drop(counter);
        yield_timeslice();
    }
}
