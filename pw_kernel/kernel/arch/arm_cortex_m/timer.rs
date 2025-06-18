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

use kernel_config::{CortexMKernelConfigInterface, KernelConfig, KernelConfigInterface};
use pw_log::info;
use time::Clock as _;

use crate::arch::arm_cortex_m::regs::Regs;
use crate::scheduler;
use crate::sync::spinlock::SpinLock;

static TICKS: SpinLock<u64> = SpinLock::new(0);
const SYSTICK_RELOAD_VALUE: u32 = KernelConfig::SYS_TICK_HZ / KernelConfig::SCHEDULER_TICK_HZ;

pub struct Clock {}

impl time::Clock for Clock {
    const TICKS_PER_SEC: u64 = KernelConfig::SYS_TICK_HZ as u64;

    fn now() -> time::Instant<Self> {
        let mut ticks = TICKS.lock();
        let systick_regs = Regs::get().systick;
        let mut current = systick_regs.cvr.read().current();
        let reload = systick_regs.rvr.read().reload();
        if systick_regs.csr.read().countflag() {
            // Update the global tick count, as reading the control bit
            // when it's 1 will clear it.
            *ticks += u64::from(SYSTICK_RELOAD_VALUE);
            current = systick_regs.cvr.read().current();
        }

        let delta = u64::from(reload - current);
        // The cortex-m systick is a count down timer which triggers
        // ever reload period, which we define as (SYS_TICK_HZ / SCHEDULER_TICK_HZ).
        // The current time is calculated as the number of ticks + delta where:
        // - ticks is the systick count * reload period
        // - delta is the amount of time that has passed within the current systick
        let res = *ticks + delta;
        time::Instant::from_ticks(res)
    }
}

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

    // set a timer
    let mut rvr = Regs::get().systick.rvr;
    let rvr_val = rvr.read().with_reload(SYSTICK_RELOAD_VALUE - 1);
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
        pw_assert::eq!(
            (ticks_per_10ms * 100) as u64,
            KernelConfig::SYS_TICK_HZ as u64
        );
    }
}

#[no_mangle]
#[allow(non_snake_case)]
pub unsafe extern "C" fn SysTick() {
    let mut ticks = TICKS.lock();
    *ticks += u64::from(SYSTICK_RELOAD_VALUE);

    //info!("SysTick {}", *ticks as u64);

    scheduler::tick(super::Arch, Clock::now());
}
