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

/// Kernel configuration common to all architectures.
pub trait KernelConfigInterface {
    /// Rate of the scheduler monotonic tick.
    const SCHEDULER_TICK_HZ: u32 = 100;
    /// The number of bytes allocated for each kernel stack.
    const KERNEL_STACK_SIZE_BYTES: usize = 2048;
}

/// Cortex-M specific configuration.
// TODO: davidroth - Once Arch is out of tree, move this configuration also.
pub trait CortexMKernelConfigInterface {
    /// Rate of the Cortex-M systick system timer.
    const SYS_TICK_HZ: u32;

    /// Number of supported MPU regions
    const NUM_MPU_REGIONS: usize;
}

/// RISC-V specific configuration.
// TODO: davidrotOnce Arch is out of tree, move this configuration also.
/// mtvec exception mode.
pub enum ExceptionMode {
    Direct,
    Vectored(usize),
}

pub trait RiscVKernelConfigInterface {
    /// Number of PMP entries.  Per the architecture spec this may be 0, 16
    /// or 64.
    const PMP_ENTRIES: usize;

    /// Number of pmpcfgN registers.  For rv32 this is `PMP_ENTRIES / 4`, for
    /// rv64 it is `PMP_ENTRIES / 8`.
    const PMP_CFG_REGISTERS: usize;

    /// mtvec exception mode. When in direct mode, base address will be set
    /// to the `_start_trap` address.
    /// When in vectored mode, the address of the vector table is passed
    /// as a usize.
    /// This isn't const, as rust doesn't allow an address only know at
    /// link time to be const.
    /// https://users.rust-lang.org/t/using-linker-defined-symbols-in-const-context-as-integers/120081
    fn get_exception_mode() -> ExceptionMode;
}
