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

use kernel::scheduler;
use kernel_config::{KernelConfig, RiscVKernelConfigInterface};
use time::Clock as _;

#[cfg(feature = "timer_mtime")]
mod mtime;
#[cfg(feature = "timer_mtime")]
use mtime::Timer;

#[cfg(feature = "timer_clint")]
mod clint;
#[cfg(feature = "timer_clint")]
use clint::Timer;

trait TimerInterface {
    fn early_init();
    fn init();
    fn enable();
    fn disable();
    fn get_current_monotonic_tick() -> u64;
    fn set_next_monotonic_tick();
}

pub struct Clock;

impl time::Clock for Clock {
    const TICKS_PER_SEC: u64 = KernelConfig::MTIME_HZ;

    fn now() -> time::Instant<Self> {
        time::Instant::from_ticks(Timer::get_current_monotonic_tick())
    }
}

pub fn early_init() {
    Timer::early_init();
}

pub fn init() {
    Timer::init();
}

pub fn mtimer_tick() {
    Timer::set_next_monotonic_tick();

    scheduler::tick(crate::Arch, Clock::now());
}
