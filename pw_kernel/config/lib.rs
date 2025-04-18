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
}

/// Cortex-M specific configuration.
// TODO: davidroth - Once Arch is out of tree, move this configuration also.
pub trait CortexMKernelConfigInterface {
    /// Rate of the Cortex-M systick system timer.
    const SYS_TICK_HZ: u32;
}

/// RISC-V specific configuration.
// TODO: davidroth - Once Arch is out of tree, move this configuration also.
pub trait RiscVKernelConfigInterface {}
