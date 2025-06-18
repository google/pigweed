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

use riscv;

mod exceptions;
pub mod protection;
mod regs;
mod spinlock;
mod threads;
mod timer;

use crate::arch::ArchInterface;

#[derive(Copy, Clone)]
pub struct Arch;

impl ArchInterface for Arch {
    type BareSpinLock = spinlock::BareSpinLock;
    type Clock = timer::Clock;

    fn early_init() {
        // Make sure interrupts are disabled
        Self::disable_interrupts();

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
