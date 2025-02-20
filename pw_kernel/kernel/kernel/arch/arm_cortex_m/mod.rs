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
use core::arch::asm;
use cortex_m::peripheral::*;
use pw_log::info;

use super::ArchInterface;

mod exceptions;
mod threads;
mod timer;

pub struct Arch {}

impl ArchInterface for Arch {
    type ThreadState = threads::ArchThreadState;

    fn early_init() {
        info!("arch early init");
        // TODO: set up the cpu here:
        //  --interrupt vector table--
        //  irq priority levels
        //  clear pending interrupts
        //  FPU initial state
        //  enable cache (if present)
        //  enable cycle counter?
        let mut p = Peripherals::take().unwrap();
        let cpuid = p.CPUID.base.read();
        info!("CPUID 0x{:x}", cpuid);

        unsafe {
            // Set the VTOR (assumes it exists)
            extern "C" {
                fn pw_boot_vector_table_addr();
            }
            let vector_table = pw_boot_vector_table_addr as *const ();
            p.SCB.vtor.write(vector_table as u32);

            // Set the interrupt priority for SVCall and PendSV to the lowest level
            // so that all of the external IRQs will preempt them.
            let mut scb = p.SCB;
            scb.set_priority(scb::SystemHandler::SVCall, 255);
            scb.set_priority(scb::SystemHandler::PendSV, 255);

            // Set the systick priority to medium
            scb.set_priority(scb::SystemHandler::SysTick, 128);

            // TODO: set all of the NVIC external irqs to medium as well

            // TODO: configure BASEPRI, FAULTMASK
        } // unsafe

        timer::systick_early_init(&mut p.SYST);

        // TEST: Intentionally trigger a hard fault to make sure the VTOR is working.
        // use core::arch::asm;
        // unsafe {
        //     asm!("bkpt");
        // }

        // TEST: Intentionally trigger a pendsv
        // use cortex_m::interrupt;
        // SCB::set_pendsv();
        // unsafe {
        //     interrupt::enable();
        // }
    }

    fn init() {
        info!("arch init");
        timer::systick_init();
    }

    fn enable_interrupts() {
        unsafe {
            cortex_m::interrupt::enable();
        }
    }
    fn disable_interrupts() {
        cortex_m::interrupt::disable();
    }
    fn interrupts_enabled() -> bool {
        // It's a complicated concept in cortex-m:
        // If PRIMASK is inactive, then interrupts are 100% disabled otherwise
        // if the current interrupt priority level is not zero (BASEPRI register) interrupts
        // at that level are not allowed. For now we're treating nonzero as full disabled.
        let primask = cortex_m::register::primask::read();
        let basepri = cortex_m::register::basepri::read();
        primask.is_active() && (basepri == 0)
    }

    fn idle() {
        cortex_m::asm::wfi();
    }
}

fn ipsr_register_read() -> u32 {
    let ipsr: u32;
    // Note: cortex-m crate does not implement this register for some reason, so
    // read and mask manually
    unsafe {
        asm!("mrs {ipsr}, ipsr", ipsr = out(reg) ipsr);
    }
    ipsr
}

// Utility function to read whether or not the cpu considers itself in a handler
fn in_interrupt_handler() -> bool {
    // IPSR[8:0] is the current exception handler (or 0 if in thread mode)
    ipsr_register_read() & (0x1ff) != 0
}

#[allow(dead_code)]
fn dump_int_pri() {
    info!(
        "basepri {} primask {}",
        cortex_m::register::basepri::read(),
        cortex_m::register::primask::read().is_active()
    );
}
