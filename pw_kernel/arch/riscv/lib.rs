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

#![no_std]

use core::arch::asm;

use kernel::KernelState;
use kernel::scheduler::SchedulerContext as _;

mod exceptions;
mod protection;
mod regs;
mod spinlock;
mod threads;
mod timer;

// Re-exports to conform to simplify public API.
pub use protection::MemoryConfig;
pub use spinlock::BareSpinLock;
pub use threads::ArchThreadState;

#[derive(Copy, Clone, Default)]
pub struct Arch;

kernel::impl_thread_arg_for_default_zst!(Arch);

impl kernel::KernelContext for Arch {
    fn early_init(self) {
        // Make sure interrupts are disabled
        Arch.disable_interrupts();

        exceptions::early_init();

        timer::early_init();
    }

    fn init(self) {
        timer::init();
    }

    fn panic() -> ! {
        unsafe {
            asm!("ebreak");
        }
        #[allow(clippy::empty_loop)]
        loop {}
    }
}

impl kernel::KernelStateContext for Arch {
    fn get_state(self) -> &'static KernelState<Arch> {
        static STATE: KernelState<Arch> = KernelState::new();
        &STATE
    }
}
