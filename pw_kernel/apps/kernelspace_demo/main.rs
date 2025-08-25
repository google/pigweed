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

use kernel::scheduler::thread::{self, StackStorage, StackStorageExt as _, Thread};
use kernel::sync::event::{Event, EventConfig, EventSignaler};
use kernel::sync::mutex::Mutex;
use kernel::{Duration, Kernel};
use kernel_config::{KernelConfig, KernelConfigInterface};
use pw_log::info;
use pw_status::Result;

pub struct DemoState<K: Kernel> {
    thread: Thread<K>,
    stack: StackStorage<{ KernelConfig::KERNEL_STACK_SIZE_BYTES }>,
    test_counter: Mutex<K, u64>,
    thread_a_done_event: Event<K>,
}

impl<K: Kernel> DemoState<K> {
    pub const fn new(kernel: K) -> DemoState<K> {
        DemoState {
            thread: Thread::new(""),
            stack: StackStorage::ZEROED,
            test_counter: Mutex::new(kernel, 0),
            thread_a_done_event: Event::new(kernel, EventConfig::ManualReset),
        }
    }
}

struct ThreadAArgs<'a, K: Kernel> {
    test_counter: &'a Mutex<K, u64>,
    done_signaler: EventSignaler<K>,
}

pub fn main<K: Kernel>(kernel: K, state: &'static mut DemoState<K>) -> Result<()> {
    let thread_b_args = ThreadAArgs {
        test_counter: &state.test_counter,
        done_signaler: state.thread_a_done_event.get_signaler(),
    };

    let thread_b = thread::init_thread_in(
        kernel,
        &mut state.thread,
        &mut state.stack,
        "B",
        test_thread_entry_b,
        &thread_b_args,
    );

    kernel::start_thread(kernel, thread_b);

    info!("Thread A re-using bootstrap thread");
    thread_a(kernel, &state.test_counter);

    let wait_result = state
        .thread_a_done_event
        .wait_until(kernel.now() + Duration::from_secs(1));
    match wait_result {
        Ok(()) => info!("Finished successfully"),
        Err(_) => info!("Thread B finished with error"),
    }
    wait_result
}

fn test_thread_entry_b<K: Kernel>(kernel: K, args: &ThreadAArgs<K>) {
    info!("Thread B starting");
    thread_b(kernel, args);
}

fn thread_a<K: Kernel>(kernel: K, test_counter: &Mutex<K, u64>) {
    for _ in 0..3 {
        let mut counter = test_counter.lock();
        kernel::sleep_until(kernel, kernel.now() + Duration::from_secs(1));
        info!("Thread A: incrementing counter");
        *counter = (*counter).saturating_add(1);
    }
    info!("Thread A: done");
}

fn thread_b<K: Kernel>(kernel: K, args: &ThreadAArgs<K>) {
    for _ in 0..4 {
        let deadline = kernel.now() + Duration::from_millis(600);
        let Ok(counter) = args.test_counter.lock_until(deadline) else {
            info!("Thread B: timeout");
            continue;
        };
        info!("Thread B: counter value {}", *counter as u64);
        pw_assert::assert!(*counter < 4);

        drop(counter);
        // Give Thread A a chance to acquire the mutex.
        kernel::yield_timeslice(kernel);
    }
    info!("Thread B: done");
    args.done_signaler.signal();
}
