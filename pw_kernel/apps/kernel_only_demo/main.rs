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
use kernel::{Duration, KernelStateContext};
use kernel_config::{KernelConfig, KernelConfigInterface};
use pw_log::info;

pub struct DemoState<C: KernelStateContext> {
    thread: Thread<C::ThreadState>,
    stack: StackStorage<{ KernelConfig::KERNEL_STACK_SIZE_BYTES }>,
    test_counter: Mutex<C, u64>,
}

impl<C: KernelStateContext> DemoState<C> {
    pub const fn new(ctx: C) -> DemoState<C> {
        DemoState {
            thread: Thread::new(""),
            stack: StackStorage::ZEROED,
            test_counter: Mutex::new(ctx, 0),
        }
    }
}

pub fn main<C: KernelStateContext>(ctx: C, state: &'static mut DemoState<C>) -> ! {
    let thread_b = thread::init_thread_in(
        ctx,
        &mut state.thread,
        &mut state.stack,
        "B",
        test_thread_entry_b,
        &state.test_counter,
    );

    kernel::start_thread(ctx, thread_b);

    info!("Thread A re-using bootstrap thread");
    thread_a(ctx, &state.test_counter);
}

fn test_thread_entry_b<C: KernelStateContext>(ctx: C, test_counter: &Mutex<C, u64>) {
    info!("Thread B starting");
    thread_b(ctx, test_counter);
}

fn thread_a<C: KernelStateContext>(ctx: C, test_counter: &Mutex<C, u64>) -> ! {
    loop {
        let mut counter = test_counter.lock();
        kernel::sleep_until(ctx, ctx.now() + Duration::from_secs(1));
        info!("Thread A: incrementing counter");
        *counter = (*counter).saturating_add(1);
    }
}

fn thread_b<C: KernelStateContext>(ctx: C, test_counter: &Mutex<C, u64>) {
    loop {
        let deadline = ctx.now() + Duration::from_millis(600);
        let Ok(counter) = test_counter.lock_until(deadline) else {
            info!("Thread B: timeout");
            continue;
        };
        info!("Thread B: counter value {}", *counter as u64);

        pw_assert::ne!(*counter as u64, 4 as u64);
        drop(counter);
        // Give Thread A a chance to acquire the mutex.
        kernel::yield_timeslice(ctx);
    }
}
