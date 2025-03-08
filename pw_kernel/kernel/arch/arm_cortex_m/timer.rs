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
use crate::scheduler;
use core::sync::atomic;
use cortex_m::peripheral::*;
use pw_log::info;

const TICKS_PER_SEC: u32 = 10000000; // 10Mhz
const TICK_HZ: u32 = 100;

// Store the total number of 100hz ticks we've accumulated to service
// current_time below.
static TICKS: atomic::AtomicU32 = atomic::AtomicU32::new(0);

#[allow(dead_code)]
pub fn systick_dump() {
    info!(
        "counter {} reload {}",
        SYST::get_current() as u32,
        SYST::get_reload() as u32
    );
}

pub fn systick_early_init(syst: &mut SYST) {
    info!("starting monotonic systick\n");

    syst.disable_counter();
    syst.disable_interrupt();
    syst.clear_current();

    // set a 100Hz timer
    syst.set_reload(TICKS_PER_SEC / TICK_HZ);
    syst.enable_counter();

    syst.enable_interrupt();
}

pub fn systick_init() {}

#[no_mangle]
#[allow(non_snake_case)]
pub unsafe extern "C" fn SysTick() {
    TICKS.fetch_add(1, atomic::Ordering::SeqCst);

    //info!("SysTick {}", current_time_ms());

    scheduler::tick(ticks_to_ms());
}

fn ticks_to_ms() -> u32 {
    // TODO: handle unsafe wrapping multiplies
    TICKS.load(atomic::Ordering::Relaxed) * 1000000 / TICK_HZ
}

fn ticks_elapsed() -> u32 {
    // TODO: handle unsafe wrapping multiplies
    (TICKS_PER_SEC / TICK_HZ) - SYST::get_current()
}

#[allow(dead_code)]
pub fn current_time_ms() -> u32 {
    let time = ticks_to_ms() + (ticks_elapsed() * 1000000 / TICKS_PER_SEC);

    time
}
