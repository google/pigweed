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

/// This trait provides a generic interface to an architecture's interrupt
/// controller, such as a RISC-V PLIC or an Arm NVIC.
pub trait InterruptController {
    /// Called early in the kernel::main() function.
    fn early_init(&self) {}

    /// Enable a specific interrupt by its IRQ number.
    fn enable_interrupt(&self, irq: u32);

    /// Disable a specific interrupt by its IRQ number.
    fn disable_interrupt(&self, irq: u32);

    /// Globally enable interrupts.
    fn enable_interrupts();

    /// Globally disable interrupts.
    fn disable_interrupts();

    /// Returns `true` if interrupts are globally enabled.
    fn interrupts_enabled() -> bool;
}
