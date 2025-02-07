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

use cortex_m::peripheral::Peripherals;
use pw_log::info;

use super::ArchInterface;

mod exceptions;

pub struct ThreadState {}

impl const super::ThreadState for ThreadState {
    fn new() -> Self {
        Self {}
    }
}

pub struct Arch {}

impl ArchInterface for Arch {
    type ThreadState = ThreadState;

    fn early_init() {
        // TODO: set up the cpu here:
        //  interrupt vector table
        //  irq priority levels
        //  clear pending interrupts
        //  FPU initial state
        //  enable cache (if present)
        //  enable cycle counter?
        let p = Peripherals::take().unwrap();
        let cpuid = p.CPUID.base.read();
        info!("CPUID 0x{:x}", cpuid);

        // Set the VTOR (assumes it exists)
        unsafe {
            extern "C" {
                fn pw_boot_vector_table_addr();
            }
            let vector_table = pw_boot_vector_table_addr as *const ();
            p.SCB.vtor.write(vector_table as u32);
        }

        // Intentionally trigger a hard fault to make sure the VTOR is working.
        // use core::arch::asm;
        // unsafe {
        //     asm!("bkpt");
        // }
    }

    fn init() {
        info!("arch init");
    }
}
