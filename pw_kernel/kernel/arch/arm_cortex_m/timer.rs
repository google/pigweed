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
use crate::arch::arm_cortex_m::regs::Regs;
use crate::scheduler;
use core::sync::atomic;
use pw_log::info;
use time::Clock as _;

const TICKS_PER_SEC: u32 = 1000000; // 1Mhz
const TICK_HZ: u32 = 100;

pub struct Clock;

impl time::Clock for Clock {
    const TICKS_PER_SEC: u64 = TICK_HZ as u64;

    fn now() -> time::Instant<Self> {
        time::Instant::from_ticks(TICKS.load(atomic::Ordering::SeqCst) as u64)
    }
}

// Store the total number of 100hz ticks we've accumulated to service
// current_time below.
static TICKS: atomic::AtomicU32 = atomic::AtomicU32::new(0);

#[allow(dead_code)]
pub fn systick_dump() {
    let systick_regs = Regs::get().systick;
    let current = systick_regs.cvr.read().current();
    let reload = systick_regs.rvr.read().reload();

    info!("current {} reload {}", current as u32, reload as u32);
}

pub fn systick_early_init() {
    info!("starting monotonic systick\n");

    let mut csr = Regs::get().systick.csr;
    // disable counter and interrupts
    let mut csr_val = csr.read().with_enable(false).with_tickint(false);
    csr.write(csr_val);

    // clear current value
    let mut cvr = Regs::get().systick.cvr;
    let cvr_val = cvr.read().with_current(0);
    cvr.write(cvr_val);

    // set a 100Hz timer
    let mut rvr = Regs::get().systick.rvr;
    let rvr_val = rvr.read().with_reload(TICKS_PER_SEC / TICK_HZ);
    rvr.write(rvr_val);

    // enable counter and interrupts
    csr_val = csr.read().with_enable(true).with_tickint(true);
    csr.write(csr_val);
}

pub fn systick_init() {
    let systick_regs = Regs::get().systick;
    let ticks_per_10ms = systick_regs.calib.read().tenms();
    info!("ticks_per_10ms: {}", ticks_per_10ms as u32);
    if ticks_per_10ms > 0 {
        pw_assert::eq!((ticks_per_10ms * 100) as u64, TICKS_PER_SEC as u64);
    }
}

#[no_mangle]
#[allow(non_snake_case)]
pub unsafe extern "C" fn SysTick() {
    TICKS.fetch_add(1, atomic::Ordering::SeqCst);

    //info!("SysTick {}", current_time_ms());

    scheduler::tick(Clock::now());
}
