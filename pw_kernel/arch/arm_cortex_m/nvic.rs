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

use kernel::interrupt::InterruptController;
use log_if::debug_if;
use pw_log::info;

const LOG_INTERRUPTS: bool = false;

pub struct Nvic {}

impl Nvic {
    pub const fn new() -> Self {
        Self {}
    }
}

impl InterruptController for Nvic {
    fn early_init(&self) {
        info!("Initializing NVIC");
    }

    fn enable_interrupt(&self, _irq: u32) {
        todo!("unimplemented")
    }

    fn disable_interrupt(&self, _irq: u32) {
        todo!("unimplemented")
    }

    fn enable_interrupts() {
        debug_if!(LOG_INTERRUPTS, "enable_interrupts");
        unsafe {
            cortex_m::interrupt::enable();
        }
    }

    fn disable_interrupts() {
        debug_if!(LOG_INTERRUPTS, "disable_interrupts");
        cortex_m::interrupt::disable();
    }

    fn interrupts_enabled() -> bool {
        // It's a complicated concept in cortex-m:
        // If PRIMASK is inactive, then interrupts are 100% disabled otherwise
        // if the current interrupt priority level is not zero (BASEPRI register) interrupts
        // at that level are not allowed. For now we're treating nonzero as full disabled.
        let primask = cortex_m::register::primask::read();
        let basepri = cortex_m::register::basepri::read();
        debug_if!(
            LOG_INTERRUPTS,
            "interrupts_enabled: primask {} basepri {}",
            primask as u32,
            basepri as u32
        );
        primask.is_active() && (basepri == 0)
    }
}
