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
use time::Clock as _;
// TODO - konkers: move into "real" user space
use syscall_user::*;

mod arch;
#[cfg(not(feature = "std_panic_handler"))]
mod panic;
mod scheduler;
pub mod sync;
mod syscall;
mod target;
mod timer;

use arch::{Arch, ArchInterface};
use scheduler::{Stack, Thread, SCHEDULER_STATE};
use sync::mutex::Mutex;
use timer::{Clock, Duration};

#[no_mangle]
#[allow(non_snake_case)]
pub extern "C" fn pw_assert_HandleFailure() -> ! {
    Arch::panic();
}

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
    unsafe fn alloc_thread(&mut self, name: &'static str) -> ForeignBox<Thread> {
        pw_assert::eq!(
            self.buffer.as_ptr().align_offset(align_of::<Thread>()) as usize,
            0 as usize
        );
        let thread_ptr = self.buffer.as_mut_ptr() as *mut Thread;
        thread_ptr.write(Thread::new(name));
        ForeignBox::new_from_ptr(&mut *thread_ptr)
    }
}

pub struct Kernel {}

macro_rules! init_thread {
    ($name:literal, $entry:expr) => {{
        info!("allocating thread: {}", $name as &'static str);
        let mut thread = {
            static mut THREAD_BUFFER: ThreadBuffer = ThreadBuffer::new();
            #[allow(static_mut_refs)]
            unsafe {
                THREAD_BUFFER.alloc_thread($name)
            }
        };

        info!("initializing thread: {}", $name as &'static str);
        thread.initialize(
            {
                static mut STACK: [u8; 2048] = [0; 2048];
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

        let bootstrap_thread = init_thread!("bootstrap", bootstrap_thread_entry);
        info!("created thread, bootstrapping");

        // special case where we bootstrap the system by half context switching to this thread
        scheduler::bootstrap_scheduler(bootstrap_thread);

        // never get to here
    }
}

// completion of main in thread context
extern "C" fn bootstrap_thread_entry(_arg: usize) {
    info!("Welcome to the first thread, continuing bootstrap");
    pw_assert::assert!(Arch::interrupts_enabled());

    Arch::init();

    SCHEDULER_STATE.lock().dump_all_threads();

    let idle_thread = init_thread!("idle", idle_thread_entry);

    SCHEDULER_STATE.lock().dump_all_threads();

    let thread_a = init_thread!("A", test_thread_entry_a);

    SCHEDULER_STATE.lock().dump_all_threads();

    let thread_b = init_thread!("B", test_thread_entry_b);

    SCHEDULER_STATE.lock().dump_all_threads();

    Thread::start(idle_thread);
    Thread::start(thread_a);
    Thread::start(thread_b);

    SCHEDULER_STATE.lock().dump_all_threads();

    info!("End of kernel test");
    pw_assert::assert!(Arch::interrupts_enabled());

    info!("Exiting bootstrap thread");
}

#[allow(dead_code)]
extern "C" fn idle_thread_entry(_arg: usize) {
    // Fake idle thread to keep the runqueue from being empty if all threads are blocked.
    pw_assert::assert!(Arch::interrupts_enabled());
    loop {
        Arch::idle();
    }
}

extern "C" fn test_thread_entry_a(_arg: usize) {
    info!("Thread A starting");
    pw_assert::assert!(Arch::interrupts_enabled());
    thread_a();
}

extern "C" fn test_thread_entry_b(_arg: usize) {
    info!("Thread B starting");
    pw_assert::assert!(Arch::interrupts_enabled());
    thread_b();
}

static TEST_COUNTER: Mutex<u64> = Mutex::new(0);

fn thread_a() {
    loop {
        let mut counter = TEST_COUNTER.lock();
        scheduler::sleep_until(Clock::now() + Duration::from_secs(1));
        info!("Thread A: incrementing counter");
        *counter += 1;
    }
}

fn thread_b() {
    loop {
        let deadline = Clock::now() + Duration::from_millis(600);
        let Ok(counter) = TEST_COUNTER.lock_until(deadline) else {
            info!("Thread B: timeout");
            continue;
        };
        info!("Thread B: counter value {}", *counter as u64);

        let val = SysCall::debug_add(0xdecaf000, 0xbad);
        match val {
            Ok(val) => info!("retval: {:08x}", val as u32),
            Err(e) => info!("error: {}", e as u32),
        }

        pw_assert::ne!(*counter as usize, 4 as usize);
        drop(counter);
        // Give Thread A a chance to acquire the mutex.
        scheduler::yield_timeslice();
    }
}
