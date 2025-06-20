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

use kernel::arch::Arch;
use kernel::scheduler::SchedulerContext as _;
use kernel::sync::mutex::Mutex;
use kernel::Duration;
use kernel_config::{KernelConfig, KernelConfigInterface};
use pw_log::info;

pub fn main() -> ! {
    // SAFETY: The `main` function thread is never executed more than once.
    let thread_b = unsafe {
        kernel::init_thread!(
            "B",
            test_thread_entry_b,
            KernelConfig::KERNEL_STACK_SIZE_BYTES
        )
    };
    kernel::start_thread(Arch, thread_b);

    info!("Thread A re-using bootstrap thread");
    thread_a()
}

fn test_thread_entry_b(_ctx: Arch, _arg: usize) {
    info!("Thread B starting");
    thread_b();
}

static TEST_COUNTER: Mutex<Arch, u64> = Mutex::new(Arch, 0);

fn thread_a() -> ! {
    loop {
        let mut counter = TEST_COUNTER.lock();
        kernel::sleep_until(Arch, Arch.now() + Duration::from_secs(1));
        info!("Thread A: incrementing counter");
        *counter = (*counter).saturating_add(1);
    }
}

fn thread_b() {
    loop {
        let deadline = Arch.now() + Duration::from_millis(600);
        let Ok(counter) = TEST_COUNTER.lock_until(deadline) else {
            info!("Thread B: timeout");
            continue;
        };
        info!("Thread B: counter value {}", *counter as u64);

        pw_assert::ne!(*counter as u64, 4 as u64);
        drop(counter);
        // Give Thread A a chance to acquire the mutex.
        kernel::yield_timeslice(Arch);
    }
}
