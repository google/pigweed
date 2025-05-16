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

use kernel_config::{KernelConfig, RiscVKernelConfigInterface};
use riscv;

mod exceptions;
mod regs;
mod spinlock;
mod threads;
mod timer;

use crate::arch::{ArchInterface, MemoryRegion, MemoryRegionType};

pub struct Arch {}

impl ArchInterface for Arch {
    type ThreadState = threads::ArchThreadState;
    type BareSpinLock = spinlock::BareSpinLock;
    type Clock = timer::Clock;

    fn early_init() {
        // Make sure interrupts are disabled
        Self::disable_interrupts();

        // Hard code full access for U-Mode in the PMP.
        const MEM_CONFIG: MemoryConfig = MemoryConfig::const_new(&[MemoryRegion {
            ty: MemoryRegionType::ReadWriteExecutable,
            start: 0x0000_0000,
            end: 0xffff_ffff,
        }]);
        unsafe { MEM_CONFIG.0.write() };

        timer::early_init();
    }

    fn init() {
        timer::init();
    }

    fn enable_interrupts() {
        unsafe {
            riscv::register::mstatus::set_mie();
        }
    }

    fn disable_interrupts() {
        unsafe {
            riscv::register::mstatus::clear_mie();
        }
    }

    fn interrupts_enabled() -> bool {
        riscv::register::mstatus::read().mie()
    }

    fn idle() {
        riscv::asm::wfi();
    }

    fn panic() -> ! {
        unsafe {
            asm!("ebreak");
        }
        #[allow(clippy::empty_loop)]
        loop {}
    }
}

type PmpConfig =
    regs::pmp::PmpConfig<{ KernelConfig::PMP_CFG_REGISTERS }, { KernelConfig::PMP_ENTRIES }>;

/// RISC-V memory configuration
///
/// Represents the full configuration of RISC-V memory configuration through
/// the PMP block.
pub struct MemoryConfig(PmpConfig);

impl MemoryConfig {
    /// Create a new `MemoryConfig` in a `const` context
    ///
    /// # Panics
    /// Will panic if the current target's PMP will does not have enough entries
    /// to represent the provided `regions`.
    pub const fn const_new(regions: &[MemoryRegion]) -> Self {
        match PmpConfig::new(regions) {
            Ok(cfg) => Self(cfg),
            Err(_) => panic!("Can't create Memory config"),
        }
    }
}
