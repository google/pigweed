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

use core::ptr;

use kernel::interrupt::InterruptController;
use kernel_config::{PlicConfig, PlicConfigInterface};
use log_if::debug_if;
use pw_log::info;

// TODO: Once the kernel supports more than one hart, these addresses will
// will need to be updated to handle multiple contexts.
// See Section 1.1 https://github.com/riscv/riscv-plic-spec/blob/1.0.0/riscv-plic-1.0.0.pdf
//
// Note that as per Section 3 of the spec, all registers are 32bit rather than usize.
const PLIC_SRC_PRIORITY_BASE: usize = PlicConfig::PLIC_BASE_ADDRESS + 0x4;
const PLIC_TARGET_ENABLE_BASE: usize = PlicConfig::PLIC_BASE_ADDRESS + 0x2000;
const PLIC_CLAIM: usize = PlicConfig::PLIC_BASE_ADDRESS + 0x200004;

const LOG_INTERRUPTS: bool = false;

pub struct Plic {}

impl Plic {
    pub const fn new() -> Self {
        Self {}
    }

    pub fn interrupt(&self) {
        let claim_reg = ptr::with_exposed_provenance_mut::<u32>(PLIC_CLAIM);
        let irq: u32;

        // Claim the interrupt
        unsafe {
            irq = claim_reg.read_volatile();
        }

        if irq == 0 {
            return;
        }

        debug_if!(LOG_INTERRUPTS, "interrupt {}", irq as u32);
        // SAFETY: All enabled IRQs are always less than PlicConfig::INTERRUPT_TABLE_SIZE
        if let Some(handler) = unsafe { PlicConfig::interrupt_table().get_unchecked(irq as usize) }
        {
            handler();
        } else {
            pw_assert::panic!("Unhandled interrupt {}", irq as u32);
        }

        // Release the claim on the interrupt
        unsafe {
            claim_reg.write_volatile(irq);
        }
    }
}

impl InterruptController for Plic {
    fn early_init(&self) {
        info!("Initializing PLIC");

        debug_if!(
            LOG_INTERRUPTS,
            "Base: {:#x} Src Priority: {:#x} Target Enable: {:#x} Claim: {:#x}",
            PlicConfig::PLIC_BASE_ADDRESS as usize,
            PLIC_SRC_PRIORITY_BASE as usize,
            PLIC_TARGET_ENABLE_BASE as usize,
            PLIC_CLAIM as usize
        );

        const GLOBAL_PRIORITY: u32 = 0;
        const IRQ_PRIORITY: u32 = 1;

        pw_assert::assert!(PlicConfig::INTERRUPT_TABLE_SIZE <= PlicConfig::NUM_IRQS as usize);
        // Start at 1, as interrupt source 0 does not exist
        for irq in 1..(PlicConfig::NUM_IRQS) as u32 {
            set_interrupt_priority(irq, IRQ_PRIORITY);

            if irq < PlicConfig::INTERRUPT_TABLE_SIZE as u32
                && PlicConfig::interrupt_table()[irq as usize].is_some()
            {
                debug_if!(
                    LOG_INTERRUPTS,
                    "Enabling interrupt {} with priority {}",
                    irq as u32,
                    IRQ_PRIORITY as u32
                );
                self.enable_interrupt(irq);
            } else {
                self.disable_interrupt(irq);
            }
        }

        debug_if!(
            LOG_INTERRUPTS,
            "Setting global priority to {}",
            GLOBAL_PRIORITY as u32
        );
        set_global_priority(GLOBAL_PRIORITY);

        unsafe {
            riscv::register::mie::set_mext();
        }
    }

    fn enable_interrupt(&self, irq: u32) {
        debug_if!(LOG_INTERRUPTS, "enable interrupt {}", irq as u32);
        set_interrupt_enable(irq, true);
    }

    fn disable_interrupt(&self, irq: u32) {
        debug_if!(LOG_INTERRUPTS, "disable interrupt {}", irq as u32);
        set_interrupt_enable(irq, false);
    }

    fn enable_interrupts() {
        debug_if!(LOG_INTERRUPTS, "enable_interrupts");
        unsafe {
            riscv::register::mstatus::set_mie();
        }
    }

    fn disable_interrupts() {
        debug_if!(LOG_INTERRUPTS, "disable_interrupts");
        unsafe {
            riscv::register::mstatus::clear_mie();
        }
    }

    fn interrupts_enabled() -> bool {
        let mie = riscv::register::mstatus::read().mie();
        debug_if!(LOG_INTERRUPTS, "interrupts_enabled: {}", mie as u8);
        mie
    }
}

fn set_interrupt_enable(irq: u32, enable: bool) {
    let enable_reg = ptr::with_exposed_provenance_mut::<u32>(
        PLIC_TARGET_ENABLE_BASE + (4 * (irq as usize / 32)),
    );
    let bitmask = 1 << (irq % 32);
    unsafe {
        let current_enables = enable_reg.read_volatile();
        let new_enables = if enable {
            current_enables | bitmask
        } else {
            current_enables & !bitmask
        };
        enable_reg.write_volatile(new_enables);
    };
}

fn set_global_priority(priority: u32) {
    let priority_reg = ptr::with_exposed_provenance_mut::<u32>(PlicConfig::PLIC_BASE_ADDRESS);
    unsafe { priority_reg.write_volatile(priority) };
}

fn set_interrupt_priority(irq: u32, priority: u32) {
    let priority_reg =
        ptr::with_exposed_provenance_mut::<u32>(PlicConfig::PLIC_BASE_ADDRESS + (irq as usize * 4));
    unsafe { priority_reg.write_volatile(priority) };
}
