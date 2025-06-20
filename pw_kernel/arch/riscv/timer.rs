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

use core::ptr::{with_exposed_provenance, with_exposed_provenance_mut};

use kernel::scheduler;
use kernel_config::{KernelConfig, KernelConfigInterface};
use pw_log::info;
use time::{Clock as _, Duration};

use crate::spinlock::InterruptGuard;

// Use the CLINT to read time and set the hardware timer.
// NOTE: assumes machine mode and the CLINT is the best timer to use.
// TODO: obtain this address from the target layer.
const CLINT_BASE: usize = 0x200_0000;
const CLINT_MTIMECMP_REGISTER: usize = CLINT_BASE + 0x4000;
const CLINT_MTIME_REGISTER: usize = CLINT_BASE + 0xbff8;

fn read_mtime() -> u64 {
    // TODO: consider using riscv::register::time instead.
    // TODO: make sure this is 32bit safe by reading high and low parts separately.
    let reg = with_exposed_provenance::<u64>(CLINT_MTIME_REGISTER);
    unsafe { reg.read_volatile() }
}

fn write_mtimecmp(value: u64) {
    let reg = with_exposed_provenance_mut::<u64>(CLINT_MTIMECMP_REGISTER);
    // TODO: make sure this is 32bit safe by writing high and low parts separately.
    unsafe { reg.write_volatile(value) }
}

#[inline(never)]
fn set_next_monotonic_tick() {
    let _guard = InterruptGuard::new();

    let now = read_mtime();

    let ticks_per_monotonic: Duration<Clock> =
        time::Duration::from_millis((1000 / KernelConfig::SCHEDULER_TICK_HZ).into());
    // safe to cast to u64 as ticks_per_monotonic will never be negative here,
    let next = now.checked_add(ticks_per_monotonic.ticks().cast_unsigned());
    if let Some(val) = next {
        write_mtimecmp(val);
    } else {
        pw_assert::debug_panic!("next_monotonic_tick overflow");
    }
}

fn disable_timer() {
    unsafe {
        // Simply disable the interrupt.
        riscv::register::mie::clear_mtimer();
    }
}

fn enable_timer() {
    unsafe {
        riscv::register::mie::set_mtimer();
    }
}

pub fn early_init() {
    info!("starting monotonic timer");

    disable_timer();
    set_next_monotonic_tick();
    enable_timer();
}

pub fn init() {}

pub fn mtimer_tick() {
    set_next_monotonic_tick();

    scheduler::tick(super::Arch, Clock::now());
}

pub struct Clock;

impl time::Clock for Clock {
    const TICKS_PER_SEC: u64 = 10_000_000;

    fn now() -> time::Instant<Self> {
        time::Instant::from_ticks(read_mtime())
    }
}
