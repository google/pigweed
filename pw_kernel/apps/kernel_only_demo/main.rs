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
use kernel::sync::mutex::Mutex;
use kernel::{Duration, Kernel};
use kernel_config::{KernelConfig, KernelConfigInterface};
use pw_log::info;

pub struct DemoState<K: Kernel> {
    thread: Thread<K>,
    stack: StackStorage<{ KernelConfig::KERNEL_STACK_SIZE_BYTES }>,
    test_counter: Mutex<K, u64>,
}

impl<K: Kernel> DemoState<K> {
    pub const fn new(kernel: K) -> DemoState<K> {
        DemoState {
            thread: Thread::new(""),
            stack: StackStorage::ZEROED,
            test_counter: Mutex::new(kernel, 0),
        }
    }
}

pub fn main<K: Kernel>(kernel: K, state: &'static mut DemoState<K>) -> ! {
    let thread_b = thread::init_thread_in(
        kernel,
        &mut state.thread,
        &mut state.stack,
        "B",
        test_thread_entry_b,
        &state.test_counter,
    );

    kernel::start_thread(kernel, thread_b);

    info!("Thread A re-using bootstrap thread");
    thread_a(kernel, &state.test_counter);
}

fn test_thread_entry_b<K: Kernel>(kernel: K, test_counter: &Mutex<K, u64>) {
    info!("Thread B starting");
    thread_b(kernel, test_counter);
}

fn thread_a<K: Kernel>(kernel: K, test_counter: &Mutex<K, u64>) -> ! {
    loop {
        let mut counter = test_counter.lock();
        kernel::sleep_until(kernel, kernel.now() + Duration::from_secs(1));
        info!("Thread A: incrementing counter");
        *counter = (*counter).saturating_add(1);
    }
}

fn thread_b<K: Kernel>(kernel: K, test_counter: &Mutex<K, u64>) {
    loop {
        let deadline = kernel.now() + Duration::from_millis(600);
        let Ok(counter) = test_counter.lock_until(deadline) else {
            info!("Thread B: timeout");
            continue;
        };
        info!("Thread B: counter value {}", *counter as u64);

        pw_assert::ne!(*counter as u64, 4 as u64);
        drop(counter);
        // Give Thread A a chance to acquire the mutex.
        kernel::yield_timeslice(kernel);
    }
}
