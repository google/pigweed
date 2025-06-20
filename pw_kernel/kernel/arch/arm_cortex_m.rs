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
use pw_cast::CastInto as _;
use pw_log::info;

use crate::{KernelState, KernelStateContext};

mod exceptions;
pub mod protection;
mod regs;
pub mod spinlock;
mod syscall;
pub mod threads;
mod timer;

#[derive(Copy, Clone, Default)]
pub struct Arch;

// Demonstration of zero over head register abstraction.
#[inline(never)]
fn get_num_mpu_regions(mpu: &mut regs::Mpu) -> u8 {
    mpu._type.read().dregion()
}

impl crate::KernelContext for Arch {
    fn early_init(self) {
        info!("arch early init");
        // TODO: set up the cpu here:
        //  --interrupt vector table--
        //  irq priority levels
        //  clear pending interrupts
        //  FPU initial state
        //  enable cache (if present)
        //  enable cycle counter?
        let p: Peripherals;
        // TODO: davidroth - use expect wrapper when available.
        if let Some(val) = Peripherals::take() {
            p = val;
        } else {
            pw_assert::panic!("Could not take peripherals.")
        }
        let mut r = regs::Regs::get();
        let cpuid = p.CPUID.base.read();
        info!("CPUID 0x{:x}", cpuid as u32);
        info!("Num MPU Regions: {}", get_num_mpu_regions(&mut r.mpu) as u8);

        unsafe {
            // Set the VTOR (assumes it exists)
            extern "C" {
                fn pw_boot_vector_table_addr();
            }
            let vector_table = pw_boot_vector_table_addr as *const ();
            p.SCB
                .vtor
                .write(vector_table.expose_provenance().cast_into());

            // Only the high two bits or the priority are guaranteed to be
            // implemented.  Values below are chosen accordingly.
            //
            // Note: Higher values have lower priority
            let mut scb = p.SCB;

            // Set SVCall (system calls) to the lowest priority.
            scb.set_priority(scb::SystemHandler::SVCall, 0b1111_1111);

            // Set PendSV (used by context switching) to just above SVCall so
            // that system calls can context switch.
            scb.set_priority(scb::SystemHandler::PendSV, 0b1011_1111);

            // Set IRQs to a priority above SVCall and PendSV so that they
            // can preempt them.
            scb.set_priority(scb::SystemHandler::SysTick, 0b0111_1111);

            // TODO: set all of the NVIC external irqs to medium as well

            scb.enable(scb::Exception::MemoryManagement);
            // TODO: configure BASEPRI, FAULTMASK
        } // unsafe

        // Set up PMP attr registers so that all PMP configs can reference them.
        #[cfg(feature = "user_space")]
        protection::init();

        timer::systick_early_init();

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

    fn init(self) {
        info!("arch init");
        timer::systick_init();
    }

    fn panic() -> ! {
        unsafe {
            asm!("bkpt");
        }
        loop {}
    }
}

impl KernelStateContext for Arch {
    fn get_state(self) -> &'static KernelState<Arch> {
        static STATE: KernelState<Arch> = KernelState::new();
        &STATE
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
    // TODO: konkers - Create register wrapper for IPSR.
    let current_exception = ipsr_register_read() & 0x1ff;

    // Treat SVCall (0xb) as in thread mode as we drop the SVCall pending bit
    // during system call execution to allow it to block and be preempted.
    // The code that manages this is in
    // [`crate::arch::arm_cortex_m::syscall::handle_syscall()`]
    current_exception != 0 && current_exception != 0xb
}

#[allow(dead_code)]
fn dump_int_pri() {
    info!(
        "basepri {} primask {}",
        cortex_m::register::basepri::read() as u8,
        cortex_m::register::primask::read().is_active() as u8
    );
}
